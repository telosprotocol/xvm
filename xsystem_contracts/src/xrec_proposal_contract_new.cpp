// Copyright (c) 2017-present Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/tcc/xrec_proposal_contract_new.h"

#include "xbase/xbase.h"
#include "xbase/xutl.h"
#include "xchain_upgrade/xchain_data_processor.h"
#include "xconfig/xconfig_register.h"
#include "xconfig/xconfig_update_parameter_action.h"
#include "xdata/xblock.h"
#include "xdata/xelect_transaction.hpp"
#include "xmbus/xevent_store.h"
#include "xmetrics/xmetrics.h"
#include "xverifier/xverifier_errors.h"
#include "xverifier/xverifier_utl.h"
#include "xstate_accessor/xtoken.h"
#include "xvm/xsystem_contracts/xerror/xerror.h"

#include <algorithm>
#include <cinttypes>
#include <ctime>

using namespace top::data;

NS_BEG2(top, system_contracts)

void xtop_rec_tcc_contract::setup() {
    xtcc_transaction_ptr_t tcc_genesis = std::make_shared<xtcc_transaction_t>();

    for (const auto & entry : tcc_genesis->m_initial_values) {
        m_tcc_parameters.set(entry.first, entry.second);
    }
    m_tcc_next_unused_proposal_id.set("0");

    top::chain_data::data_processor_t data;
    top::chain_data::xtop_chain_data_processor::get_contract_data(common::xlegacy_account_address_t{address()}, data);
    deposit(state_accessor::xtoken_t{static_cast<uint64_t>(data.top_balance)});
}

bool xtop_rec_tcc_contract::get_proposal_info(const std::string & proposal_id, tcc::proposal_info & proposal) {
    std::string value;
    try {
        value = m_tcc_proposal_ids.get(proposal_id);
    } catch (top::error::xtop_error_t const &) {
        xdbg("[xtop_rec_tcc_contract::get_proposal_info] can't find proposal for id: %s", proposal_id.c_str());
        return false;  // not exist
    }

    top::base::xstream_t stream{base::xcontext_t::instance(), (uint8_t*)value.data(), static_cast<uint32_t>(value.size())};
    proposal.deserialize(stream);
    return true;
}

void xtop_rec_tcc_contract::charge(std::string const & token_name, uint64_t token_amount) {
    // withdraw from src action(account) state
    auto token = withdraw(token_amount);
    xdbg("[xtop_rec_tcc_contract::charge] at_source_action_stage, token name: %s, amount: %" PRIu64, token_name.c_str(), token_amount);

    // serialize token to target action(account)
    asset_to_next_action(std::move(token));
}

void xtop_rec_tcc_contract::submitProposal(const std::string & target,
                                           const std::string & value,
                                           tcc::proposal_type type,
                                           uint64_t effective_timer_height) {
    XMETRICS_TIME_RECORD("sysContract_recTccProposal_submit_proposal");
    XMETRICS_CPU_TIME_RECORD("sysContract_recTccProposal_submit_proposal_cpu");

    // XCONTRACT_ENSURE(modification_description.size() <= MODIFICATION_DESCRIPTION_SIZE, "[xtop_rec_tcc_contract::submitProposal] description size too big!");
    XCONTRACT_ENSURE(is_valid_proposal_type(type) == true, "[xtop_rec_tcc_contract::submitProposal] input invalid proposal type!");

    auto const & src_account = sender();
    xdbg("[xtop_rec_tcc_contract::submitProposal] account: %s, target: %s, value: %s, type: %d, effective_timer_height: %" PRIu64,
         src_account.c_str(), target.c_str(), value.c_str(), type, effective_timer_height);


    tcc::proposal_info proposal;
    switch (type)
    {
    case tcc::proposal_type::proposal_update_parameter:
        {
            XCONTRACT_ENSURE(m_tcc_parameters.exist(target), "[xtop_rec_tcc_contract::submitProposal] proposal_add_parameter target do not exist");
            auto config_value = m_tcc_parameters.get(target);
            if (config_value.empty()) {
                xwarn("[xtop_rec_tcc_contract::submitProposal] parameter: %s, not found", target.c_str());
                top::error::throw_error(error::xerrc_t::proposal_not_found);
            }

            if (config_value == value) {
                xwarn("[xtop_rec_tcc_contract::submitProposal] parameter: %s, provide onchain value: %s, new value: %s", target.c_str(), config_value.c_str(), value.c_str());
                top::error::throw_error(error::xerrc_t::proposal_not_changed);
            }
        }
        break;

    case tcc::proposal_type::proposal_update_asset:
        XCONTRACT_ENSURE(xverifier::xtx_utl::address_is_valid(target) == xverifier::xverifier_error::xverifier_success, "[xtop_rec_tcc_contract::submitProposal] proposal_update_asset type proposal, target invalid!");
        break;
    case tcc::proposal_type::proposal_add_parameter:
        XCONTRACT_ENSURE(!m_tcc_parameters.exist(target), "[xtop_rec_tcc_contract::submitProposal] proposal_add_parameter target already exist");
        break;
    case tcc::proposal_type::proposal_delete_parameter:
        XCONTRACT_ENSURE(m_tcc_parameters.exist(target), "[xtop_rec_tcc_contract::submitProposal] proposal_add_parameter target do not exist");
        break;
    case tcc::proposal_type::proposal_update_parameter_incremental_add:
    case tcc::proposal_type::proposal_update_parameter_incremental_delete:
        // current only support whitelist
        XCONTRACT_ENSURE(target == "whitelist", "[xtop_rec_tcc_contract::submitProposal] current target cannot support proposal_update_parameter_increamental_add/delete");
        check_bwlist_proposal(value);
        break;
    default:
        assert(false);
        xwarn("[xtop_rec_tcc_contract::submitProposal] proposal type %u current not support", type);
        top::error::throw_error(error::xerrc_t::unknown_proposal_type);
        break;
    }

    std::error_code ec;
    auto token = last_action_asset(ec);
    auto token_amount = token.amount();
    asset_to_next_action(std::move(token));
    assert(!ec);
    top::error::throw_error(ec);

    xdbg("[xtop_rec_tcc_contract::submitProposal] the sender transaction token: %s, amount: %" PRIu64, token.symbol().c_str(), token_amount);
    auto min_tcc_proposal_deposit = XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_tcc_proposal_deposit);
    XCONTRACT_ENSURE(token.amount() >= min_tcc_proposal_deposit * TOP_UNIT, "[xtop_rec_tcc_contract::submitProposal] deposit less than minimum proposal deposit!");

    uint32_t proposal_expire_time = XGET_ONCHAIN_GOVERNANCE_PARAMETER(tcc_proposal_expire_time);
    std::string expire_time = m_tcc_parameters.get("tcc_proposal_expire_time");
    if (expire_time.empty()) {
        xwarn("[xtop_rec_tcc_contract::submitProposal] parameter tcc_proposal_expire_time not found in blockchain");
    } else {
        try {
            proposal_expire_time = std::stoi(expire_time);
        } catch (std::exception & e) {
            xwarn("[xtop_rec_tcc_contract::submitProposal] parameter tcc_proposal_expire_time in tcc chain: %s", expire_time.c_str());
        }
    }

    // set proposal info
    std::string system_id = m_tcc_next_unused_proposal_id.value();

    proposal.proposal_id = base::xstring_utl::tostring(base::xstring_utl::touint64(system_id) + 1);
    proposal.parameter = target;
    proposal.new_value = value;
    proposal.deposit = token_amount;
    proposal.type = type;
    // proposal.modification_description = modification_description;
    proposal.effective_timer_height = effective_timer_height;
    proposal.proposal_client_address = src_account.value();
    proposal.end_time = time() + proposal_expire_time;
    proposal.priority = tcc::priority_critical;


    // first phase no cosigning process
    proposal.cosigning_status = tcc::cosigning_success;
    proposal.voting_status = tcc::status_none;

    top::base::xstream_t newstream(base::xcontext_t::instance());
    proposal.serialize(newstream);
    std::string proposal_value((char *)newstream.data(), newstream.size());

    m_tcc_proposal_ids.set(proposal.proposal_id, proposal_value);
    m_tcc_next_unused_proposal_id.set(proposal.proposal_id);
    xinfo("[xtop_rec_tcc_contract::submitProposal] timer round: %" PRIu64 ", added new proposal: %s, stream detail size: %zu", time(), proposal.proposal_id.c_str(), value.size());
    XMETRICS_PACKET_INFO("sysContract_recTccProposal_submit_proposal", "timer round", std::to_string(time()), "proposal id", proposal.proposal_id);
    delete_expired_proposal();
}

void xtop_rec_tcc_contract::withdrawProposal(const std::string & proposal_id) {
    XMETRICS_TIME_RECORD("sysContract_recTccProposal_withdraw_proposal");
    XMETRICS_CPU_TIME_RECORD("sysContract_recTccProposal_withdraw_proposal_cpu");
    auto src_account = sender();
    xdbg("[xtop_rec_tcc_contract::withdrawProposal] proposal_id: %s, account: %s", proposal_id.c_str(), src_account.c_str());

    tcc::proposal_info proposal;
    if (!get_proposal_info(proposal_id, proposal)) {
        xdbg("[xtop_rec_tcc_contract::withdrawProposal] can't find proposal: %s", proposal_id.c_str());
        top::error::throw_error(error::xerrc_t::proposal_not_found);
    }
    XCONTRACT_ENSURE(src_account.value() == proposal.proposal_client_address, "[xtop_rec_tcc_contract::withdrawProposal] only proposer can cancel the proposal!");

    // if (proposal.voting_status != status_none) {
    //     // cosigning in phase one is done automatically,
    //     // only check if voting in progress, can't withdraw
    //     xdbg("[PROPOSAL] in withdrawProposal proposal: %s, voting status: %u", proposal_id.c_str(), proposal.voting_status);
    //     return;
    // }
    if (m_tcc_vote_ids.exist(proposal_id)) {
        m_tcc_vote_ids.remove(proposal_id);
    }
    m_tcc_proposal_ids.remove(proposal_id);

    xdbg("[xtop_rec_tcc_contract::withdrawProposal] transfer deposit back, proposal id: %s, account: %s, deposit: %" PRIu64, proposal_id.c_str(), src_account.c_str(), proposal.deposit);
    transfer(common::xaccount_address_t{proposal.proposal_client_address}, proposal.deposit, contract_common::xfollowup_transaction_schedule_type_t::immediately);
    delete_expired_proposal();

}

void xtop_rec_tcc_contract::tccVote(std::string const & proposal_id, bool option) {
    XMETRICS_TIME_RECORD("sysContract_recTccProposal_tcc_vote");
    XMETRICS_CPU_TIME_RECORD("sysContract_recTccProposal_tcc_vote_cpu");
    auto const & src_account = sender();
    xdbg("[xtop_rec_tcc_contract::tccVote] tccVote start, proposal_id: %s, account: %s vote: %d", proposal_id.c_str(), src_account.c_str(), option);

    // check if the voting client address exists in initial comittee
    if (!voter_in_committee(src_account.value())) {
        xwarn("[xtop_rec_tcc_contract::tccVote] source addr is not a commitee voter: %s", src_account.c_str());
        top::error::throw_error(error::xerrc_t::invalid_proposer);
    }

    tcc::proposal_info proposal;
    if (!get_proposal_info(proposal_id, proposal)) {
        xwarn("[xtop_rec_tcc_contract::tccVote] can't find proposal: %s", proposal_id.c_str());
        top::error::throw_error(error::xerrc_t::proposal_not_found);
    }

    if (proposal.voting_status == tcc::status_none) {
        proposal.voting_status = tcc::voting_in_progress;
    } else if (proposal.voting_status == tcc::voting_failure || proposal.voting_status == tcc::voting_success) {
        xdbg("[xtop_rec_tcc_contract::tccVote] proposal: %s already voted done, status: %d", proposal_id.c_str(), proposal.voting_status);
        return;
    }


    auto committee_list = XGET_ONCHAIN_GOVERNANCE_PARAMETER(tcc_member);
    std::vector<std::string> vec_committee;
    uint32_t voter_committee_size = base::xstring_utl::split_string(committee_list, ',', vec_committee);

    std::map<std::string, bool> voting_result;
    auto value_string = m_tcc_vote_ids.get(proposal_id);
    if (!value_string.empty()) {
        top::base::xstream_t stream{base::xcontext_t::instance(), (uint8_t*)value_string.data(), static_cast<uint32_t>(value_string.size())};
        MAP_DESERIALIZE_SIMPLE(stream, voting_result);
    }

    if (proposal_expired(proposal_id)) {
        uint32_t yes_voters = 0;
        uint32_t no_voters = 0;
        uint32_t not_yet_voters = 0;
        for (const auto & entry : voting_result) {
            if (entry.second) {
                ++yes_voters;
            } else {
                ++no_voters;
            }
        }
        not_yet_voters = voter_committee_size - yes_voters - no_voters;

        if (proposal.priority == tcc::priority_critical) {
            if (((yes_voters * 1.0 / voter_committee_size) >= (2.0 / 3)) && ((no_voters * 1.0 / voter_committee_size) < 0.20)) {
                proposal.voting_status = tcc::voting_success;
            } else {
                proposal.voting_status = tcc::voting_failure;
            }
        } else if (proposal.priority == tcc::priority_important) {
            if ((yes_voters * 1.0 / voter_committee_size) >= 0.51 && (not_yet_voters * 1.0 / voter_committee_size < 0.25)) {
                proposal.voting_status = tcc::voting_success;
            } else {
                proposal.voting_status = tcc::voting_failure;
            }
        } else {
            // normal priority
            if ((yes_voters * 1.0 / voter_committee_size) >= 0.51) {
                proposal.voting_status = tcc::voting_success;
            } else {
                proposal.voting_status = tcc::voting_failure;
            }
        }
        xdbg("[xtop_rec_tcc_contract::tccVote] proposal: %s has expired, priority: %" PRIu8 ", yes voters: %u, no voters: %u, not yet voters: %u, voting status: %d",
             proposal_id.c_str(),
             proposal.priority,
             yes_voters,
             no_voters,
             not_yet_voters,
             proposal.voting_status);

    } else {
        auto it = voting_result.find(src_account.value());
        if (it != voting_result.end()) {
            xinfo("[xtop_rec_tcc_contract::tccVote] client addr(%s) already voted", src_account.c_str());
            top::error::throw_error(error::xerrc_t::proposal_already_voted);
        }
        // record the voting for this client address
        voting_result.insert({src_account.value(), option});
        uint32_t yes_voters = 0;
        uint32_t no_voters = 0;
        uint32_t not_yet_voters = 0;
        for (const auto & entry : voting_result) {
            if (entry.second) {
                ++yes_voters;
            } else {
                ++no_voters;
            }
        }
        not_yet_voters = voter_committee_size - yes_voters - no_voters;

        if (proposal.priority == tcc::priority_critical) {
            if (((yes_voters * 1.0 / voter_committee_size) >= (2.0 / 3)) && ((no_voters * 1.0 / voter_committee_size) < 0.20) && (not_yet_voters == 0)) {
                proposal.voting_status = tcc::voting_success;
            } else if ((no_voters * 1.0 / voter_committee_size) >= 0.20) {
                proposal.voting_status = tcc::voting_failure;
            }
        } else if (proposal.priority == tcc::priority_important) {
            if ((yes_voters * 1.0 / voter_committee_size) >= 0.51 && (not_yet_voters * 1.0 / voter_committee_size < 0.25)) {
                proposal.voting_status = tcc::voting_success;
            } else if ((no_voters * 1.0 / voter_committee_size) >= 0.51) {
                proposal.voting_status = tcc::voting_failure;
            }
        } else {
            // normal priority
            if ((yes_voters * 1.0 / voter_committee_size) >= 0.51) {
                proposal.voting_status = tcc::voting_success;
            } else if ((no_voters * 1.0 / voter_committee_size) >= 0.51) {
                proposal.voting_status = tcc::voting_failure;
            }
        }
        xdbg("[xtop_rec_tcc_contract::tccVote] proposal: %s has NOT expired, priority: %" PRIu8 ", yes voters: %u, no voters: %u, not yet voters: %u, voting status: %d",
             proposal_id.c_str(),
             proposal.priority,
             yes_voters,
             no_voters,
             not_yet_voters,
             proposal.voting_status);
    }

    if (proposal.voting_status == tcc::voting_failure || proposal.voting_status == tcc::voting_success) {
        xdbg("[xtop_rec_tcc_contract::tccVote] proposal: %s, status: %d, transfer (%lu) deposit to client: %s",
             proposal_id.c_str(),
             proposal.voting_status,
             proposal.deposit,
             proposal.proposal_client_address.c_str());
        if (m_tcc_vote_ids.exist(proposal_id)) {
            m_tcc_vote_ids.remove(proposal_id);
        }
        m_tcc_proposal_ids.remove(proposal_id);
        transfer(common::xaccount_address_t{proposal.proposal_client_address}, proposal.deposit, contract_common::xfollowup_transaction_schedule_type_t::immediately);

        if (proposal.voting_status == tcc::voting_success) {
            // save the voting status
            top::base::xstream_t stream(base::xcontext_t::instance());
            stream.reset();
            proposal.serialize(stream);
            std::string voted_proposal((char *)stream.data(), stream.size());
            std::string new_whitelist{""};
            switch (proposal.type)
            {
            case tcc::proposal_type::proposal_update_parameter:
                // notify config center to load the changed parameter
                m_tcc_voted_proposal.set(voted_proposal);
                m_tcc_parameters.set(proposal.parameter, proposal.new_value);
                break;

            case tcc::proposal_type::proposal_update_asset:
                transfer(common::xaccount_address_t{proposal.parameter}, base::xstring_utl::touint64(proposal.new_value), contract_common::xfollowup_transaction_schedule_type_t::immediately);
                break;
            case tcc::proposal_type::proposal_add_parameter:
                m_tcc_parameters.set(proposal.parameter, proposal.new_value);
                m_tcc_voted_proposal.set(voted_proposal);
                break;
            case tcc::proposal_type::proposal_delete_parameter:
                m_tcc_parameters.remove(proposal.parameter);
                m_tcc_voted_proposal.set(voted_proposal);
                break;
            case tcc::proposal_type::proposal_update_parameter_incremental_add:
                new_whitelist = top::config::xconfig_utl::incremental_add_bwlist(XGET_ONCHAIN_GOVERNANCE_PARAMETER(whitelist), proposal.new_value);
                m_tcc_parameters.set(proposal.parameter, new_whitelist);
                m_tcc_voted_proposal.set(voted_proposal);
                break;
            case tcc::proposal_type::proposal_update_parameter_incremental_delete:
                new_whitelist = top::config::xconfig_utl::incremental_delete_bwlist(XGET_ONCHAIN_GOVERNANCE_PARAMETER(whitelist), proposal.new_value);
                m_tcc_parameters.set(proposal.parameter, new_whitelist);
                m_tcc_voted_proposal.set(voted_proposal);
                break;

            default:
                assert(false);
                xwarn("[xtop_rec_tcc_contract::tccVote] proposal type %u current not support", proposal.type);
                top::error::throw_error(error::xerrc_t::unknown_proposal_type);
                break;
            }

        }
    } else if (proposal.voting_status == tcc::voting_in_progress) {
        top::base::xstream_t stream(base::xcontext_t::instance());
        MAP_SERIALIZE_SIMPLE(stream, voting_result);
        m_tcc_vote_ids.set(proposal_id, std::string((char *)stream.data(), stream.size()));

        stream.reset();
        proposal.serialize(stream);
        std::string voted_proposal((char *)stream.data(), stream.size());
        m_tcc_proposal_ids.set(proposal_id, voted_proposal);
    }

    delete_expired_proposal();
}

bool xtop_rec_tcc_contract::proposal_expired(const std::string & proposal_id) {
    tcc::proposal_info proposal{};
    if (!get_proposal_info(proposal_id, proposal)) {
        return false;
    }

    return proposal.end_time < time();
}

bool xtop_rec_tcc_contract::is_valid_proposal_type(tcc::proposal_type type) {
    switch ( type)
    {
    case tcc::proposal_type::proposal_update_parameter:
    case tcc::proposal_type::proposal_update_asset:
    case tcc::proposal_type::proposal_add_parameter:
    case tcc::proposal_type::proposal_delete_parameter:
    case tcc::proposal_type::proposal_update_parameter_incremental_add:
    case tcc::proposal_type::proposal_update_parameter_incremental_delete:
        return true;

    default:
        return false;
    }
}

void xtop_rec_tcc_contract::check_bwlist_proposal(std::string const& bwlist) {
    std::vector<std::string> vec_member;
    uint32_t size = base::xstring_utl::split_string(bwlist, ',', vec_member);
    XCONTRACT_ENSURE(size > 0, "[xtop_rec_tcc_contract::check_bwlist_proposal] target value error, size zero");
    for (auto const& v: vec_member) {
        XCONTRACT_ENSURE(top::xverifier::xverifier_success == top::xverifier::xtx_utl::address_is_valid(v), "[xtop_rec_tcc_contract::check_bwlist_proposal]  target value error, addr invalid");
    }

    std::sort(vec_member.begin(), vec_member.end());
    vec_member.erase(std::unique(vec_member.begin(), vec_member.end()), vec_member.end());
    XCONTRACT_ENSURE(vec_member.size() == size, "[xtop_rec_tcc_contract::check_bwlist_proposal]  target value error, addr duplicated");
}

bool xtop_rec_tcc_contract::voter_in_committee(const std::string & voter_client_addr) {
    auto committee_list = XGET_ONCHAIN_GOVERNANCE_PARAMETER(tcc_member);
    std::vector<std::string> vec_committee;
    base::xstring_utl::split_string(committee_list, ',', vec_committee);

    for (auto const& addr: vec_committee) {
        if (addr == voter_client_addr) return true;
    }

    return false;
}

void xtop_rec_tcc_contract::delete_expired_proposal() {

    auto proposals = m_tcc_proposal_ids.value();

    for (auto const& item: proposals) {
        tcc::proposal_info proposal{};
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t*)item.second.data(), item.second.size());
        proposal.deserialize(stream);

        if (proposal.end_time < time()) { //expired
            transfer(common::xaccount_address_t{proposal.proposal_client_address}, proposal.deposit, contract_common::xfollowup_transaction_schedule_type_t::immediately);
            m_tcc_proposal_ids.remove(proposal.proposal_id);

            if (m_tcc_vote_ids.exist(proposal.proposal_id)) {
                m_tcc_vote_ids.remove(proposal.proposal_id);
            }

            xkinfo("[xtop_rec_tcc_contract::delete_expired_proposal] delete proposal id: %s", proposal.proposal_id.c_str());
        }
    }
}

NS_END2

// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xreward/xtable_vote_contract.h"
#include "xchain_upgrade/xchain_upgrade_center.h"

#include "xbase/xutl.h"
#include "xbasic/xutility.h"
#include "xcommon/xrole_type.h"
#include "xdata/xgenesis_data.h"
#include "xmetrics/xmetrics.h"
#include "xstore/xstore_error.h"
#include "xutility/xhash.h"

using top::base::xcontext_t;
using top::base::xstream_t;
using top::base::xstring_utl;
using namespace top::data;

NS_BEG2(top, xstake)

#define TIMER_ADJUST_DENOMINATOR 10

xtable_vote_contract::xtable_vote_contract(common::xnetwork_id_t const & network_id) : xbase_t{network_id} {}

void xtable_vote_contract::setup() {
    // vote related
    for (auto i = 1; i <= xstake::XPROPERTY_SPLITED_NUM; i++) {
        std::string property;
        property = property + XPORPERTY_CONTRACT_VOTES_KEY_BASE + "-" + std::to_string(i);
        MAP_CREATE(property);
    }

    MAP_CREATE(XPORPERTY_CONTRACT_POLLABLE_KEY);
    STRING_CREATE(XPORPERTY_CONTRACT_TIME_KEY);
}

// vote related
void xtable_vote_contract::voteNode(vote_info_map_t const & vote_info) {
    XMETRICS_TIME_RECORD("sysContract_tableVote_vote_node");

    auto const & account = SOURCE_ADDRESS();
    xinfo("[xtable_vote_contract::voteNode] timer round: %" PRIu64 ", src_addr: %s, self addr: %s, pid: %d\n", TIME(), account.c_str(), SELF_ADDRESS().c_str(), getpid());

    const xtransaction_ptr_t trans_ptr = GET_TRANSACTION();
    XCONTRACT_ENSURE(trans_ptr->get_tx_type() == xtransaction_type_vote && trans_ptr->get_source_action().get_action_type() == xaction_type_source_null,
                     "xtable_vote_contract::voteNode: source_action type must be xaction_type_source_null and transaction_type must be xtransaction_type_vote");
    XMETRICS_PACKET_INFO("sysContract_tableVote_vote_node", "timer round", std::to_string(TIME()), "voter address", account);

    set_vote_info(account, vote_info, true);
}

void xtable_vote_contract::unvoteNode(vote_info_map_t const & vote_info) {
    XMETRICS_TIME_RECORD("sysContract_tableVote_unvote_node");

    auto const & account = SOURCE_ADDRESS();
    xinfo("[xtable_vote_contract::unvoteNode] timer round: %" PRIu64 ", src_addr: %s, self addr: %s, pid: %d\n", TIME(), account.c_str(), SELF_ADDRESS().c_str(), getpid());

    const xtransaction_ptr_t trans_ptr = GET_TRANSACTION();
    XCONTRACT_ENSURE(trans_ptr->get_tx_type() == xtransaction_type_abolish_vote && trans_ptr->get_source_action().get_action_type() == xaction_type_source_null,
                     "xtable_vote_contract::unvoteNode: source_action type must be xaction_type_source_null and transaction_type must be xtransaction_type_abolish_vote");
    XMETRICS_PACKET_INFO("sysContract_tableVote_unvote_node", "timer round", std::to_string(TIME()), "unvoter address", account);

    set_vote_info(account, vote_info, false);
}

void xtable_vote_contract::set_vote_info(std::string const & account, vote_info_map_t const & vote_info, bool b_vote) {
    if (!add_adv_vote(account, vote_info, b_vote)) {
        return;
    }

    auto onchain_timer_round = TIME();
    if (!is_expire(onchain_timer_round)) {
        xdbg("[xtable_vote_contract::set_vote_info]  is not expire pid: %d, b_vote: %d\n", getpid(), b_vote);
        return;
    }
    commit_stake();
    commit_total_votes_num();
}

void xtable_vote_contract::commit_stake() {
    std::map<std::string, std::string> adv_votes;

    try {
        XMETRICS_TIME_RECORD("sysContract_tableVote_get_property_contract_pollable_key");
        MAP_COPY_GET(XPORPERTY_CONTRACT_POLLABLE_KEY, adv_votes);
    } catch (std::runtime_error & e) {
        xdbg("[update_adv_votes] MAP COPY GET error:%s", e.what());
        throw;
    }

    chain_upgrade::xtop_chain_fork_config_center fork_config_center;
    auto fork_config = fork_config_center.chain_fork_config();
    uint64_t timer = TIME();
    if (chain_upgrade::xtop_chain_fork_config_center::is_forked(fork_config.vote_contract_trx_split, timer)) {
        xinfo("[xtable_vote_contract::commit_stake] split table vote trx %" PRIu64, timer);
        split_and_report(sys_contract_rec_registration_addr, "update_batch_stake_v2", adv_votes);

    } else {
        base::xstream_t stream(base::xcontext_t::instance());
        stream << adv_votes;
        CALL(common::xaccount_address_t{sys_contract_rec_registration_addr}, "update_batch_stake", std::string((char *)stream.data(), stream.size()));
    }
}

int32_t xtable_vote_contract::get_node_info(const std::string & account, xreg_node_info & reg_node_info) {
    xdbg("[xtable_vote_contract::get_node_info] node account: %s, pid: %d\n", account.c_str(), getpid());

    std::string reg_node_str;
    int32_t ret = MAP_GET2(XPORPERTY_CONTRACT_REG_KEY, account, reg_node_str, sys_contract_rec_registration_addr);
    if (ret || reg_node_str.empty()) {
        return xaccount_property_not_exist;
    }
    base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)reg_node_str.data(), reg_node_str.size());
    if (stream.size() > 0) {
        reg_node_info.serialize_from(stream);
    }

    return 0;
}

bool xtable_vote_contract::add_adv_vote(std::string const & account, vote_info_map_t const & vote_info, bool b_vote) {
    std::map<std::string, uint64_t> votes_table;
    std::string vote_info_str;
    uint32_t sub_map_no = (utl::xxh32_t::digest(account) % xstake::XPROPERTY_SPLITED_NUM) + 1;
    std::string property = (std::string)XPORPERTY_CONTRACT_VOTES_KEY_BASE + "-" + std::to_string(sub_map_no);
    {
        XMETRICS_TIME_RECORD("sysContract_tableVote_get_property_contract_votes_key");
        int32_t ret = MAP_GET2(property, account, vote_info_str);
        // here if not success, means account has no vote info yet, so vote_info_str is empty, using above default votes_table directly
        if (ret) xwarn("[xtable_vote_contract::add_adv_vote] get property empty, account %s", account.c_str());
        if (!vote_info_str.empty()) {
            base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)vote_info_str.c_str(), (uint32_t)vote_info_str.size());
            stream >> votes_table;
        }
    }


    auto pid = getpid();
    uint64_t old_vote_tickets = 0;
    for (auto const & entity : vote_info) {
        auto const & adv_account = entity.first;
        auto const & votes = entity.second;

        xdbg("[xtable_vote_contract::add_adv_vote] b_vote: %d, voter: %s, node: %s, votes: %u, property: %s, pid: %d\n",
             b_vote,
             account.c_str(),
             adv_account.c_str(),
             votes,
             property.c_str(),
             pid);

        uint64_t node_total_votes = get_advance_tickets(adv_account);
        // add_vote_advance_tickets(account, adv_account, count, b_vote);
        if (!b_vote) {
            auto iter = votes_table.find(adv_account);
            if (iter != votes_table.end()) {
                old_vote_tickets = iter->second;
            }
            XCONTRACT_ENSURE(iter != votes_table.end(), "xtable_vote_contract::add_adv_vote: vote not found");
            XCONTRACT_ENSURE(votes <= old_vote_tickets, "xtable_vote_contract::add_adv_vote: votes not enough");
            if (votes == old_vote_tickets) {
                votes_table.erase(iter);
            } else {
                votes_table[adv_account] -= votes;
            }

            node_total_votes -= votes;
        } else {
            xreg_node_info node_info;
            auto ret = get_node_info(adv_account, node_info);
            XCONTRACT_ENSURE(ret == 0, "xtable_vote_contract::add_adv_vote: node not exist");
            XCONTRACT_ENSURE(node_info.is_auditor_node() == true, "xtable_vote_contract::add_adv_vote: only auditor can be voted");

            auto min_votes_num = XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_votes_num);
            XCONTRACT_ENSURE(votes >= min_votes_num, "xtable_vote_contract::add_adv_vote: lower than lowest votes");

            votes_table[adv_account] += votes;

            auto max_vote_nodes_num = XGET_ONCHAIN_GOVERNANCE_PARAMETER(max_vote_nodes_num);
            XCONTRACT_ENSURE(votes_table.size() <= max_vote_nodes_num, "xtable_vote_contract::add_adv_vote: beyond the maximum nodes that can be voted by you");
            node_total_votes += votes;
        }

        add_advance_tickets(adv_account, node_total_votes);
    }

    if (votes_table.size() == 0) {
        MAP_REMOVE(property, account);
    } else {
        xstream_t stream(xcontext_t::instance());
        stream << votes_table;
        vote_info_str = std::string((char *)stream.data(), stream.size());
        {
            XMETRICS_TIME_RECORD("sysContract_tableVote_set_property_contract_voter_key");
            MAP_SET(property, account, vote_info_str);
        }
    }

    return true;
}

void xtable_vote_contract::add_advance_tickets(std::string const & advance_account, uint64_t tickets) {
    xdbg("[xtable_vote_contract::add_advance_tickets] adv account: %s, tickets: %llu, pid:%d\n", advance_account.c_str(), tickets, getpid());

    if (tickets == 0) {
        XMETRICS_TIME_RECORD("sysContract_tableVote_remove_property_contract_pollable_key");
        REMOVE(enum_type_t::map, XPORPERTY_CONTRACT_POLLABLE_KEY, advance_account);
    } else {
        XMETRICS_TIME_RECORD("sysContract_tableVote_set_property_contract_pollable_key");
        WRITE(enum_type_t::map, XPORPERTY_CONTRACT_POLLABLE_KEY, base::xstring_utl::tostring(tickets), advance_account);
    }
}

uint64_t xtable_vote_contract::get_advance_tickets(std::string const & advance_account) {
    std::string value_str;

    {
        XMETRICS_TIME_RECORD("sysContract_tableVote_get_property_contract_pollable_key");
        int32_t ret = MAP_GET2(XPORPERTY_CONTRACT_POLLABLE_KEY, advance_account, value_str);
        if (ret || value_str.empty()) {
            return 0;
        }
    }
    return base::xstring_utl::touint64(value_str);
}

bool xtable_vote_contract::is_expire(const uint64_t onchain_timer_round) {
    auto timer_interval = XGET_ONCHAIN_GOVERNANCE_PARAMETER(votes_report_interval);
    uint64_t new_time_height = onchain_timer_round;
    uint64_t old_time_height = 0;

    std::string time_height_str = STRING_GET(XPORPERTY_CONTRACT_TIME_KEY);
    if (!time_height_str.empty()) {
        old_time_height = xstring_utl::touint64(time_height_str);
    }

    xdbg("[xtable_vote_contract::is_expire] new_time_height: %llu, old_time_height: %lld, timer_interval: %d, pid: %d\n",
         new_time_height,
         old_time_height,
         timer_interval,
         getpid());

    if (new_time_height - old_time_height <= timer_interval) {
        return false;
    }

    STRING_SET(XPORPERTY_CONTRACT_TIME_KEY, xstring_utl::tostring(new_time_height));
    return true;
}

void xtable_vote_contract::commit_total_votes_num() {
    std::map<std::string, std::string> pollables;
    try {
        XMETRICS_TIME_RECORD("sysContract_tableVote_get_property_contract_pollable_key");
        MAP_COPY_GET(XPORPERTY_CONTRACT_POLLABLE_KEY, pollables);
    } catch (std::runtime_error & e) {
        xdbg("[xtable_vote_contract::commit_total_votes_num] MAP COPY GET error:%s", e.what());
        throw;
    }

    chain_upgrade::xtop_chain_fork_config_center fork_config_center;
    auto fork_config = fork_config_center.chain_fork_config();
    uint64_t timer = TIME();
    if (chain_upgrade::xtop_chain_fork_config_center::is_forked(fork_config.vote_contract_trx_split, timer)) {
        xinfo("[xtable_vote_contract::commit_total_votes_num] split table vote trx %" PRIu64, timer);
        split_and_report(sys_contract_zec_vote_addr, "on_receive_shard_votes_v2", pollables);

    } else {
        base::xstream_t stream(base::xcontext_t::instance());
        stream << pollables;
        CALL(common::xaccount_address_t{sys_contract_zec_vote_addr}, "on_receive_shard_votes", std::string((char *)stream.data(), stream.size()));
    }

}

void xtable_vote_contract::split_and_report(std::string const& report_contract, std::string const& report_func, std::map<std::string, std::string> const& report_content) {
    auto timer = TIME();
    if (report_content.size() == 0) {
        xinfo("[xtable_vote_contract::split_and_report] the report content size zero");
        return;
    }


    if (report_content.size() <= XVOTE_TRX_LIMIT) {
        base::xstream_t  call_stream(base::xcontext_t::instance());
        call_stream << timer;
        call_stream << report_content;
        xinfo("[xtable_vote_contract::split_and_report] the report content size %u", report_content.size());
        CALL(common::xaccount_address_t{report_contract}, report_func, std::string((char *)call_stream.data(), call_stream.size()));

    } else {

        uint16_t count = 0;
        std::map<std::string, std::string> split_report_content;
        for (auto const& item: report_content) {
            count++;
            split_report_content[item.first] = item.second;
            if (count % XVOTE_TRX_LIMIT == 0) {
                base::xstream_t call_stream(base::xcontext_t::instance());
                call_stream << timer;
                call_stream << split_report_content;
                xinfo("[xtable_vote_contract::split_and_report] the report content size %u, count %u", report_content.size(), count);
                CALL(common::xaccount_address_t{report_contract}, report_func, std::string((char *)call_stream.data(), call_stream.size()));
                count = 0;
                split_report_content.clear();
            }

        }

        if (!split_report_content.empty()) {
            base::xstream_t call_stream(base::xcontext_t::instance());
            call_stream << timer;
            call_stream << split_report_content;
            xinfo("[xtable_vote_contract::split_and_report] the report content size %u, count %u", report_content.size(), split_report_content.size());
            CALL(common::xaccount_address_t{report_contract}, report_func, std::string((char *)call_stream.data(), call_stream.size()));
        }

    }
}

NS_END2

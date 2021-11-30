// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xregistration/xrec_registration_contract_new.h"

#include "xchain_upgrade/xchain_data_processor.h"
#include "xdata/xnative_contract_address.h"
#include "xdata/xrootblock.h"
#include "xdata/xslash.h"
#include "xstore/xstore_error.h"

using namespace top::base;
using namespace top::chain_data;
using namespace top::data;
using namespace top::store;
using namespace top::xstake;

#if !defined (XREC_MODULE)
#    define XREC_MODULE "SysContract_"
#endif
#define XCONTRACT_PREFIX "Registration_"
#define XREG_CONTRACT XREC_MODULE XCONTRACT_PREFIX

NS_BEG2(top, system_contracts)

#define TIMER_ADJUST_DENOMINATOR 10

void xtop_rec_registration_contract_new::setup() {
    std::vector<std::pair<std::string, std::string>> db_kv_101;
    xchain_data_processor_t::get_stake_map_property(common::xlegacy_account_address_t{address()}, XPORPERTY_CONTRACT_REG_KEY, db_kv_101);
    for (auto const & _p : db_kv_101) {
        auto const & node_info_serialized = _p.second;

        xstream_t stream{ xcontext_t::instance(), (uint8_t *)node_info_serialized.data(), (uint32_t)node_info_serialized.size() };
        xreg_node_info node_info;
        node_info.serialize_from(stream);

        auto old_role = static_cast<common::xold_role_type_t>(node_info.m_registered_role);
        switch (old_role) {
        case common::xold_role_type_t::advance:
            node_info.m_registered_role = common::xrole_type_t::advance;
            break;

        case common::xold_role_type_t::consensus:
            node_info.m_registered_role = common::xrole_type_t::validator;
            break;

        case common::xold_role_type_t::archive:
            node_info.m_registered_role = common::xrole_type_t::archive;
            break;

        case common::xold_role_type_t::edge:
            node_info.m_registered_role = common::xrole_type_t::edge;
            break;

        default:
            break;
        }
        stream.reset();
        node_info.serialize_to(stream);
        m_reg_prop.set(_p.first, {reinterpret_cast<char *>(stream.data()), static_cast<size_t>(stream.size())});
    }

    std::vector<std::pair<std::string, std::string>> db_kv_128;
    xchain_data_processor_t::get_stake_map_property(common::xlegacy_account_address_t{address()}, XPORPERTY_CONTRACT_REFUND_KEY, db_kv_128);
    for (auto const & _p : db_kv_128) {
        m_refund_prop.set(_p.first, _p.second);
    }

    std::vector<std::pair<std::string, std::string>> db_kv_132;
    xchain_data_processor_t::get_stake_map_property(common::xlegacy_account_address_t{address()}, XPROPERTY_CONTRACT_SLASH_INFO_KEY, db_kv_132);
    for (auto const & _p : db_kv_132) {
        m_slash_prop.set(_p.first, _p.second);
    }

    std::string db_kv_129;
    xchain_data_processor_t::get_stake_string_property(common::xlegacy_account_address_t{address()}, XPORPERTY_CONTRACT_GENESIS_STAGE_KEY, db_kv_129);
    if (!db_kv_129.empty()) {
        m_genesis_prop.set(db_kv_129);
    }

    const uint32_t old_tables_count = 256;
    for (auto table = 0; table < enum_vledger_const::enum_vbucket_has_tables_count; table++) {
        std::string table_address{std::string{sys_contract_sharding_vote_addr} + "@" + std::to_string(table)};
        std::map<std::string, uint64_t> adv_get_votes_detail;
        for (auto i = 1; i <= xstake::XPROPERTY_SPLITED_NUM; i++) {
            std::string property;
            property = property + XPORPERTY_CONTRACT_VOTES_KEY_BASE + "-" + std::to_string(i);
            {
                std::map<std::string, std::map<std::string, uint64_t>> votes_detail;
                for (uint32_t j = 0; j < old_tables_count; j++) {
                    auto table_addr = std::string{sys_contract_sharding_vote_addr} + "@" + xstring_utl::tostring(j);
                    std::vector<std::pair<std::string, std::string>> db_kv_112;
                    xchain_data_processor_t::get_stake_map_property(common::xlegacy_account_address_t{table_addr}, property, db_kv_112);
                    for (auto const & _p : db_kv_112) {
                        xvaccount_t vaccount{_p.first};
                        auto account_table_id = vaccount.get_ledger_subaddr();
                        if (static_cast<uint16_t>(account_table_id) != static_cast<uint16_t>(table)) {
                            continue;
                        }
                        std::map<std::string, uint64_t> votes;
                        xstream_t stream(xcontext_t::instance(), (uint8_t *)_p.second.c_str(), (uint32_t)_p.second.size());
                        stream >> votes;
                        for (auto const & vote : votes) {
                            if (votes_detail[_p.first].count(vote.first)) {
                                votes_detail[_p.first][vote.first] += vote.second;
                            } else {
                                votes_detail[_p.first][vote.first] = vote.second;
                            }
                        }
                    }
                }
                for (auto const & vote_detail : votes_detail) {
                    for (auto const & adv_get_votes : vote_detail.second) {
                        if (adv_get_votes_detail.count(adv_get_votes.first)) {
                            adv_get_votes_detail[adv_get_votes.first] += adv_get_votes.second;
                        } else {
                            adv_get_votes_detail[adv_get_votes.first] = adv_get_votes.second;
                        }
                    }
                }
            }
        }
        {
            if(adv_get_votes_detail.empty()) {
                continue;
            }
            std::map<std::string, std::string> adv_get_votes_str_detail;
            for (auto const & adv_get_votes : adv_get_votes_detail) {
                adv_get_votes_str_detail.insert(std::make_pair(adv_get_votes.first, base::xstring_utl::tostring(adv_get_votes.second)));
            }
            xstream_t stream(xcontext_t::instance());
            stream << adv_get_votes_str_detail;
            std::string adv_get_votes_str{std::string((const char*)stream.data(), stream.size())};
            // MAP_SET(XPORPERTY_CONTRACT_TICKETS_KEY, table_address, adv_get_votes_str);
            m_tickets_prop.set(table_address, adv_get_votes_str);
        }
    }

    data_processor_t data;
    xtop_chain_data_processor::get_contract_data(common::xlegacy_account_address_t{address()}, data);
    // TOP_TOKEN_INCREASE(data.top_balance);
    deposit(state_accessor::xtoken_t{static_cast<uint64_t>(data.top_balance)});

    // auto const & source_address = sender().to_string();
    // MAP_CREATE(XPORPERTY_CONTRACT_VOTE_REPORT_TIME_KEY);
    // MAP_SET(XPORPERTY_CONTRACT_VOTE_REPORT_TIME_KEY, source_address, base::xstring_utl::tostring(0));
    m_votes_report_time_prop.set(sender().to_string(), xstring_utl::tostring(0));

#ifdef MAINNET_ACTIVATED
    xactivation_record record;
    record.activated = 1;
    record.activation_time = 1;

    base::xstream_t stream(base::xcontext_t::instance());
    record.serialize_to(stream);
    // auto value_str = ;
    // STRING_SET(XPORPERTY_CONTRACT_GENESIS_STAGE_KEY, value_str);
    m_genesis_prop.set(std::string((char *)stream.data(), stream.size()));
#endif

    xdbg("[xrec_registration_contract_new_t::setup] pid:%d", getpid());
    xreg_node_info node_info;
    {
        common::xnetwork_id_t network_id{top::config::to_chainid(XGET_CONFIG(chain_name))};
        std::vector<node_info_t> const & seed_nodes = xrootblock_t::get_seed_nodes();
        for (size_t i = 0; i < seed_nodes.size(); i++) {
            node_info_t node_data = seed_nodes[i];

            if (m_reg_prop.exist(node_data.m_account.value())) {
                continue;
            }
            node_info.m_account             = node_data.m_account;
            node_info.m_account_mortgage    = 0;
            node_info.m_genesis_node        = true;
            node_info.m_registered_role     = common::xrole_type_t::edge | common::xrole_type_t::advance | common::xrole_type_t::validator;
            node_info.m_network_ids.insert(network_id);
            node_info.nickname              = std::string("bootnode") + std::to_string(i + 1);
            node_info.consensus_public_key  = xpublic_key_t{node_data.m_publickey};
            //xdbg("[xrec_registration_contract_new_t::setup] pid:%d,node account: %s, public key: %s\n", getpid(), node_data.m_account.c_str(), node_data.m_publickey.c_str());
            update_node_info(node_info);
        }
    }
}

void xrec_registration_contract_new_t::mortgage(std::string const & token_name, uint64_t token_amount) {
    // withdraw from src action(account) state
    auto token = withdraw(token_amount);
    xdbg("[xrec_registration_contract_new_t::mortgage] at_source_action_stage, token name: %s, amount: %" PRIu64, token_name.c_str(), token_amount);

    // serialize token to target action(account)
    asset_to_next_action(std::move(token));
}

void xrec_registration_contract_new_t::confirm_deposit(std::string const& token_name, uint64_t token_amount) {
    // target exec fail deposit back to src action(account)
    std::error_code ec;
    auto token = last_action_asset(ec);
    assert(!ec);
    top::error::throw_error(ec);
    xdbg("[xrec_registration_contract_new_t::confirm_deposit] at_confirm_action_stage, token name: %s, amount: %" PRIu64, token.symbol().c_str(), token.amount());
    deposit(std::move(token));
}

void xrec_registration_contract_new_t::registerNode(const std::string & role_type_name,
                                                    const std::string & nickname,
                                                    const std::string & signing_key,
                                                    const uint32_t dividend_rate
#if defined XENABLE_MOCK_ZEC_STAKE
                                                    ,
                                                    common::xaccount_address_t const & registration_account
#endif
) {
    std::set<common::xnetwork_id_t> network_ids;
    common::xnetwork_id_t nid{top::config::to_chainid(XGET_CONFIG(chain_name))};
    network_ids.insert(nid);

#if defined XENABLE_MOCK_ZEC_STAKE
    registerNode2(role_type_name, nickname, signing_key, dividend_rate, network_ids, registration_account);
#else
    registerNode2(role_type_name, nickname, signing_key, dividend_rate, network_ids);
#endif

}

void xtop_rec_registration_contract_new::registerNode2(const std::string & role_type_name,
                                                     const std::string & nickname,
                                                     const std::string & signing_key,
                                                     const uint32_t dividend_rate,
                                                     const std::set<common::xnetwork_id_t> & network_ids
#if defined(XENABLE_MOCK_ZEC_STAKE)
                                                     ,
                                                     common::xaccount_address_t const & registration_account
#endif
) {
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "registerNode_Called", 1);
    XMETRICS_TIME_RECORD(XREG_CONTRACT "registerNode_ExecutionTime");

#if defined(XENABLE_MOCK_ZEC_STAKE)
    auto const & account = registration_account;
#else
    auto const & account = sender();
#endif
    xdbg("[xrec_registration_contract_new_t::registerNode2] call xregistration_contract registerNode() pid:%d, balance: %lld, account: %s, node_types: %s, signing_key: %s, dividend_rate: %u",
         getpid(),
         balance(),
         account.c_str(),
         role_type_name.c_str(),
         signing_key.c_str(),
         dividend_rate);

    xreg_node_info node_info;
    auto ret = get_node_info(account.value(), node_info);
    XCONTRACT_ENSURE(ret != 0, "xrec_registration_contract_new_t::registerNode2: node exist!");
    common::xrole_type_t role_type = common::to_role_type(role_type_name);
    XCONTRACT_ENSURE(role_type != common::xrole_type_t::invalid, "xrec_registration_contract_new_t::registerNode2: invalid node_type!");
    XCONTRACT_ENSURE(is_valid_name(nickname) == true, "xrec_registration_contract_new_t::registerNode: invalid nickname");
    XCONTRACT_ENSURE(dividend_rate >= 0 && dividend_rate <= 100, "xrec_registration_contract_new_t::registerNode: dividend_rate must be >=0 and be <= 100");

    std::error_code ec;
    auto token = last_action_asset(ec);
    auto token_amount = token.amount();
    asset_to_next_action(std::move(token));
    assert(!ec);
    top::error::throw_error(ec);

    node_info.m_account = account;
    node_info.m_registered_role = role_type;
#if defined(XENABLE_MOCK_ZEC_STAKE)
    token_amount = 100000000000000;
    node_info.m_account_mortgage = 100000000000000;
#else
    node_info.m_account_mortgage += token_amount;
#endif
    node_info.nickname                  = nickname;
    node_info.consensus_public_key      = xpublic_key_t{signing_key};
    node_info.m_support_ratio_numerator = dividend_rate;
    if (network_ids.empty()) {
        xdbg("[xrec_registration_contract_new_t::registerNode2] network_ids empty");
        common::xnetwork_id_t nid{top::config::to_chainid(XGET_CONFIG(chain_name))};
        node_info.m_network_ids.insert(nid);
    } else {
        std::string network_ids_str;
        for (auto const & net_id : network_ids) {
            network_ids_str += net_id.to_string() + ' ';
        }
        xdbg("[xrec_registration_contract_new_t::registerNode2] network_ids %s", network_ids_str.c_str());
        node_info.m_network_ids = network_ids;
    }

    if (node_info.is_validator_node()) {
        if (node_info.m_validator_credit_numerator == 0) {
            node_info.m_validator_credit_numerator = XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_credit);
        }
    }
    if (node_info.is_auditor_node()) {
        if (node_info.m_auditor_credit_numerator == 0) {
            node_info.m_auditor_credit_numerator = XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_credit);
        }
    }
    uint64_t min_deposit = node_info.get_required_min_deposit();
    xdbg(("[xrec_registration_contract_new_t::registerNode2] call xregistration_contract registerNode() pid:%d, transaction_type:%d, source action type: %d, m_deposit: %" PRIu64
          ", min_deposit: %" PRIu64 ", account: %s"),
         getpid(),
         transaction_type(),
         source_action_type(),
         token_amount,
         min_deposit,
         account.c_str());
    XCONTRACT_ENSURE(token_amount >= min_deposit, "xrec_registration_contract_new_t::registerNode2: mortgage must be greater than minimum deposit");
    XCONTRACT_ENSURE(node_info.m_account_mortgage >= min_deposit, "xrec_registration_contract_new_t::registerNode2: mortgage must be greater than minimum deposit");

    update_node_info(node_info);
    check_and_set_genesis_stage();

    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "registerNode_Executed", 1);
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "registeredUserCnt", 1);
}

void xtop_rec_registration_contract_new::unregisterNode() {
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "unregisterNode_Called", 1);
    XMETRICS_TIME_RECORD(XREG_CONTRACT "unregisterNode_ExecutionTime");
    uint64_t cur_time = time();
    std::string const& account = sender().to_string();
    xdbg("[xrec_registration_contract_new_t::unregisterNode] call xregistration_contract unregisterNode(), balance: %lld, account: %s", balance(), account.c_str());

    xreg_node_info node_info;
    auto ret = get_node_info(account, node_info);
    XCONTRACT_ENSURE(ret == 0, "xrec_registration_contract_new_t::unregisterNode: node not exist");

    xslash_info s_info;
    if (get_slash_info(account, s_info) == 0 && s_info.m_staking_lock_time > 0) {
        XCONTRACT_ENSURE(cur_time - s_info.m_punish_time >= s_info.m_staking_lock_time, "[xrec_registration_contract_new_t::unregisterNode]: has punish time, cannot deregister now");
    }

    xdbg("[xrec_registration_contract_new_t::unregisterNode] call xregistration_contract unregisterNode() pid:%d, balance:%lld, account: %s, refund: %lld",
         getpid(),
         balance(),
         account.c_str(),
         node_info.m_account_mortgage);
    // refund
    // TRANSFER(account, node_info.m_account_mortgage);

    ins_refund(account, node_info.m_account_mortgage);

    delete_node_info(account);

    xdbg("[xrec_registration_contract_new_t::unregisterNode] finish call xregistration_contract unregisterNode() pid:%d", getpid());

    XMETRICS_COUNTER_DECREMENT(XREG_CONTRACT "registeredUserCnt", 1);
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "unregisterNode_Executed", 1);
}

void xtop_rec_registration_contract_new::updateNodeInfo(const std::string & nickname, const int updateDepositType, const uint64_t deposit, const uint32_t dividend_rate, const std::string & node_types, const std::string & node_sign_key) {
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "updateNodeInfo_Called", 1);
    XMETRICS_TIME_RECORD(XREG_CONTRACT "updateNodeInfo_ExecutionTime");

    auto const & source_account = sender();

    xdbg("[xrec_registration_contract_new_t::updateNodeInfo] call xregistration_contract updateNodeInfo() pid:%d, balance: %lld, account: %s, nickname: %s, updateDepositType: %u, deposit: %llu, dividend_rate: %u, node_types: %s, node_sign_key: %s",
         getpid(),
         balance(),
         source_account.c_str(),
         nickname.c_str(),
         updateDepositType,
         deposit,
         dividend_rate,
         node_types.c_str(),
         node_sign_key.c_str());

    xreg_node_info node_info;
    auto ret = get_node_info(source_account.value(), node_info);
    XCONTRACT_ENSURE(ret == 0, "xrec_registration_contract_new_t::updateNodeInfo: node does not exist!");
    XCONTRACT_ENSURE(is_valid_name(nickname) == true, "xrec_registration_contract_new_t::updateNodeInfo: invalid nickname");
    XCONTRACT_ENSURE(updateDepositType == 1 || updateDepositType == 2, "xrec_registration_contract_new_t::updateNodeInfo: invalid updateDepositType");
    XCONTRACT_ENSURE(dividend_rate >= 0 && dividend_rate <= 100, "xrec_registration_contract_new_t::updateNodeInfo: dividend_rate must be greater than or be equal to zero");
    common::xrole_type_t role_type = common::to_role_type(node_types);
    XCONTRACT_ENSURE(role_type != common::xrole_type_t::invalid, "xrec_registration_contract_new_t::updateNodeInfo: invalid node_type!");

    node_info.nickname          = nickname;
    node_info.m_registered_role = role_type;

    uint64_t min_deposit = node_info.get_required_min_deposit();
    if (updateDepositType == 1) { // stake deposit
        std::error_code ec;
        auto token = last_action_asset(ec);
        auto token_amount = token.amount();
        asset_to_next_action(std::move(token));
        assert(!ec);
        top::error::throw_error(ec);
        node_info.m_account_mortgage += token_amount;
        XCONTRACT_ENSURE(node_info.m_account_mortgage >= min_deposit, "xrec_registration_contract_new_t::updateNodeInfo: deposit not enough");
    } else {

        XCONTRACT_ENSURE(deposit <= node_info.m_account_mortgage && node_info.m_account_mortgage - deposit >= min_deposit, "xrec_registration_contract_new_t::updateNodeInfo: unstake deposit too big");
        if (deposit > 0) {
            transfer(source_account, deposit, contract_common::xfollowup_transaction_schedule_type_t::immediately);
        }
        node_info.m_account_mortgage -= deposit;
    }

    if (node_info.m_support_ratio_numerator != dividend_rate) {
        uint64_t cur_time = time();
        auto SDR_INTERVAL = XGET_ONCHAIN_GOVERNANCE_PARAMETER(dividend_ratio_change_interval);
        XCONTRACT_ENSURE(node_info.m_last_update_time == 0 || cur_time - node_info.m_last_update_time >= SDR_INTERVAL,
                     "xrec_registration_contract_new_t::updateNodeInfo: set must be longer than or equal to SDR_INTERVAL");

        node_info.m_support_ratio_numerator = dividend_rate;
        node_info.m_last_update_time = cur_time;
    }
    node_info.consensus_public_key = xpublic_key_t{node_sign_key};
    if (node_info.is_validator_node()) {
        if (node_info.m_validator_credit_numerator == 0) {
            node_info.m_validator_credit_numerator = XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_credit);
        }
    }
    if (node_info.is_auditor_node()) {
        if (node_info.m_auditor_credit_numerator == 0) {
            node_info.m_auditor_credit_numerator = XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_credit);
        }
    }

    update_node_info(node_info);
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "updateNodeInfo_Executed", 1);
}

void xtop_rec_registration_contract_new::redeemNodeDeposit() {
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "redeemNodeDeposit_Called", 1);
    XMETRICS_TIME_RECORD(XREG_CONTRACT "redeemNodeDeposit_ExecutionTime");
    uint64_t cur_time = time();
    auto const & source_account = sender();
    xdbg("[xrec_registration_contract_new_t::redeemNodeDeposit] pid:%d, balance: %lld, account: %s\n", getpid(), balance(), source_account.c_str());

    xrefund_info refund;
    auto ret = get_refund(source_account.value(), refund);
    XCONTRACT_ENSURE(ret == 0, "xrec_registration_contract_new_t::redeemNodeDeposit: refund not exist");
    XCONTRACT_ENSURE(cur_time - refund.create_time >= REDEEM_INTERVAL, "xrec_registration_contract_new_t::redeemNodeDeposit: interval must be greater than or equal to REDEEM_INTERVAL");

    xdbg("[xrec_registration_contract_new_t::redeemNodeDeposit] pid:%d, balance:%llu, account: %s, refund amount: %llu\n", getpid(), balance(), source_account.c_str(), refund.refund_amount);
    // refund
    // transfer(source_account, refund.refund_amount, contract_common::xfollowup_transaction_schedule_type_t::delay);

    del_refund(source_account.value());
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "redeemNodeDeposit_Executed", 1);
}

void xtop_rec_registration_contract_new::setDividendRatio(uint32_t dividend_rate) {
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "setDividendRatio_Called", 1);
    XMETRICS_TIME_RECORD(XREG_CONTRACT "setDividendRatio_ExecutionTime");
    auto const & source_account = sender();
    uint64_t cur_time = time();

    xreg_node_info node_info;
    auto ret = get_node_info(source_account.value(), node_info);

    XCONTRACT_ENSURE(ret == 0, "xrec_registration_contract_new_t::setDividendRatio: node not exist");

    xdbg("[xrec_registration_contract_new_t::setDividendRatio] pid:%d, balance: %lld, account: %s, dividend_rate: %u, cur_time: %llu, last_update_time: %llu\n",
         getpid(),
         balance(),
         source_account.c_str(),
         dividend_rate,
         cur_time,
         node_info.m_last_update_time);

    auto SDR_INTERVAL = XGET_ONCHAIN_GOVERNANCE_PARAMETER(dividend_ratio_change_interval);
    XCONTRACT_ENSURE(node_info.m_last_update_time == 0 || cur_time - node_info.m_last_update_time >= SDR_INTERVAL,
                     "xrec_registration_contract_new_t::setDividendRatio: set must be longer than or equal to SDR_INTERVAL");
    XCONTRACT_ENSURE(dividend_rate >= 0 && dividend_rate <= 100, "xrec_registration_contract_new_t::setDividendRatio: dividend_rate must be >=0 and be <= 100");

    xdbg("[xrec_registration_contract_new_t::setDividendRatio] call xregistration_contract registerNode() pid:%d, balance:%lld, account: %s\n",
         getpid(),
         balance(),
         source_account.c_str());
    node_info.m_support_ratio_numerator = dividend_rate;
    node_info.m_last_update_time = cur_time;

    update_node_info(node_info);
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "setDividendRatio_Executed", 1);
}

void xtop_rec_registration_contract_new::setNodeName(const std::string & nickname) {
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "setNodeName_Called", 1);
    XMETRICS_TIME_RECORD(XREG_CONTRACT "setNodeName_ExecutionTime");
    auto const & source_account = sender();

    XCONTRACT_ENSURE(is_valid_name(nickname) == true, "xrec_registration_contract_new_t::setNodeName: invalid nickname");

    xreg_node_info node_info;
    auto ret = get_node_info(source_account.value(), node_info);
    XCONTRACT_ENSURE(ret == 0, "xrec_registration_contract_new_t::setNodeName: node not exist");

    xdbg("[xrec_registration_contract_new_t::setNodeName] pid:%d, balance: %lld, account: %s, nickname: %s\n", getpid(), balance(), source_account.c_str(), nickname.c_str());
    XCONTRACT_ENSURE(node_info.nickname != nickname, "xrec_registration_contract_new_t::setNodeName: nickname can not be same");

    node_info.nickname = nickname;
    update_node_info(node_info);
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "setNodeName_Executed", 1);
}

void xtop_rec_registration_contract_new::updateNodeSignKey(const std::string & node_sign_key) {
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "updateNodeSignKey_Called", 1);
    XMETRICS_TIME_RECORD(XREG_CONTRACT "updateNodeSignKey_ExecutionTime");
    auto const & source_account = sender();

    xreg_node_info node_info;
    auto ret = get_node_info(source_account.value(), node_info);
    XCONTRACT_ENSURE(ret == 0, "xrec_registration_contract_new_t::updateNodeSignKey: node not exist");

    xdbg("[xrec_registration_contract_new_t::updateNodeSignKey] pid:%d, balance: %lld, account: %s, node_sign_key: %s\n", getpid(), balance(), source_account.c_str(), node_sign_key.c_str());
    XCONTRACT_ENSURE(node_info.consensus_public_key.to_string() != node_sign_key, "xrec_registration_contract_new_t::updateNodeSignKey: node_sign_key can not be same");

    node_info.consensus_public_key = xpublic_key_t{node_sign_key};
    update_node_info(node_info);
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "updateNodeSignKey_Executed", 1);
}

// void xtop_rec_registration_contract_new::update_node_credit(std::set<std::string> const& accounts) {
//     std::string source_address = SOURCE_ADDRESS();
//     if (sys_contract_zec_workload_addr != source_address) {
//         xwarn("[xrec_registration_contract_new_t::update_node_credit] invalid call from %s", source_address.c_str());
//         return;
//     }

//     xreg_node_info node_info;

//     for (auto & account : accounts) {
//         auto ret = get_node_info(account, node_info);
//         if (ret != 0) {
//             xdbg("[xtop_rec_registration_contract_new::update_node_credit] get_node_info error, account: %s\n", account.c_str());
//             continue;
//         }
//         node_info.update_credit_score();
//         // node_info.calc_stake();
//         update_node_info(node_info);
//     }
// }

void xtop_rec_registration_contract_new::update_node_info(xreg_node_info & node_info) {
    xstream_t stream(xcontext_t::instance());
    node_info.serialize_to(stream);
    m_reg_prop.set(node_info.m_account.value(), std::string{reinterpret_cast<char *>(stream.data()), static_cast<size_t>(stream.size())});
}

void xtop_rec_registration_contract_new::delete_node_info(std::string const & account) {
    XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_REG_KEY_RemoveExecutionTime");
    m_reg_prop.remove(account);
}

int32_t xtop_rec_registration_contract_new::get_node_info(const std::string & account, xreg_node_info & node_info) {
    xdbg("[xrec_registration_contract_new_t] get_node_info account(%s) pid:%d\n", account.c_str(), getpid());

    auto value_str = m_reg_prop.get(account);
    if (value_str.empty()) {
        xdbg("[xrec_registration_contract_new_t] account(%s) not exist pid:%d\n", account.c_str(), getpid());
        return enum_xstore_error_type::xaccount_property_not_exist;
    }

    base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.data(), (uint32_t)value_str.size());

    node_info.serialize_from(stream);

    return 0;
}

bool xtop_rec_registration_contract_new::check_if_signing_key_exist(const std::string & signing_key) {
    std::map<std::string, std::string> map_nodes;

    {
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_REG_KEY_CopyGetExecutionTime");
        map_nodes = m_reg_prop.value();
    }

    for (auto const & it : map_nodes) {
        xstake::xreg_node_info reg_node_info;
        xstream_t stream(xcontext_t::instance(), (uint8_t *)it.second.c_str(), it.second.size());
        reg_node_info.serialize_from(stream);
        if (reg_node_info.consensus_public_key.to_string() == signing_key) return true;
    }

    return false;
}

int32_t xtop_rec_registration_contract_new::ins_refund(const std::string & account, uint64_t const & refund_amount) {
    xrefund_info refund;
    get_refund(account, refund);

    refund.refund_amount += refund_amount;
    refund.create_time   = time();

    base::xstream_t stream(base::xcontext_t::instance());

    refund.serialize_to(stream);
    {
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_REFUND_KEY_SetExecutionTime");
        m_refund_prop.set(account, std::string{reinterpret_cast<char*>(stream.data()), static_cast<size_t>(stream.size())});
    }


    return 0;
}

int32_t xtop_rec_registration_contract_new::del_refund(const std::string & account) {
    {
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_REFUND_KEY_RemoveExecutionTime");
        m_refund_prop.remove(account);
    }

    return 0;
}

int32_t xtop_rec_registration_contract_new::get_refund(const std::string & account, xrefund_info & refund) {
    auto value_str = m_refund_prop.get(account);

    if (value_str.empty()) {
        xdbg("[xrec_registration_contract_new_t::get_refund] account(%s) not exist pid:%d\n", account.c_str(), getpid());
        return xaccount_property_not_exist;
    }

    base::xstream_t stream(base::xcontext_t::instance(), reinterpret_cast<uint8_t *>(const_cast<char *>(value_str.data())), static_cast<uint32_t>(value_str.size()));

    refund.serialize_from(stream);

    return 0;
}

void xtop_rec_registration_contract_new::update_batch_stake(std::map<std::string, std::string> const & contract_adv_votes) {
    XMETRICS_TIME_RECORD(XREG_CONTRACT "update_batch_stake_ExecutionTime");
    auto const & source_account = sender();
    xdbg("[xtop_rec_registration_contract_new::update_batch_stake] src_addr: %s, pid:%d, size: %d\n", source_account.c_str(), getpid(), contract_adv_votes.size());

    std::string base_addr;
    uint32_t    table_id;
    if (!data::xdatautil::extract_parts(source_account.value(), base_addr, table_id) || sys_contract_sharding_vote_addr != base_addr) {
        xwarn("[xtop_rec_registration_contract_new::update_batch_stake] invalid call from %s", source_account.c_str());
        return;
    }

    xstream_t stream(xcontext_t::instance());
    stream << contract_adv_votes;
    std::string contract_adv_votes_str = std::string((const char *)stream.data(), stream.size());
    {
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_TICKETS_KEY_SetExecutionTime");
        m_tickets_prop.set(source_account.value(), contract_adv_votes_str);
    }

    std::map<std::string, std::string> votes_table = m_tickets_prop.value();
    std::map<std::string, std::string> map_nodes = m_reg_prop.value();

    auto update_adv_votes = [&](std::string adv_account, std::map<std::string, std::string> votes_table) {
        uint64_t total_votes = 0;
        for (auto const & vote : votes_table) {
            auto const & vote_str = vote.second;
            std::map<std::string, std::string> contract_votes;
            if (!vote_str.empty()) {
                base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)vote_str.c_str(), (uint32_t)vote_str.size());
                stream >> contract_votes;
            }

            auto iter = contract_votes.find(adv_account);
            if (iter != contract_votes.end()) {
                total_votes += base::xstring_utl::touint64(iter->second);
            }
        }

        return total_votes;
    };

    for (auto const & entity : map_nodes) {
        auto const & account = entity.first;
        auto const & reg_node_str = entity.second;
        if (reg_node_str.empty()) {
            continue;
        }
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)reg_node_str.data(), reg_node_str.size());
        xreg_node_info reg_node_info;
        reg_node_info.serialize_from(stream);

        if (!reg_node_info.is_auditor_node()) {
            continue;
        }

        reg_node_info.m_vote_amount = update_adv_votes(account, votes_table);
        //reg_node_info.calc_stake();
        update_node_info(reg_node_info);
    }

    check_and_set_genesis_stage();
}

bool xtop_rec_registration_contract_new::handle_receive_shard_votes(uint64_t report_time, uint64_t last_report_time, std::map<std::string, std::string> const & contract_adv_votes, std::map<std::string, std::string> & merge_contract_adv_votes) {
    xdbg("[xtop_rec_registration_contract_new::handle_receive_shard_votes] report vote table size: %d, original vote table size: %d",
            contract_adv_votes.size(), merge_contract_adv_votes.size());
    if (report_time < last_report_time) {
        return false;
    }
    if (report_time == last_report_time) {
        merge_contract_adv_votes.insert(contract_adv_votes.begin(), contract_adv_votes.end());
        xdbg("[xtop_rec_registration_contract_new::handle_receive_shard_votes] same batch of vote report, report vote table size: %d, total size: %d",
            contract_adv_votes.size(), merge_contract_adv_votes.size());
    } else {
        merge_contract_adv_votes = contract_adv_votes;
    }
    return true;
}

void xtop_rec_registration_contract_new::update_batch_stake_v2(uint64_t report_time, std::map<std::string, std::string> const & contract_adv_votes) {
    XMETRICS_TIME_RECORD(XREG_CONTRACT "update_batch_stake_ExecutionTime");
    auto const & source_account = sender();
    xdbg("[xtop_rec_registration_contract_new::update_batch_stake_v2] src_addr: %s, report_time: %llu, pid:%d, contract_adv_votes size: %d\n",
        source_account.c_str(), report_time, getpid(), contract_adv_votes.size());

    std::string base_addr;
    uint32_t    table_id;
    if (!data::xdatautil::extract_parts(source_account.value(), base_addr, table_id) || sys_contract_sharding_vote_addr != base_addr) {
        xwarn("[xtop_rec_registration_contract_new::update_batch_stake_v2] invalid call from %s", source_account.c_str());
        return;
    }

    bool replace = true;
    uint64_t last_report_time = 0;
    std::string value_str = m_votes_report_time_prop.get(source_account.value());
    if (!value_str.empty()) {
        last_report_time = base::xstring_utl::touint64(value_str);
        xdbg("[xtop_rec_registration_contract_new::update_batch_stake_v2] last_report_time: %llu", last_report_time);
    }
    m_votes_report_time_prop.set(source_account.value(), base::xstring_utl::tostring(report_time));

    {
        std::map<std::string, std::string> auditor_votes;
        std::string auditor_votes_str = m_tickets_prop.get(source_account.value());
        if (!auditor_votes_str.empty()) {
            base::xstream_t votes_stream(base::xcontext_t::instance(), (uint8_t *)auditor_votes_str.c_str(), (uint32_t)auditor_votes_str.size());
            votes_stream >> auditor_votes;
        }
        if ( !handle_receive_shard_votes(report_time, last_report_time, contract_adv_votes, auditor_votes) ) {
            XCONTRACT_ENSURE(false, "[xrec_registration_contract_new_t::on_receive_shard_votes_v2] handle_receive_shard_votes fail");
        }

        xstream_t stream(xcontext_t::instance());
        stream << auditor_votes;
        std::string contract_adv_votes_str = std::string((const char *)stream.data(), stream.size());
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_TICKETS_KEY_SetExecutionTime");
        m_tickets_prop.set(source_account.value(), contract_adv_votes_str);
    }

    std::map<std::string, std::string> votes_table;
    {
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_TICKETS_KEY_CopyGetExecutionTime");
        votes_table = m_tickets_prop.value();
    }

    std::map<std::string, std::string> map_nodes;
    {
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_REG_KEY_CopyGetExecutionTime");
        map_nodes = m_reg_prop.value();
    }

    auto update_adv_votes = [&](std::string adv_account, std::map<std::string, std::string> votes_table) {
        uint64_t total_votes = 0;
        for (auto const & vote : votes_table) {
            auto const & vote_str = vote.second;
            std::map<std::string, std::string> contract_votes;
            if (!vote_str.empty()) {
                base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)vote_str.c_str(), (uint32_t)vote_str.size());
                stream >> contract_votes;
            }

            auto iter = contract_votes.find(adv_account);
            if (iter != contract_votes.end()) {
                total_votes += base::xstring_utl::touint64(iter->second);
            }
        }

        return total_votes;
    };

    for (auto const & entity : map_nodes) {
        auto const & account = entity.first;
        auto const & reg_node_str = entity.second;
        if (reg_node_str.empty()) {
            continue;
        }
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)reg_node_str.data(), reg_node_str.size());
        xreg_node_info reg_node_info;
        reg_node_info.serialize_from(stream);

        reg_node_info.m_vote_amount = update_adv_votes(account, votes_table);
        update_node_info(reg_node_info);
    }

    check_and_set_genesis_stage();
}

void xtop_rec_registration_contract_new::check_and_set_genesis_stage() {
    std::string value_str = m_genesis_prop.value();
    xactivation_record record;

    if (!value_str.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.c_str(), (uint32_t)value_str.size());
        record.serialize_from(stream);
    }
    if (record.activated) {
        xinfo("[xrec_registration_contract_new_t::check_and_set_genesis_stage] activated: %d, activation_time: %llu, pid:%d\n", record.activated, record.activation_time, getpid());
        return;
    }

    auto map_nodes = m_reg_prop.value();
    bool active = check_registered_nodes_active(map_nodes);
    if (active) {
        record.activated = 1;
        record.activation_time = time();
    }

    base::xstream_t stream(base::xcontext_t::instance());
    record.serialize_to(stream);
    m_genesis_prop.set(std::string((char *)stream.data(), stream.size()));
}

void xtop_rec_registration_contract_new::updateNodeType(const std::string & node_types) {
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "updateNodeType_Called", 1);
    XMETRICS_TIME_RECORD(XREG_CONTRACT "updateNodeType_ExecutionTime");
    auto const& source_account = sender();

    xdbg("[xtop_rec_registration_contract_new::updateNodeType] pid: %d, balance: %lld, account: %s, node_types: %s\n",
        getpid(), balance(), source_account.c_str(), node_types.c_str());

    xreg_node_info node_info;
    auto ret = get_node_info(source_account.value(), node_info);
    XCONTRACT_ENSURE(ret == 0, "xtop_rec_registration_contract_new::updateNodeType: node not exist");


    common::xrole_type_t role_type = common::to_role_type(node_types);
    XCONTRACT_ENSURE(role_type != common::xrole_type_t::invalid, "xtop_rec_registration_contract_new::updateNodeType: invalid node_type!");
    XCONTRACT_ENSURE(role_type != node_info.m_registered_role, "xtop_rec_registration_contract_new::updateNodeType: node_types can not be same!");
    node_info.m_registered_role  = role_type;

    std::error_code ec;
    auto token = last_action_asset(ec);
    auto token_amount = token.amount();
    asset_to_next_action(std::move(token));
    assert(!ec);
    top::error::throw_error(ec);
    node_info.m_account_mortgage += token_amount;

    uint64_t min_deposit = node_info.get_required_min_deposit();
    xdbg(("[xtop_rec_registration_contract_new::updateNodeType] min_deposit: %" PRIu64 ", account: %s, account morgage: %llu\n"),
        min_deposit, source_account.c_str(), node_info.m_account_mortgage);
    XCONTRACT_ENSURE(node_info.m_account_mortgage >= min_deposit, "xtop_rec_registration_contract_new::updateNodeType: deposit not enough");

    if (node_info.is_validator_node()) {
        if (node_info.m_validator_credit_numerator == 0) {
            node_info.m_validator_credit_numerator = XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_credit);
        }
    }
    if (node_info.is_auditor_node()) {
        if (node_info.m_auditor_credit_numerator == 0) {
            node_info.m_auditor_credit_numerator = XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_credit);
        }
    }

    update_node_info(node_info);
    check_and_set_genesis_stage();

    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "updateNodeType_Executed", 1);
}

void xtop_rec_registration_contract_new::stakeDeposit() {
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "stakeDeposit_Called", 1);
    XMETRICS_TIME_RECORD(XREG_CONTRACT "stakeDeposit_ExecutionTime");
    auto const& source_account = sender();
    xreg_node_info node_info;
    auto ret = get_node_info(source_account.value(), node_info);
    XCONTRACT_ENSURE(ret == 0, "xrec_registration_contract_new_t::stakeDeposit: node not exist");

    std::error_code ec;
    auto token = last_action_asset(ec);
    auto token_amount = token.amount();
    asset_to_next_action(std::move(token));
    assert(!ec);
    top::error::throw_error(ec);

    xdbg("[xrec_registration_contract_new_t::stakeDeposit] pid: %d, balance: %lld, account: %s, stake deposit: %llu\n", getpid(), balance(), source_account.c_str(), token_amount);
    XCONTRACT_ENSURE(token_amount != 0, "xrec_registration_contract_new_t::stakeDeposit: stake deposit can not be zero");

    node_info.m_account_mortgage += token_amount;
    update_node_info(node_info);
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "stakeDeposit_Executed", 1);
}

void xtop_rec_registration_contract_new::unstakeDeposit(uint64_t unstake_deposit) {
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "unstakeDeposit_Called", 1);
    XMETRICS_TIME_RECORD(XREG_CONTRACT "unstakeDeposit_ExecutionTime");
    auto const& source_account = sender();
    xreg_node_info node_info;
    auto ret = get_node_info(source_account.value(), node_info);
    XCONTRACT_ENSURE(ret == 0, "xrec_registration_contract_new_t::unstakeDeposit: node not exist");

    xdbg("[xrec_registration_contract_new_t::unstakeDeposit] pid: %d, balance: %lld, account: %s, unstake deposit: %llu, account morgage: %llu\n",
        getpid(), balance(), source_account.c_str(), unstake_deposit, node_info.m_account_mortgage);
    XCONTRACT_ENSURE(unstake_deposit != 0, "xrec_registration_contract_new_t::unstakeDeposit: unstake deposit can not be zero");

    uint64_t min_deposit = node_info.get_required_min_deposit();
    XCONTRACT_ENSURE(unstake_deposit <= node_info.m_account_mortgage && node_info.m_account_mortgage - unstake_deposit >= min_deposit, "xrec_registration_contract_new_t::unstakeDeposit: unstake deposit too big");
    transfer(source_account, unstake_deposit, contract_common::xfollowup_transaction_schedule_type_t::immediately);

    node_info.m_account_mortgage -= unstake_deposit;
    update_node_info(node_info);
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "unstakeDeposit_Executed", 1);
}

int32_t xtop_rec_registration_contract_new::get_slash_info(std::string const & account, xslash_info & node_slash_info) {
    auto value_str = m_slash_prop.get(account);
    if (value_str.empty()) {
        xdbg("[xrec_registration_contract_new_t][get_slash_info] account(%s) not exist,  pid:%d\n", account.c_str(), getpid());
        return xaccount_property_not_exist;
    }

    xstream_t stream(xcontext_t::instance(), reinterpret_cast<uint8_t*>(const_cast<char *>(value_str.data())), value_str.size());
    node_slash_info.serialize_from(stream);

    return 0;
}

void xtop_rec_registration_contract_new::slash_staking_time(std::string const & node_addr) {
    auto current_time = time();
    xslash_info s_info;
    get_slash_info(node_addr, s_info);
    if (0 != s_info.m_punish_time) {  // already has slahs info
        auto minus_time = current_time - s_info.m_punish_time;
        if (minus_time > s_info.m_staking_lock_time) {
            s_info.m_staking_lock_time = 0;
        } else {
            s_info.m_staking_lock_time -= minus_time;
        }
    }

    auto lock_time_inc = XGET_ONCHAIN_GOVERNANCE_PARAMETER(backward_node_lock_duration_increment);
    auto lock_time_max = XGET_ONCHAIN_GOVERNANCE_PARAMETER(max_nodedeposit_lock_duration);
    s_info.m_punish_time = current_time;
    s_info.m_staking_lock_time += lock_time_inc;
    if (s_info.m_staking_lock_time > lock_time_max) {
        s_info.m_staking_lock_time = lock_time_max;
        s_info.m_punish_staking++;
    }

    xstream_t stream(xcontext_t::instance());
    s_info.serialize_to(stream);
    {
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPROPERTY_CONTRACT_SLASH_INFO_KEY_SetExecutionTime");
        m_slash_prop.set(node_addr, std::string((char *)stream.data(), stream.size()));
    }
}

void xtop_rec_registration_contract_new::slash_unqualified_node(std::string const & punish_node_str) {
    XMETRICS_TIME_RECORD(XREG_CONTRACT "slash_unqualified_node_ExecutionTime");
    auto const & self_account = address();
    auto const & source_account = sender();

    std::string base_addr = "";
    uint32_t table_id = 0;
    XCONTRACT_ENSURE(data::xdatautil::extract_parts(source_account.value(), base_addr, table_id), "source address extract base_addr or table_id error!");
    xdbg("[xtop_rec_registration_contract_new][slash_unqualified_node] self_account %s, source_addr %s, base_addr %s\n", self_account.c_str(), source_account.c_str(), base_addr.c_str());

    XCONTRACT_ENSURE(source_account.value() == sys_contract_zec_slash_info_addr, "invalid source addr's call!");

    std::vector<data::xaction_node_info_t> node_slash_info;
    base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)punish_node_str.data(), punish_node_str.size());
    VECTOR_OBJECT_DESERIALZE2(stream, node_slash_info);
    xinfo("[xtop_rec_registration_contract_new][slash_unqualified_node] do slash unqualified node, size: %zu", node_slash_info.size());

    for (auto const & value : node_slash_info) {
        std::string const & addr = value.node_id.value();
        xdbg("[xtop_rec_registration_contract_new][slash_unqualified_node] do slash unqualified node, addr: %s", addr.c_str());
        xreg_node_info reg_node;
        auto ret = get_node_info(addr, reg_node);
        if (ret != 0) {
            xwarn("[xtop_rec_registration_contract_new][slash_staking_time] get reg node_info error, account: %s", addr.c_str());
            continue;
        }

        if (value.action_type) {  // punish
            xkinfo("[xrec_registration_contract_new_t][slash_unqualified_node] effective slash credit & staking time, node addr: %s", addr.c_str());
            XMETRICS_PACKET_INFO("sysContract_zecSlash", "effective slash credit & staking time, node addr", addr);
            reg_node.slash_credit_score(value.node_type);
            slash_staking_time(addr);
        } else {
            xkinfo("[xrec_registration_contract_new_t][slash_unqualified_node] effective award credit, node addr: %s", addr.c_str());
            XMETRICS_PACKET_INFO("sysContract_zecSlash", "effective award credit, node addr", addr)
            reg_node.award_credit_score(value.node_type);
        }

        update_node_info(reg_node);
    }
}

bool xtop_rec_registration_contract_new::is_valid_name(const std::string & nickname) const {
    int len = nickname.length();
    if (len < 4 || len > 16) {
        return false;
    }

    return std::find_if(nickname.begin(), nickname.end(), [](unsigned char c) { return !std::isalnum(c) && c != '_'; }) == nickname.end();
};

NS_END2

#undef XREG_CONTRACT
#undef XCONTRACT_PREFIX
#undef XREC_MODULE

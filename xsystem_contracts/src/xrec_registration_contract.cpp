// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xregistration/xrec_registration_contract.h"

#include "xbase/xmem.h"
#include "xbase/xutl.h"
#include "xbasic/xutility.h"
#include "xcommon/xrole_type.h"
#include "xdata/xgenesis_data.h"
#include "xdata/xproperty.h"
#include "xdata/xslash.h"
#include "xdata/xrootblock.h"
#include "xelect_common/elect_option.h"
#include "xstore/xstore_error.h"
#include "secp256k1/secp256k1.h"
#include "secp256k1/secp256k1_recovery.h"
#include "xmetrics/xmetrics.h"
#include "xvledger/xvledger.h"
#include "xvm/xsystem_contracts/xerror/xerror.h"


using top::base::xcontext_t;
using top::base::xstream_t;
using namespace top::data;

#if !defined (XREC_MODULE)
#    define XREC_MODULE "sysContract_"
#endif
#define XCONTRACT_PREFIX "registration_"

#define XREG_CONTRACT XREC_MODULE XCONTRACT_PREFIX

NS_BEG2(top, xstake)

#define TIMER_ADJUST_DENOMINATOR 10

xrec_registration_contract::xrec_registration_contract(common::xnetwork_id_t const & network_id) : xbase_t{network_id} {}

void xrec_registration_contract::setup() {
    MAP_CREATE(XPORPERTY_CONTRACT_REG_KEY);
    MAP_CREATE(XPORPERTY_CONTRACT_TICKETS_KEY);
    MAP_CREATE(XPORPERTY_CONTRACT_REFUND_KEY);
    STRING_CREATE(XPORPERTY_CONTRACT_GENESIS_STAGE_KEY);
    MAP_CREATE(XPROPERTY_CONTRACT_SLASH_INFO_KEY);
#ifdef MAINNET_ACTIVATED
    xactivation_record record;
    record.activated = 1;
    record.activation_time = 1;

    base::xstream_t stream(base::xcontext_t::instance());
    record.serialize_to(stream);
    auto value_str = std::string((char *)stream.data(), stream.size());
    STRING_SET(XPORPERTY_CONTRACT_GENESIS_STAGE_KEY, value_str);
#endif

    xdbg("[xrec_registration_contract::setup] pid:%d\n", getpid());
    if (elect::ElectOption::Instance()->IsEnableStaticec()) {
        return;
    }
    xreg_node_info node_info;
    {
        common::xnetwork_id_t network_id{top::config::to_chainid(XGET_CONFIG(chain_name))};
        const std::vector<node_info_t> & seed_nodes = data::xrootblock_t::get_seed_nodes();
        for (size_t i = 0; i < seed_nodes.size(); i++) {
            node_info_t node_data = seed_nodes[i];

            node_info.m_account             = node_data.m_account;
            node_info.m_account_mortgage    = 0;
            node_info.m_genesis_node        = true;
            node_info.m_registered_role     = common::xenum_role_type::edge | common::xenum_role_type::advance | common::xenum_role_type::consensus;
            node_info.m_network_ids.insert(network_id.value());
            node_info.nickname              = std::string("bootnode") + std::to_string(i + 1);
            node_info.consensus_public_key  = xpublic_key_t{node_data.m_publickey};
            //xdbg("[xrec_registration_contract::setup] pid:%d,node account: %s, public key: %s\n", getpid(), node_data.m_account.c_str(), node_data.m_publickey.c_str());
            std::error_code ec;
            update_node_info(node_info, ec);
            XCONTRACT_ENSURE(!ec, "xrec_registration_contract::xrec_registration_contract: node update error!");
        }
        /*{
            // test
            for (auto i = 0; i < 700; i++) {
                std::stringstream ss;
                ss << std::setw(40) << std::setfill('0') << i;
                auto key = ss.str();
                node_info.m_account             = key;
                node_info.m_account_mortgage    = 1000000000000;
                node_info.m_genesis_node        = false;
                node_info.m_registered_role     = common::xenum_role_type::advance;
                node_info.m_network_ids.insert(network_id.value());
                node_info.nickname              = std::string("testnode") + std::to_string(i + 1);
                node_info.m_vote_amount         = 1000000;
                //node_info.consensus_public_key  = xpublic_key_t{node_data.m_publickey};
                //xdbg("[xrec_registration_contract::setup] pid:%d,node account: %s, public key: %s\n", getpid(), node_data.m_account.c_str(), node_data.m_publickey.c_str());
                update_node_info(node_info);
            }
        }*/
    }
}

void xrec_registration_contract::registerNode(const std::string & node_types, 
                                              const std::string & nickname, 
                                              const std::string & signing_key, 
                                              const uint32_t dividend_rate
#if defined XENABLE_MOCK_ZEC_STAKE
                                              , std::string const & registration_account
#endif
) {
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "registerNode_Called", 1);
    XMETRICS_TIME_RECORD(XREG_CONTRACT "registerNode_ExecutionTime");
    std::set<common::xnetwork_id_t> network_ids{common::xnetwork_id_t{top::config::to_chainid(XGET_CONFIG(chain_name))}};
#if defined XENABLE_MOCK_ZEC_STAKE
    auto const & account = common::xaccount_address_t{registration_account};
#else
    auto const & account = common::xaccount_address_t{SOURCE_ADDRESS()};
#endif
    XCONTRACT_ENSURE(!account.empty(), "xrec_registration_contract::registerNode: source address empty");
    XCONTRACT_ENSURE(base::xvaccount_t::get_addrtype_from_account(account.value()) != base::enum_vaccount_addr_type_invalid, "xrec_registration_contract::registerNode: source address invalid");

    xdbg("[xrec_registration_contract::registerNode2] call xregistration_contract registerNode(), balance: %lld, account: %s, node_types: %s, signing_key: %s, dividend_rate: %u",
         GET_BALANCE(),
         account.c_str(),
         node_types.c_str(),
         signing_key.c_str(),
         dividend_rate);

    std::error_code ec;
    auto node_info = get_node_info(account, ec);
    // auto ret = get_node_info(account, node_info);
    XCONTRACT_ENSURE(system_contracts::error::xsystem_contract_errc_t(ec.value()) == system_contracts::error::xsystem_contract_errc_t::rec_registration_node_info_not_found, "xrec_registration_contract::registerNode2: node exist!");
    const xtransaction_ptr_t trans_ptr = GET_TRANSACTION();
    xdbg("[xrec_registration_contract::registerNode2] call xregistration_contract registerNode(), transaction_type:%d, source action type: %d",
         trans_ptr->get_tx_type(),
         trans_ptr->get_source_action().get_action_type());
    XCONTRACT_ENSURE(trans_ptr->get_source_action().get_action_type() == xaction_type_asset_out,
                     "xrec_registration_contract::registerNode: source_action type must be xaction_type_asset_out");

    xstream_t stream(xcontext_t::instance(), (uint8_t *)trans_ptr->get_source_action().get_action_param().data(), trans_ptr->get_source_action().get_action_param().size());

    data::xproperty_asset asset_out{0};
    stream >> asset_out.m_token_name;
    stream >> asset_out.m_amount;
    auto instruction = registerNode2(account, node_types, nickname, signing_key, dividend_rate, network_ids, asset_out, node_info);
    execute_instruction(instruction);
    check_and_set_genesis_stage();

    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "registerNode_Executed", 1);
}

xproperty_instruction_t xrec_registration_contract::registerNode2(common::xaccount_address_t const & account,
                                                                  const std::string & node_types,
                                                                  const std::string & nickname,
                                                                  const std::string & signing_key,
                                                                  const uint32_t dividend_rate,
                                                                  const std::set<common::xnetwork_id_t> & network_ids,
                                                                  data::xproperty_asset const & asset_out,
                                                                  xreg_node_info & node_info) {
    common::xrole_type_t role_type = common::to_role_type(node_types);
    XCONTRACT_ENSURE(role_type != common::xrole_type_t::invalid, "xrec_registration_contract::registerNode2: invalid node_type!");
    XCONTRACT_ENSURE(is_valid_name(nickname) == true, "xrec_registration_contract::registerNode: invalid nickname");
    XCONTRACT_ENSURE(dividend_rate >= 0 && dividend_rate <= 100, "xrec_registration_contract::registerNode: dividend_rate must be >=0 and be <= 100");
    XCONTRACT_ENSURE(base::xvaccount_t::get_addrtype_from_account(account.value()) != base::enum_vaccount_addr_type_invalid, "xrec_registration_contract::registerNode: source address invalid");
    node_info.m_account = account;
    node_info.m_registered_role = role_type;
#if defined XENABLE_MOCK_ZEC_STAKE
    node_info.m_account_mortgage = 100000000000000;
#else
    node_info.m_account_mortgage += asset_out.m_amount;
#endif
    node_info.nickname = nickname;
    node_info.consensus_public_key = xpublic_key_t{signing_key};
    node_info.m_support_ratio_numerator = dividend_rate;
    if (network_ids.empty()) {
        xdbg("[xrec_registration_contract::registerNode2] network_ids empty\n");
        common::xnetwork_id_t nid{top::config::to_chainid(XGET_CONFIG(chain_name))};
        node_info.m_network_ids.insert(nid.value());
    } else {
#if defined(DEBUG)
        std::string network_ids_str;
        for (auto const & net_id : network_ids) {
            assert(net_id.has_value());
            network_ids_str += net_id.to_string() + ' ';
        }
        xdbg("[xrec_registration_contract::registerNode2] network_ids %s\n", network_ids_str.c_str());
#endif
    }
    uint64_t min_deposit = node_info.get_required_min_deposit();
    xdbg(("[xrec_registration_contract::registerNode2] call xregistration_contract registerNode(), m_deposit: %" PRIu64
          ", min_deposit: %" PRIu64 ", account: %s"),
         asset_out.m_amount,
         min_deposit,
         account.c_str());
    // XCONTRACT_ENSURE(asset_out.m_amount >= min_deposit, "xrec_registration_contract::registerNode2: mortgage must be greater than minimum deposit");
    XCONTRACT_ENSURE(node_info.m_account_mortgage >= min_deposit, "xrec_registration_contract::registerNode2: mortgage must be greater than minimum deposit");
    if (node_info.validator()) {
        if (node_info.m_validator_credit_numerator == 0) {
            node_info.m_validator_credit_numerator = XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_credit);
        }
    }
    if (node_info.is_auditor_node()) {
        if (node_info.m_auditor_credit_numerator == 0) {
            node_info.m_auditor_credit_numerator = XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_credit);
        }
    }
    
    return create_binlog(XPORPERTY_CONTRACT_REG_KEY, xproperty_cmd_type_map_set, node_info.m_account.value(), node_info);
}

void xrec_registration_contract::unregisterNode() {
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "unregisterNode_Called", 1);
    XMETRICS_TIME_RECORD(XREG_CONTRACT "unregisterNode_ExecutionTime");
    uint64_t cur_time = TIME();
    auto const & account = common::xaccount_address_t{SOURCE_ADDRESS()};
    XCONTRACT_ENSURE(base::xvaccount_t::get_addrtype_from_account(account.value()) != base::enum_vaccount_addr_type_invalid, "xrec_registration_contract::unregisterNode: source address invalid");
    xdbg(
        "[xrec_registration_contract::unregisterNode] call xregistration_contract unregisterNode() pid:%d, balance: %lld, account: %s\n", getpid(), GET_BALANCE(), account.c_str());

    std::error_code ec;
    auto node_info = get_node_info(account, ec);
    XCONTRACT_ENSURE(!ec, "xrec_registration_contract::unregisterNode: node not exist!");

    ec.clear();
    auto s_info = get_slash_info(account, ec);
    XCONTRACT_ENSURE(!ec, "xrec_registration_contract::unregisterNode: node not exist!");
    if (s_info.m_staking_lock_time > 0) {
        XCONTRACT_ENSURE(cur_time - s_info.m_punish_time >= s_info.m_staking_lock_time, "[xrec_registration_contract::unregisterNode]: has punish time, cannot deregister now");
    }
    xdbg("[xrec_registration_contract::unregisterNode] call xregistration_contract unregisterNode() pid:%d, balance:%lld, account: %s, refund: %lld\n",
         getpid(),
         GET_BALANCE(),
         account.c_str(),
         node_info.m_account_mortgage);

    ec.clear();
    update_node_refund(account, node_info.m_account_mortgage, ec);
    XCONTRACT_ENSURE(!ec, "xrec_registration_contract::registerNode2: update_node_refund error!");

    delete_node_info(account);
    xdbg("[xrec_registration_contract::unregisterNode] finish call xregistration_contract unregisterNode() pid:%d\n", getpid());

    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "unregisterNode_Executed", 1);
}

xproperty_instruction_t xrec_registration_contract::handleNodeInfo(uint64_t cur_time,
                                                                   const std::string & nickname,
                                                                   const uint32_t dividend_rate,
                                                                   const std::string & node_types,
                                                                   const std::string & node_sign_key,
                                                                   xreg_node_info & node_info) {
    XCONTRACT_ENSURE(is_valid_name(nickname) == true, "xrec_registration_contract::handleNodeInfo: invalid nickname");
    XCONTRACT_ENSURE(dividend_rate >= 0 && dividend_rate <= 100, "xrec_registration_contract::handleNodeInfo: dividend_rate must be greater than or be equal to zero");
    common::xrole_type_t role_type = common::to_role_type(node_types);
    XCONTRACT_ENSURE(role_type != common::xrole_type_t::invalid, "xrec_registration_contract::handleNodeInfo: invalid node_type!");

    node_info.nickname = nickname;
    node_info.m_registered_role = role_type;

    if (node_info.m_support_ratio_numerator != dividend_rate) {
        auto SDR_INTERVAL = XGET_ONCHAIN_GOVERNANCE_PARAMETER(dividend_ratio_change_interval);
        XCONTRACT_ENSURE(node_info.m_last_update_time == 0 || cur_time >= SDR_INTERVAL + node_info.m_last_update_time,
                         "xrec_registration_contract::handleNodeInfo: set must be longer than or equal to SDR_INTERVAL");
        node_info.m_support_ratio_numerator = dividend_rate;
        node_info.m_last_update_time = cur_time;
    }
    node_info.consensus_public_key = xpublic_key_t{node_sign_key};
    if (node_info.validator()) {
        if (node_info.m_validator_credit_numerator == 0) {
            node_info.m_validator_credit_numerator = XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_credit);
        }
    }
    if (node_info.is_auditor_node()) {
        if (node_info.m_auditor_credit_numerator == 0) {
            node_info.m_auditor_credit_numerator = XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_credit);
        }
    }
    return create_binlog(XPORPERTY_CONTRACT_REG_KEY, xproperty_cmd_type_map_set, node_info.m_account.value(), node_info);
}

void xrec_registration_contract::updateNodeInfo(const std::string & nickname,
                                                const int updateDepositType,
                                                const uint64_t deposit,
                                                const uint32_t dividend_rate,
                                                const std::string & node_types,
                                                const std::string & node_sign_key) {
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "updateNodeInfo_Called", 1);
    XMETRICS_TIME_RECORD(XREG_CONTRACT "updateNodeInfo_ExecutionTime");

    auto const & account = common::xaccount_address_t{SOURCE_ADDRESS()};
    xdbg(
        "[xrec_registration_contract::updateNodeInfo] call xregistration_contract updateNodeInfo() pid:%d, balance: %lld, account: %s, nickname: %s, updateDepositType: %u, "
        "deposit: %llu, dividend_rate: %u, node_types: %s, node_sign_key: %s\n",
        getpid(),
        GET_BALANCE(),
        account.c_str(),
        nickname.c_str(),
        updateDepositType,
        deposit,
        dividend_rate,
        node_types.c_str(),
        node_sign_key.c_str());
    XCONTRACT_ENSURE(!account.empty(), "xrec_registration_contract::updateNodeInfo: account must be not empty");
    XCONTRACT_ENSURE(base::xvaccount_t::get_addrtype_from_account(account.value()) != base::enum_vaccount_addr_type_invalid, "xrec_registration_contract::updateNodeInfo: source address invalid");
    std::error_code ec;
    auto node_info = get_node_info(account, ec);
    XCONTRACT_ENSURE(!ec, "xrec_registration_contract::updateNodeInfo: node not exist!");
    uint64_t cur_time = TIME();
    handleNodeInfo(cur_time, nickname, dividend_rate, node_types, node_sign_key, node_info);
    XCONTRACT_ENSURE(updateDepositType == 1 || updateDepositType == 2, "xrec_registration_contract::updateNodeInfo: invalid updateDepositType");
    uint64_t min_deposit = node_info.get_required_min_deposit();
    if (updateDepositType == 1) {  // stake deposit
        const xtransaction_ptr_t trans_ptr = GET_TRANSACTION();
        XCONTRACT_ENSURE(trans_ptr->get_source_action().get_action_type() == xaction_type_asset_out,
                         "xrec_registration_contract::updateNodeInfo: source_action type must be xaction_type_asset_out");
        if (trans_ptr->get_source_action().get_action_param().size() > 0) {
            xstream_t stream(xcontext_t::instance(), (uint8_t *)trans_ptr->get_source_action().get_action_param().data(), trans_ptr->get_source_action().get_action_param().size());
            data::xproperty_asset asset_out{0};
            stream >> asset_out.m_token_name;
            stream >> asset_out.m_amount;
            XCONTRACT_ENSURE(asset_out.m_amount == deposit, "xrec_registration_contract::updateNodeInfo: invalid deposit!");
            node_info.m_account_mortgage += asset_out.m_amount;
        }

        XCONTRACT_ENSURE(node_info.m_account_mortgage >= min_deposit, "xrec_registration_contract::updateNodeInfo: deposit not enough");
    } else {
        XCONTRACT_ENSURE(deposit <= node_info.m_account_mortgage && node_info.m_account_mortgage - deposit >= min_deposit,
                         "xrec_registration_contract::updateNodeInfo: unstake deposit too big");
        if (deposit > 0) {
            TRANSFER(node_info.m_account.value(), deposit);
        }
        node_info.m_account_mortgage -= deposit;
    }
    ec.clear();
    update_node_info(node_info, ec);
    XCONTRACT_ENSURE(!ec, "xrec_registration_contract::updateNodeInfo: node update error!");
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "updateNodeInfo_Executed", 1);
}

void xrec_registration_contract::redeemNodeDeposit() {
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "redeemNodeDeposit_Called", 1);
    XMETRICS_TIME_RECORD(XREG_CONTRACT "redeemNodeDeposit_ExecutionTime");
    auto const & account = common::xaccount_address_t{SOURCE_ADDRESS()};
    XCONTRACT_ENSURE(base::xvaccount_t::get_addrtype_from_account(account.value()) != base::enum_vaccount_addr_type_invalid, "xrec_registration_contract::redeemNodeDeposit: source address invalid");
    std::error_code ec;
    auto refund = get_node_refund(account, ec);
    uint64_t cur_time = TIME();
    XCONTRACT_ENSURE(!ec, "xrec_registration_contract::redeemNodeDeposit: refund not exist");
    xdbg("[xrec_registration_contract::redeemNodeDeposit] cur_time: %lu, create_time: %lu, REDEEM_INTERVAL: %lu\n", cur_time, refund.create_time, REDEEM_INTERVAL);
    XCONTRACT_ENSURE(cur_time - refund.create_time >= REDEEM_INTERVAL, "xrec_registration_contract::redeemNodeDeposit: interval must be greater than or equal to REDEEM_INTERVAL");
    xdbg(
        "[xrec_registration_contract::redeemNodeDeposit] pid:%d, balance:%llu, account: %s, refund amount: %llu\n", getpid(), GET_BALANCE(), account.c_str(), refund.refund_amount);
    // refund
    TRANSFER(account.value(), refund.refund_amount);

    delete_node_refund(account);
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "redeemNodeDeposit_Executed", 1);
}

xproperty_instruction_t xrec_registration_contract::handleDividendRatio(common::xlogic_time_t cur_time, uint32_t dividend_rate, xreg_node_info & node_info) {
    auto interval = XGET_ONCHAIN_GOVERNANCE_PARAMETER(dividend_ratio_change_interval);
    xdbg("[xrec_registration_contract::handleDividendRatio] cur_time: %lu, last_update_time: %lu, interval: %lu\n", cur_time, node_info.m_last_update_time, interval);
    XCONTRACT_ENSURE(node_info.m_last_update_time == 0 || cur_time - node_info.m_last_update_time >= interval,
                     "xrec_registration_contract::handleDividendRatio: set must be longer than or equal to SDR_INTERVAL");
    XCONTRACT_ENSURE(dividend_rate != node_info.m_support_ratio_numerator, "xrec_registration_contract::handleDividendRatio: dividend_rate must be different");
    XCONTRACT_ENSURE(dividend_rate >= 0 && dividend_rate <= 100, "xrec_registration_contract::handleDividendRatio: dividend_rate must be >=0 and be <= 100");
    node_info.m_support_ratio_numerator = dividend_rate;
    node_info.m_last_update_time = cur_time;
    return create_binlog(XPORPERTY_CONTRACT_REG_KEY, xproperty_cmd_type_map_set, node_info.m_account.value(), node_info);
}

void xrec_registration_contract::setDividendRatio(uint32_t dividend_rate) {
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "setDividendRatio_Called", 1);
    XMETRICS_TIME_RECORD(XREG_CONTRACT "setDividendRatio_ExecutionTime");
    // check params
    auto const & account = common::xaccount_address_t{SOURCE_ADDRESS()};
    XCONTRACT_ENSURE(base::xvaccount_t::get_addrtype_from_account(account.value()) != base::enum_vaccount_addr_type_invalid, "xrec_registration_contract::setDividendRatio: source address invalid");
    std::error_code ec;
    auto node_info = get_node_info(account, ec);
    XCONTRACT_ENSURE(!ec, "xrec_registration_contract::setDividendRatio: node not exist!");
    xdbg("[xrec_registration_contract::setDividendRatio] pid:%d, balance: %lld, account: %s, dividend_rate: %u\n", getpid(), GET_BALANCE(), account.c_str(), dividend_rate);
    common::xlogic_time_t cur_time = TIME();
    // update params
    auto instruction = handleDividendRatio(cur_time, dividend_rate, node_info);
    execute_instruction(instruction);
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "setDividendRatio_Executed", 1);
}

xproperty_instruction_t xrec_registration_contract::handleNodeName(const std::string & nickname, xreg_node_info & node_info) {
    XCONTRACT_ENSURE(is_valid_name(nickname) == true, "xrec_registration_contract::setNodeName: invalid nickname");
    XCONTRACT_ENSURE(node_info.nickname != nickname, "xrec_registration_contract::setNodeName: nickname can not be same");
    node_info.nickname = nickname;
    return create_binlog(XPORPERTY_CONTRACT_REG_KEY, xproperty_cmd_type_map_set, node_info.m_account.value(), node_info);
}

void xrec_registration_contract::setNodeName(const std::string & nickname) {
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "setNodeName_Called", 1);
    XMETRICS_TIME_RECORD(XREG_CONTRACT "setNodeName_ExecutionTime");
    // check params
    auto const & account = common::xaccount_address_t{SOURCE_ADDRESS()};
    XCONTRACT_ENSURE(base::xvaccount_t::get_addrtype_from_account(account.value()) != base::enum_vaccount_addr_type_invalid, "xrec_registration_contract::setNodeName: source address invalid");
    std::error_code ec;
    auto node_info = get_node_info(account, ec);
    XCONTRACT_ENSURE(!ec, "xrec_registration_contract::setNodeName: node not exist!");
    xdbg("[xrec_registration_contract::setNodeName] pid:%d, balance: %lld, account: %s, nickname: %s\n", getpid(), GET_BALANCE(), account.c_str(), nickname.c_str());
    auto instruction = handleNodeName(nickname, node_info);
    execute_instruction(instruction);
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "setNodeName_Executed", 1);
}

xproperty_instruction_t xrec_registration_contract::handleNodeSignKey(const std::string & node_sign_key, xreg_node_info & node_info) {
    XCONTRACT_ENSURE(node_info.consensus_public_key.to_string() != node_sign_key, "xrec_registration_contract::updateNodeSignKey: node_sign_key can not be same");
    node_info.consensus_public_key = xpublic_key_t{node_sign_key};
    return create_binlog(XPORPERTY_CONTRACT_REG_KEY, xproperty_cmd_type_map_set, node_info.m_account.value(), node_info);
}

void xrec_registration_contract::updateNodeSignKey(const std::string & node_sign_key) {
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "updateNodeSignKey_Called", 1);
    XMETRICS_TIME_RECORD(XREG_CONTRACT "updateNodeSignKey_ExecutionTime");
    // check params
    auto const & account = common::xaccount_address_t{SOURCE_ADDRESS()};
    XCONTRACT_ENSURE(base::xvaccount_t::get_addrtype_from_account(account.value()) != base::enum_vaccount_addr_type_invalid, "xrec_registration_contract::updateNodeSignKey: source address invalid");
    std::error_code ec;
    auto node_info = get_node_info(account, ec);
    XCONTRACT_ENSURE(!ec, "xrec_registration_contract::updateNodeSignKey: node not exist!");
    xdbg(
        "[xrec_registration_contract::updateNodeSignKey] pid:%d, balance: %lld, account: %s, node_sign_key: %s\n", getpid(), GET_BALANCE(), account.c_str(), node_sign_key.c_str());
    auto instruction = handleNodeSignKey(node_sign_key, node_info);
    execute_instruction(instruction);
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "updateNodeSignKey_Executed", 1);
}

void xrec_registration_contract::update_node_info(xreg_node_info & node_info, std::error_code & ec) {
    base::xstream_t stream(base::xcontext_t::instance());
    std::string stream_str;
    try {
        node_info.serialize_to(stream);
        stream_str = std::string((char *)stream.data(), stream.size());
    } catch (std::exception const & eh) {
        xerror("xrec_registration_contract: xreg_node_info_serialize_to error");
        ec = system_contracts::error::xsystem_contract_errc_t::serialization_error;
    }

    XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_REG_KEY_SetExecutionTime");
    MAP_SET(XPORPERTY_CONTRACT_REG_KEY, node_info.m_account.value(), stream_str);
}

void xrec_registration_contract::delete_node_info(common::xaccount_address_t const & account) {
    XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_REG_KEY_RemoveExecutionTime");
    REMOVE(enum_type_t::map, XPORPERTY_CONTRACT_REG_KEY, account.value());
}

xreg_node_info xrec_registration_contract::get_node_info(common::xaccount_address_t const & account, std::error_code & ec) const {
    std::string value_str;
    {
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_REG_KEY_GetExecutionTime");
        MAP_GET2(XPORPERTY_CONTRACT_REG_KEY, account.value(), value_str);
    }

    if (value_str.empty()) {
        xdbg("[xrec_registration_contract] account(%s) not exist pid:%d\n", account.c_str(), getpid());
        ec = system_contracts::error::xsystem_contract_errc_t::rec_registration_node_info_not_found;
        return {};
    }

    base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.c_str(), (uint32_t)value_str.size());
    try {
        xreg_node_info node_info;
        node_info.serialize_from(stream);
        return node_info;
    } catch (std::exception const & eh) {
        xerror("xrec_registration_contract: xreg_node_info_serialize_from error");
        ec = system_contracts::error::xsystem_contract_errc_t::deserialization_error;
    }
    return {};
}

void xrec_registration_contract::update_node_refund(common::xaccount_address_t const & account, uint64_t const & refund_amount, std::error_code & ec) {
    auto refund = get_node_refund(account, ec);
    XCONTRACT_ENSURE(!ec, "xrec_registration_contract::setNodeName: node not exist!");

    refund.refund_amount += refund_amount;
    refund.create_time = TIME();

    ec.clear();
    std::string stream_str;
    base::xstream_t stream(base::xcontext_t::instance());
    try {
        refund.serialize_to(stream);
        stream_str = std::string((char *)stream.data(), stream.size());
    } catch (std::exception const & eh) {
        xerror("xrec_registration_contract: xrefund_info_serialize_to error");
        ec = system_contracts::error::xsystem_contract_errc_t::serialization_error;
    }

    XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_REFUND_KEY_SetExecutionTime");
    MAP_SET(XPORPERTY_CONTRACT_REFUND_KEY, account.value(), stream_str);
}

void xrec_registration_contract::delete_node_refund(common::xaccount_address_t const & account) {
    XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_REFUND_KEY_RemoveExecutionTime");
    REMOVE(enum_type_t::map, XPORPERTY_CONTRACT_REFUND_KEY, account.value());
}

xrefund_info xrec_registration_contract::get_node_refund(common::xaccount_address_t const & account, std::error_code & ec) const {
    std::string value_str;
    {
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_REFUND_KEY_GetExecutionTime");
        MAP_GET2(XPORPERTY_CONTRACT_REFUND_KEY, account.value(), value_str);
    }

    if (value_str.empty()) {
        xdbg("[xrec_registration_contract::get_refund] account(%s) not exist pid:%d\n", account.c_str(), getpid());
        ec = system_contracts::error::xsystem_contract_errc_t::rec_registration_node_info_not_found;
        return {};
    }

    base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.c_str(), (uint32_t)value_str.size());
    try {
        xrefund_info refund;
        refund.serialize_from(stream);
        return refund;
    } catch (std::exception const & eh) {
        xerror("xrec_registration_contract: xrefund_info_serialize_from error");
        ec = system_contracts::error::xsystem_contract_errc_t::deserialization_error;
    }
    return {};
}

void xrec_registration_contract::update_batch_stake(std::map<std::string, std::string> const & contract_adv_votes) {
    XMETRICS_TIME_RECORD(XREG_CONTRACT "update_batch_stake_ExecutionTime");
    auto const & source_address = SOURCE_ADDRESS();
    XCONTRACT_ENSURE(base::xvaccount_t::get_addrtype_from_account(source_address) != base::enum_vaccount_addr_type_invalid, "xrec_registration_contract::update_batch_stake: source address invalid");
    xdbg("[xrec_registration_contract::update_batch_stake] src_addr: %s, pid:%d, size: %d\n", source_address.c_str(), getpid(), contract_adv_votes.size());

    std::string base_addr;
    uint32_t table_id;
    if (!data::xdatautil::extract_parts(source_address, base_addr, table_id) || sys_contract_sharding_vote_addr != base_addr) {
        xwarn("[xrec_registration_contract::update_batch_stake] invalid call from %s", source_address.c_str());
        return;
    }

    xstream_t stream(xcontext_t::instance());
    stream << contract_adv_votes;
    std::string contract_adv_votes_str = std::string((const char *)stream.data(), stream.size());
    {
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_TICKETS_KEY_SetExecutionTime");
        MAP_SET(XPORPERTY_CONTRACT_TICKETS_KEY, source_address, contract_adv_votes_str);
    }

    std::map<std::string, std::string> votes_table;
    try {
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_TICKETS_KEY_CopyGetExecutionTime");
        MAP_COPY_GET(XPORPERTY_CONTRACT_TICKETS_KEY, votes_table);
    } catch (std::runtime_error & e) {
        xdbg("[xrec_registration_contract::update_batch_stake] MAP COPY GET error:%s", e.what());
    }

    std::map<std::string, std::string> map_nodes;
    try {
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_REG_KEY_CopyGetExecutionTime");
        MAP_COPY_GET(XPORPERTY_CONTRACT_REG_KEY, map_nodes);
    } catch (std::runtime_error & e) {
        xdbg("[xrec_registration_contract::update_batch_stake] MAP COPY GET error:%s", e.what());
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

        if (!reg_node_info.is_auditor_node()) {
            continue;
        }

        reg_node_info.m_vote_amount = update_adv_votes(account, votes_table);
        // reg_node_info.calc_stake();
        std::error_code ec;
        update_node_info(reg_node_info, ec);
        XCONTRACT_ENSURE(!ec, "xrec_registration_contract::update_batch_stake: node update error!");
    }

    check_and_set_genesis_stage();
}

bool xrec_registration_contract::handle_receive_shard_votes(uint64_t report_time,
                                                            uint64_t last_report_time,
                                                            std::map<std::string, std::string> const & contract_adv_votes,
                                                            std::map<std::string, std::string> & merge_contract_adv_votes) {
    xdbg("[xrec_registration_contract::handle_receive_shard_votes] report vote table size: %d, original vote table size: %d",
         contract_adv_votes.size(),
         merge_contract_adv_votes.size());
    if (report_time < last_report_time) {
        return false;
    }
    if (report_time == last_report_time) {
        merge_contract_adv_votes.insert(contract_adv_votes.begin(), contract_adv_votes.end());
        xdbg("[xrec_registration_contract::handle_receive_shard_votes] same batch of vote report, report vote table size: %d, total size: %d",
             contract_adv_votes.size(),
             merge_contract_adv_votes.size());
    } else {
        merge_contract_adv_votes = contract_adv_votes;
    }
    return true;
}

void xrec_registration_contract::update_batch_stake_v2(uint64_t report_time, std::map<std::string, std::string> const & contract_adv_votes) {
    XMETRICS_TIME_RECORD(XREG_CONTRACT "update_batch_stake_ExecutionTime");
    auto const & source_address = SOURCE_ADDRESS();
    XCONTRACT_ENSURE(base::xvaccount_t::get_addrtype_from_account(source_address) != base::enum_vaccount_addr_type_invalid, "xrec_registration_contract::update_batch_stake_v2: source address invalid");
    xdbg("[xrec_registration_contract::update_batch_stake_v2] src_addr: %s, report_time: %llu, pid:%d, contract_adv_votes size: %d\n",
         source_address.c_str(),
         report_time,
         getpid(),
         contract_adv_votes.size());

    std::string base_addr;
    uint32_t table_id;
    if (!data::xdatautil::extract_parts(source_address, base_addr, table_id) || sys_contract_sharding_vote_addr != base_addr) {
        xwarn("[xrec_registration_contract::update_batch_stake_v2] invalid call from %s", source_address.c_str());
        return;
    }

    if (!MAP_PROPERTY_EXIST(XPORPERTY_CONTRACT_VOTE_REPORT_TIME_KEY)) {
        MAP_CREATE(XPORPERTY_CONTRACT_VOTE_REPORT_TIME_KEY);
        MAP_SET(XPORPERTY_CONTRACT_VOTE_REPORT_TIME_KEY, source_address, base::xstring_utl::tostring(0));
    }
    bool replace = true;
    std::string value_str;
    uint64_t last_report_time = 0;
    MAP_GET2(XPORPERTY_CONTRACT_VOTE_REPORT_TIME_KEY, source_address, value_str);
    if (!value_str.empty()) {
        last_report_time = base::xstring_utl::touint64(value_str);
        xdbg("[xrec_registration_contract::update_batch_stake_v2] last_report_time: %llu", last_report_time);
    }
    MAP_SET(XPORPERTY_CONTRACT_VOTE_REPORT_TIME_KEY, source_address, base::xstring_utl::tostring(report_time));

    {
        std::map<std::string, std::string> auditor_votes;
        std::string auditor_votes_str;
        MAP_GET2(XPORPERTY_CONTRACT_TICKETS_KEY, source_address, auditor_votes_str);
        if (!auditor_votes_str.empty()) {
            base::xstream_t votes_stream(base::xcontext_t::instance(), (uint8_t *)auditor_votes_str.c_str(), (uint32_t)auditor_votes_str.size());
            votes_stream >> auditor_votes;
        }
        if (!handle_receive_shard_votes(report_time, last_report_time, contract_adv_votes, auditor_votes)) {
            XCONTRACT_ENSURE(false, "[xrec_registration_contract::on_receive_shard_votes_v2] handle_receive_shard_votes fail");
        }

        xstream_t stream(xcontext_t::instance());
        stream << auditor_votes;
        std::string contract_adv_votes_str = std::string((const char *)stream.data(), stream.size());
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_TICKETS_KEY_SetExecutionTime");
        MAP_SET(XPORPERTY_CONTRACT_TICKETS_KEY, source_address, contract_adv_votes_str);
    }

    std::map<std::string, std::string> votes_table;
    try {
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_TICKETS_KEY_CopyGetExecutionTime");
        MAP_COPY_GET(XPORPERTY_CONTRACT_TICKETS_KEY, votes_table);
    } catch (std::runtime_error & e) {
        xdbg("[xrec_registration_contract::update_batch_stake_v2] MAP COPY GET error:%s", e.what());
    }

    std::map<std::string, std::string> map_nodes;
    try {
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_REG_KEY_CopyGetExecutionTime");
        MAP_COPY_GET(XPORPERTY_CONTRACT_REG_KEY, map_nodes);
    } catch (std::runtime_error & e) {
        xdbg("[xrec_registration_contract::update_batch_stake_v2] MAP COPY GET error:%s", e.what());
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
        std::error_code ec;
        update_node_info(reg_node_info, ec);
        XCONTRACT_ENSURE(!ec, "xrec_registration_contract::update_batch_stake_v2: node update error!");
    }

    check_and_set_genesis_stage();
}

void xrec_registration_contract::check_and_set_genesis_stage() {
    std::string value_str;
    xactivation_record record;

    {
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_GENESIS_STAGE_KEY_GetExecutionTime");
        value_str = STRING_GET(XPORPERTY_CONTRACT_GENESIS_STAGE_KEY);
    }

    if (!value_str.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.c_str(), (uint32_t)value_str.size());
        record.serialize_from(stream);
    }
    if (record.activated) {
        xinfo("[xrec_registration_contract::check_and_set_genesis_stage] activated: %d, activation_time: %llu, pid:%d\n", record.activated, record.activation_time, getpid());
        return;
    }

    std::map<std::string, std::string> map_nodes;
    try {
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_REG_KEY_CopyGetExecutionTime");
        MAP_COPY_GET(XPORPERTY_CONTRACT_REG_KEY, map_nodes);
    } catch (std::runtime_error & e) {
        xdbg("[xrec_registration_contract::check_and_set_genesis_stage] MAP COPY GET error:%s", e.what());
    }
    bool active = check_registered_nodes_active(map_nodes);
    if (active) {
        record.activated = 1;
        record.activation_time = TIME();
    }

    base::xstream_t stream(base::xcontext_t::instance());
    record.serialize_to(stream);
    value_str = std::string((char *)stream.data(), stream.size());
    {
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_GENESIS_STAGE_KEY_SetExecutionTime");
        STRING_SET(XPORPERTY_CONTRACT_GENESIS_STAGE_KEY, value_str);
    }
}

xproperty_instruction_t xrec_registration_contract::handleNodeType(const std::string & node_types, const data::xproperty_asset & asset_out, xreg_node_info & node_info) {
    common::xrole_type_t role_type = common::to_role_type(node_types);
    XCONTRACT_ENSURE(role_type != common::xrole_type_t::invalid, "xrec_registration_contract::handleNodeType: invalid node_type!");
    XCONTRACT_ENSURE(role_type != node_info.m_registered_role, "xrec_registration_contract::handleNodeType: node_types can not be same!");
    // update params
    node_info.m_account_mortgage += asset_out.m_amount;
    node_info.m_registered_role = role_type;
    uint64_t min_deposit = node_info.get_required_min_deposit();
    xdbg(("[xrec_registration_contract::handleNodeType] min_deposit: %" PRIu64 ", account: %s, account morgage: %llu\n"),
         min_deposit,
         node_info.m_account.c_str(),
         node_info.m_account_mortgage);
    XCONTRACT_ENSURE(node_info.m_account_mortgage >= min_deposit, "xrec_registration_contract::handleNodeType: deposit not enough");
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

    return create_binlog(XPORPERTY_CONTRACT_REG_KEY, xproperty_cmd_type_map_set, node_info.m_account.value(), node_info);
}

void xrec_registration_contract::updateNodeType(const std::string & node_types) {
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "updateNodeType_Called", 1);
    XMETRICS_TIME_RECORD(XREG_CONTRACT "updateNodeType_ExecutionTime");
    // check params
    auto const & account = common::xaccount_address_t{SOURCE_ADDRESS()};
    XCONTRACT_ENSURE(!account.empty(), "xrec_registration_contract::updateNodeType: account must be not empty");
    XCONTRACT_ENSURE(base::xvaccount_t::get_addrtype_from_account(account.value()) != base::enum_vaccount_addr_type_invalid, "xrec_registration_contract::updateNodeType: source address invalid");
    std::error_code ec;
    auto node_info = get_node_info(account, ec);
    XCONTRACT_ENSURE(!ec, "xrec_registration_contract::updateNodeType: node not exist!");
    xdbg("[xrec_registration_contract::updateNodeType] pid: %d, balance: %lld, account: %s, node_types: %s\n", getpid(), GET_BALANCE(), account.c_str(), node_types.c_str());
    const xtransaction_ptr_t trans_ptr = GET_TRANSACTION();
    XCONTRACT_ENSURE(trans_ptr->get_source_action().get_action_type() == xaction_type_asset_out, "xrec_registration_contract::updateNodeType: source_action type must be xaction_type_asset_out");
    data::xproperty_asset asset_out{0};
    if (trans_ptr->get_source_action().get_action_param().size() > 0) {
        xstream_t stream(xcontext_t::instance(), (uint8_t *)trans_ptr->get_source_action().get_action_param().data(), trans_ptr->get_source_action().get_action_param().size());
        stream >> asset_out.m_token_name;
        stream >> asset_out.m_amount;
    }
    auto instruction = handleNodeType(node_types, asset_out, node_info);
    execute_instruction(instruction);
    check_and_set_genesis_stage();
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "updateNodeType_Executed", 1);
}

void xrec_registration_contract::stakeDeposit() {
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "stakeDeposit_Called", 1);
    XMETRICS_TIME_RECORD(XREG_CONTRACT "stakeDeposit_ExecutionTime");
    auto const & account = common::xaccount_address_t{SOURCE_ADDRESS()};
    XCONTRACT_ENSURE(base::xvaccount_t::get_addrtype_from_account(account.value()) != base::enum_vaccount_addr_type_invalid, "xrec_registration_contract::stakeDeposit: source address invalid");
    std::error_code ec;
    auto node_info = get_node_info(account, ec);
    XCONTRACT_ENSURE(!ec, "xrec_registration_contract::stakeDeposit: node not exist!");

    const xtransaction_ptr_t trans_ptr = GET_TRANSACTION();
    xstream_t stream(xcontext_t::instance(), (uint8_t *)trans_ptr->get_source_action().get_action_param().data(), trans_ptr->get_source_action().get_action_param().size());
    data::xproperty_asset asset_out{0};
    stream >> asset_out.m_token_name;
    stream >> asset_out.m_amount;
    xdbg("[xrec_registration_contract::stakeDeposit] pid: %d, balance: %lld, account: %s, stake deposit: %llu\n", getpid(), GET_BALANCE(), account.c_str(), asset_out.m_amount);
    XCONTRACT_ENSURE(asset_out.m_amount != 0, "xrec_registration_contract::stakeDeposit: stake deposit can not be zero");

    node_info.m_account_mortgage += asset_out.m_amount;
    ec.clear();
    update_node_info(node_info, ec);
    XCONTRACT_ENSURE(!ec, "xrec_registration_contract::stakeDeposit: node update error!");
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "stakeDeposit_Executed", 1);
}

void xrec_registration_contract::unstakeDeposit(uint64_t unstake_deposit) {
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "unstakeDeposit_Called", 1);
    XMETRICS_TIME_RECORD(XREG_CONTRACT "unstakeDeposit_ExecutionTime");
    auto const & account = common::xaccount_address_t{SOURCE_ADDRESS()};
    XCONTRACT_ENSURE(base::xvaccount_t::get_addrtype_from_account(account.value()) != base::enum_vaccount_addr_type_invalid, "xrec_registration_contract::unstakeDeposit: source address invalid");
    std::error_code ec;
    auto node_info = get_node_info(account, ec);
    XCONTRACT_ENSURE(!ec, "xrec_registration_contract::unstakeDeposit: node not exist!");

    xdbg("[xrec_registration_contract::unstakeDeposit] pid: %d, balance: %lld, account: %s, unstake deposit: %llu, account morgage: %llu\n",
         getpid(),
         GET_BALANCE(),
         account.c_str(),
         unstake_deposit,
         node_info.m_account_mortgage);
    XCONTRACT_ENSURE(unstake_deposit != 0, "xrec_registration_contract::unstakeDeposit: unstake deposit can not be zero");

    uint64_t min_deposit = node_info.get_required_min_deposit();
    XCONTRACT_ENSURE(unstake_deposit <= node_info.m_account_mortgage && node_info.m_account_mortgage - unstake_deposit >= min_deposit,
                     "xrec_registration_contract::unstakeDeposit: unstake deposit too big");
    node_info.m_account_mortgage -= unstake_deposit;
    TRANSFER(account.value(), unstake_deposit);
    ec.clear();
    update_node_info(node_info, ec);
    XCONTRACT_ENSURE(!ec, "xrec_registration_contract::unstakeDeposit: node update error!");
    XMETRICS_COUNTER_INCREMENT(XREG_CONTRACT "unstakeDeposit_Executed", 1);
}

xslash_info xrec_registration_contract::get_slash_info(common::xaccount_address_t const & account, std::error_code & ec) const {
    std::string value_str;
    {
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPROPERTY_CONTRACT_SLASH_INFO_KEY_GetExecutionTime");
        MAP_GET2(XPROPERTY_CONTRACT_SLASH_INFO_KEY, account.value(), value_str);
    }
    if (value_str.empty()) {
        xdbg("[xrec_registration_contract][get_slash_info] account(%s) not exist,  pid:%d\n", account.c_str(), getpid());
        ec = system_contracts::error::xsystem_contract_errc_t::rec_registration_node_info_not_found;
        return {};
    }

    xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
    try {
        xslash_info node_slash_info;
        node_slash_info.serialize_from(stream);
        return node_slash_info;
    } catch (std::exception const & eh) {
        xerror("xrec_registration_contract: xslash_info_serialize_from error");
        ec = system_contracts::error::xsystem_contract_errc_t::deserialization_error;
    }
    return {};
}

void xrec_registration_contract::update_slash_info(common::xaccount_address_t const & account, xslash_info const & s_info, std::error_code & ec) {
    std::string stream_str;
    base::xstream_t stream(base::xcontext_t::instance());
    try {
        s_info.serialize_to(stream);
        stream_str = std::string((char *)stream.data(), stream.size());
    } catch (std::exception const & eh) {
        xerror("xrec_registration_contract: xslash_info_serialize_to error");
        ec = system_contracts::error::xsystem_contract_errc_t::serialization_error;
    }

    XMETRICS_TIME_RECORD(XREG_CONTRACT "XPROPERTY_CONTRACT_SLASH_INFO_KEY_SetExecutionTime");
    MAP_SET(XPROPERTY_CONTRACT_SLASH_INFO_KEY, account.value(), stream_str);
}

void xrec_registration_contract::slash_staking_time(common::xaccount_address_t const & account) {
    auto current_time = TIME();
    std::error_code ec;
    auto s_info = get_slash_info(account, ec);
    XCONTRACT_ENSURE(!ec, "xrec_registration_contract::slash_staking_time: node not exist!");
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

    ec.clear();
    update_slash_info(account, s_info, ec);
    XCONTRACT_ENSURE(!ec, "xrec_registration_contract::slash_staking_time: node update error!");
}

void xrec_registration_contract::slash_unqualified_node(std::string const & punish_node_str) {
    XMETRICS_TIME_RECORD(XREG_CONTRACT "slash_unqualified_node_ExecutionTime");
    auto const & account = SELF_ADDRESS();
    auto const & source_addr = SOURCE_ADDRESS();
    XCONTRACT_ENSURE(base::xvaccount_t::get_addrtype_from_account(account.value()) != base::enum_vaccount_addr_type_invalid, "xrec_registration_contract::slash_unqualified_node: source address invalid");

    std::string base_addr = "";
    uint32_t table_id = 0;
    XCONTRACT_ENSURE(data::xdatautil::extract_parts(source_addr, base_addr, table_id), "source address extract base_addr or table_id error!");
    xdbg("[xrec_registration_contract][slash_unqualified_node] self_account %s, source_addr %s, base_addr %s\n", account.c_str(), source_addr.c_str(), base_addr.c_str());

    XCONTRACT_ENSURE(source_addr == top::sys_contract_zec_slash_info_addr, "invalid source addr's call!");

    std::vector<xaction_node_info_t> node_slash_info;
    base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)punish_node_str.data(), punish_node_str.size());
    VECTOR_OBJECT_DESERIALZE2(stream, node_slash_info);
    xinfo("[xrec_registration_contract][slash_unqualified_node] do slash unqualified node, size: %zu", node_slash_info.size());

    for (auto const & value : node_slash_info) {
        auto const & addr = value.node_id;
        xdbg("[xrec_registration_contract][slash_unqualified_node] do slash unqualified node, addr: %s", addr.c_str());
        std::error_code ec;
        auto reg_node = get_node_info(addr, ec);
        if (!ec) {
            xwarn("[xrec_registration_contract][slash_unqualified_node] get reg node_info error, account: %s", addr.c_str());
            continue;
        }

        if (value.action_type) {  // punish
            xkinfo("[xrec_registration_contract][slash_unqualified_node] effective slash credit & staking time, node addr: %s", addr.c_str());
            XMETRICS_PACKET_INFO("sysContract_zecSlash", "effective slash credit & staking time, node addr", addr.value());
            reg_node.slash_credit_score(value.node_type);
            slash_staking_time(addr);
        } else {
            xkinfo("[xrec_registration_contract][slash_unqualified_node] effective award credit, node addr: %s", addr.c_str());
            XMETRICS_PACKET_INFO("sysContract_zecSlash", "effective award credit, node addr", addr.value())
            reg_node.award_credit_score(value.node_type);
        }

        ec.clear();
        update_node_info(reg_node, ec);
        XCONTRACT_ENSURE(!ec, "xrec_registration_contract::slash_unqualified_node: node update error!");
    }
}

bool xrec_registration_contract::is_valid_name(const std::string & nickname) const {
    int len = nickname.length();
    if (len < 4 || len > 16) {
        return false;
    }

    return std::find_if(nickname.begin(), nickname.end(), [](unsigned char c) { return !std::isalnum(c) && c != '_'; }) == nickname.end();
};

xproperty_instruction_t xrec_registration_contract::create_binlog(std::string name,
                                                                  data::xproperty_op_code_t op_code,
                                                                  std::string op_para1,
                                                                  xreg_node_info op_para2) {
    base::xstream_t stream(base::xcontext_t::instance());
    op_para2.serialize_to(stream);
    std::string value = std::string((char *)stream.data(), stream.size());
    xproperty_instruction_t instruction{name, op_code, op_para1, value};
    return instruction;
}

void xrec_registration_contract::execute_instruction(xproperty_instruction_t instruction){
    if(instruction.op_code == xproperty_cmd_type_map_set){
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_TICKETS_KEY_SetExecutionTime");
        MAP_SET(instruction.name, instruction.op_para1, instruction.op_para2);
    } else if(instruction.op_code == xproperty_cmd_type_map_remove) {
        std::string debug_str = instruction.name + "XPORPERTY_CONTRACT_TICKETS_KEY_RemoveExecutionTime";
        XMETRICS_TIME_RECORD(XREG_CONTRACT "XPORPERTY_CONTRACT_TICKETS_KEY_SetExecutionTime");
        REMOVE(enum_type_t::map, instruction.name, instruction.op_para1);
    }
}

NS_END2

#undef XREG_CONTRACT
#undef XCONTRACT_PREFIX
#undef XZEC_MODULE

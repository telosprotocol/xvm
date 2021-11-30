// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xreward/xtable_reward_claiming_contract_new.h"

#include "xchain_upgrade/xchain_data_processor.h"
#include "xcommon/xlegacy_account_address.h"
#include "xdata/xnative_contract_address.h"
#include "xmetrics/xmetrics.h"
#include "xvm/xerror/xvm_error_code.h"

#ifndef XSYSCONTRACT_MODULE
#    define XSYSCONTRACT_MODULE "SysContract_"
#endif
#define XCONTRACT_PREFIX "TableRewardClaiming_"
#define XTABLE_REWARD_CLAIMING XSYSCONTRACT_MODULE XCONTRACT_PREFIX

NS_BEG2(top, system_contracts)

void xtop_table_reward_claiming_contract_new::setup() {
    const int old_tables_count = 256;
    uint32_t table_id = 0;
    if (!xdatautil::extract_table_id_from_address(recver().value(), table_id)) {
        xwarn("[xtable_reward_claiming_contract_new_t::setup] EXTRACT_TABLE_ID failed, node reward pid: %d, account: %s", getpid(), recver().c_str());
        return;
    }
    xdbg("[xtable_reward_claiming_contract_new_t::setup] table id: %d", table_id);

    uint64_t acc_token = 0;
    uint32_t acc_token_decimals = 0;
    for (size_t i = 1; i <= m_voter_dividend_reward_prop.size(); i++) {
        auto voter_dividend_reward_prop_str = std::string{xstake::XPORPERTY_CONTRACT_VOTER_DIVIDEND_REWARD_KEY_BASE} + "-" + std::to_string(i);
        auto & voter_dividend_reward_prop = read_voter_dividend_reward_property_to_set(i);
        // MAP_CREATE(property);
        for (auto j = 0; j < old_tables_count; j++) {
            auto table_addr = std::string{sys_contract_sharding_reward_claiming_addr} + "@" + base::xstring_utl::tostring(j);
            std::vector<std::pair<std::string, std::string>> db_kv_121;
            chain_data::xchain_data_processor_t::get_stake_map_property(common::xlegacy_account_address_t{table_addr}, voter_dividend_reward_prop_str, db_kv_121);
            for (auto const & _p : db_kv_121) {
                base::xvaccount_t vaccount{_p.first};
                auto account_table_id = vaccount.get_ledger_subaddr();
                if (static_cast<uint16_t>(account_table_id) != static_cast<uint16_t>(table_id)) {
                    continue;
                }
                xstake::xreward_record record;
                auto detail = _p.second;
                base::xstream_t stream{base::xcontext_t::instance(), (uint8_t *)detail.data(), static_cast<uint32_t>(detail.size())};
                record.serialize_from(stream);
                // MAP_SET(property, _p.first, _p.second);
                voter_dividend_reward_prop.set(_p.first, _p.second);
                acc_token += static_cast<uint64_t>(record.unclaimed / xstake::REWARD_PRECISION);
                acc_token_decimals += static_cast<uint32_t>(record.unclaimed % xstake::REWARD_PRECISION);
            }
        }
    }

    // MAP_CREATE(xstake::XPORPERTY_CONTRACT_NODE_REWARD_KEY);
    for (auto i = 0; i < old_tables_count; i++) {
        auto table_addr = std::string{sys_contract_sharding_reward_claiming_addr} + "@" + base::xstring_utl::tostring(i);
        std::vector<std::pair<std::string, std::string>> db_kv_124;
        chain_data::xchain_data_processor_t::get_stake_map_property(common::xlegacy_account_address_t{table_addr}, xstake::XPORPERTY_CONTRACT_NODE_REWARD_KEY, db_kv_124);
        for (auto const & _p : db_kv_124) {
            base::xvaccount_t vaccount{_p.first};
            auto account_table_id = vaccount.get_ledger_subaddr();
            if (static_cast<uint16_t>(account_table_id) != static_cast<uint16_t>(table_id)) {
                continue;
            }
            // MAP_SET(XPORPERTY_CONTRACT_NODE_REWARD_KEY, _p.first, _p.second);
            m_node_reward_prop.set(_p.first, _p.second);
            xstake::xreward_node_record record;
            auto detail = _p.second;
            base::xstream_t stream{base::xcontext_t::instance(), (uint8_t *)detail.data(), static_cast<uint32_t>(detail.size())};
            record.serialize_from(stream);
            acc_token += static_cast<uint64_t>(record.m_unclaimed / xstake::REWARD_PRECISION);
            acc_token_decimals += static_cast<uint32_t>(record.m_unclaimed % xstake::REWARD_PRECISION);
        }
    }

    acc_token += (acc_token_decimals / xstake::REWARD_PRECISION);
    if (acc_token_decimals % xstake::REWARD_PRECISION != 0) {
        acc_token += 1;
    }
    deposit(state_accessor::xtoken_t(acc_token));
}

contract_common::properties::xmap_property_t<std::string, std::string> & xtop_table_reward_claiming_contract_new::read_voter_dividend_reward_property_to_set(uint32_t index) {
    if (index > m_voter_dividend_reward_prop.size() || index <= 0) {
        top::error::throw_error(xvm::enum_xvm_error_code::query_contract_data_property_missing);
    }

    return m_voter_dividend_reward_prop[index];
}

contract_common::properties::xmap_property_t<std::string, std::string> const & xtop_table_reward_claiming_contract_new::get_voter_dividend_reward_property(uint32_t index) const {
    if (index > m_voter_dividend_reward_prop.size() || index <= 0) {
        top::error::throw_error(xvm::enum_xvm_error_code::query_contract_data_property_missing);
    }

    return m_voter_dividend_reward_prop[index];
}

void xtop_table_reward_claiming_contract_new::update_vote_reward_record(common::xaccount_address_t const & account, xstake::xreward_record const & record) {
    uint32_t sub_map_no = (utl::xxh32_t::digest(account.to_string()) % m_voter_dividend_reward_prop.size()) + 1;
    auto & voter_dividend_reward_prop = read_voter_dividend_reward_property_to_set(sub_map_no);

    base::xstream_t stream(base::xcontext_t::instance());
    record.serialize_to(stream);
    auto value_str = std::string((char *)stream.data(), stream.size());
    voter_dividend_reward_prop.set(account.to_string(), value_str);
}

void xtop_table_reward_claiming_contract_new::recv_voter_dividend_reward(const common::xlogic_time_t current_time, std::map<std::string, top::xstake::uint128_t> const & rewards) {
    XMETRICS_TIME_RECORD(XTABLE_REWARD_CLAIMING "recv_voter_dividend_reward");
    auto const & source_address = sender();
    auto const & self_address = recver();

    xinfo("[xtable_reward_claiming_contract_new_t::recv_voter_dividend_reward] self address: %s, source address: %s, issuance_clock_height: %llu",
          self_address.c_str(),
          source_address.c_str(),
          current_time);

    if (rewards.size() == 0) {
        xwarn("[xtable_reward_claiming_contract_new_t::recv_voter_dividend_reward] rewards size 0");
        return;
    }

    XCONTRACT_ENSURE(sys_contract_zec_reward_addr == source_address.value(),
                     "xtable_reward_claiming_contract_new_t::recv_voter_dividend_reward from invalid address: " + source_address.value());
    auto table_id = self_address.table_id();
    auto adv_votes_bytes = get_property<contract_common::properties::xmap_property_t<std::string, std::string>>(
        state_accessor::properties::xtypeless_property_identifier_t{xstake::XPORPERTY_CONTRACT_POLLABLE_KEY},
        common::xaccount_address_t{common::xaccount_base_address_t::build_from(sys_contract_sharding_vote_addr), table_id});
    auto adv_votes = adv_votes_bytes.value();

    for (size_t i = 1; i <= m_voter_dividend_reward_prop.size(); ++i) {
        auto votes_property_name = std::string{xstake::XPORPERTY_CONTRACT_VOTES_KEY_BASE} + "-" + std::to_string(i);
        auto voters_bytes = get_property<contract_common::properties::xmap_property_t<std::string, std::string>>(
            state_accessor::properties::xtypeless_property_identifier_t{votes_property_name}, common::xaccount_address_t{common::xaccount_base_address_t::build_from(sys_contract_sharding_vote_addr), table_id});
        auto voters = voters_bytes.value();
        xdbg("[xtable_reward_claiming_contract_new_t::recv_voter_dividend_reward] vote maps %s size: %d", votes_property_name.c_str(), voters.size());
        // calc_voter_reward(voters);
        for (auto const & entity : voters) {
            auto const & account = entity.first;
            auto const & vote_table_str = entity.second;
            base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)vote_table_str.c_str(), (uint32_t)vote_table_str.size());
            std::map<std::string, uint64_t> votes_table;
            stream >> votes_table;
            xdbg("[xtable_reward_claiming_contract_new_t::recv_voter_dividend_reward] voter: %s", account.c_str());
            auto record = get_vote_reward_record(common::xaccount_address_t{account}); // not care return value hear
            add_voter_reward(current_time, votes_table, rewards, adv_votes, record);
            update_vote_reward_record(common::xaccount_address_t{account}, record);
        }
    }
}

void xtop_table_reward_claiming_contract_new::add_voter_reward(const common::xlogic_time_t current_time,
                                                               std::map<std::string, uint64_t> & votes_table,
                                                               std::map<std::string, top::xstake::uint128_t> const & rewards,
                                                               std::map<std::string, std::string> const & adv_votes,
                                                               xstake::xreward_record & record) {
    top::xstake::uint128_t node_vote_reward = 0;
    top::xstake::uint128_t voter_node_reward = 0;
    uint64_t node_total_votes = 0;
    uint64_t voter_node_votes = 0;
    record.issue_time = current_time;
    for (auto const & adv_vote : votes_table) {
        auto const & adv = adv_vote.first;
        auto iter = rewards.find(adv);
        // account total rewards
        if (iter != rewards.end()) {
            node_vote_reward = iter->second;
            //node_vote_reward = static_cast<xuint128_t>(iter->second.first * xstake::REWARD_PRECISION) + iter->second.second;
        } else {
            node_vote_reward = 0;
            continue;
        }
        // account total votes
        auto iter2 = adv_votes.find(adv);
        if (iter2 != adv_votes.end()) {
            node_total_votes = base::xstring_utl::touint64(iter2->second);
        } else {
            node_total_votes = 0;
            continue;
        }
        // voter votes
        voter_node_votes = votes_table[adv];
        // voter reward
        voter_node_reward = node_vote_reward * voter_node_votes / node_total_votes;
        // add to property
        bool found = false;
        for (auto & node_reward : record.node_rewards) {
            if (node_reward.account == adv) {
                found = true;
                node_reward.accumulated += voter_node_reward;
                node_reward.unclaimed += voter_node_reward;
                node_reward.issue_time = current_time;
                break;
            }
        }
        if (!found) {
            xstake::node_record_t voter_node_record;
            voter_node_record.account = adv;
            voter_node_record.accumulated = voter_node_reward;
            voter_node_record.unclaimed = voter_node_reward;
            voter_node_record.issue_time = current_time;
            record.node_rewards.push_back(voter_node_record);
        }
        record.accumulated += voter_node_reward;
        record.unclaimed += voter_node_reward;
        xdbg(
            "[xtable_reward_claiming_contract_new_t::recv_voter_dividend_reward] adv node: %s, node_vote_reward: [%llu, %u], node_total_votes: %llu, voter_node_votes: "
            "%llu, voter_node_reward: [%llu, %u]",
            adv.c_str(),
            static_cast<uint64_t>(node_vote_reward / xstake::REWARD_PRECISION),
            static_cast<uint32_t>(node_vote_reward % xstake::REWARD_PRECISION),
            node_total_votes,
            voter_node_votes,
            static_cast<uint64_t>(voter_node_reward / xstake::REWARD_PRECISION),
            static_cast<uint32_t>(voter_node_reward % xstake::REWARD_PRECISION));
    }
}

void xtop_table_reward_claiming_contract_new::claimVoterDividend() {
    XMETRICS_TIME_RECORD(XTABLE_REWARD_CLAIMING "claim_voter_dividend");

    auto account = sender();
    auto reward_record = get_vote_reward_record(account);
    auto cur_time = time();
    xinfo("[xtable_reward_claiming_contract_new_t::claimVoterDividend] balance:%llu, account: %s, cur_time: %llu, last_claim_time: %llu\n",
          balance(),
          account.c_str(),
          cur_time,
          reward_record.last_claim_time);
    auto min_voter_dividend = XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_voter_dividend);
    XCONTRACT_ENSURE(reward_record.unclaimed > min_voter_dividend, "claimVoterDividend no enough reward");

    // transfer to account
    xinfo("[xtable_reward_claiming_contract_new_t::claimVoterDividend] timer round: %" PRIu64 ", account: %s, reward:: [%llu, %u], pid:%d\n",
          cur_time,
          account.c_str(),
          static_cast<uint64_t>(reward_record.unclaimed / xstake::REWARD_PRECISION),
          static_cast<uint32_t>(reward_record.unclaimed % xstake::REWARD_PRECISION),
          getpid());
    XMETRICS_PACKET_INFO(XTABLE_REWARD_CLAIMING "claim_voter_dividend",
                         "timer round",
                         std::to_string(cur_time),
                         "source address",
                         account.c_str(),
                         "reward",
                         std::to_string(static_cast<uint64_t>(reward_record.unclaimed / xstake::REWARD_PRECISION)));

    transfer(account, static_cast<uint64_t>(reward_record.unclaimed / xstake::REWARD_PRECISION), contract_common::xfollowup_transaction_schedule_type_t::immediately);
    reward_record.unclaimed = reward_record.unclaimed % xstake::REWARD_PRECISION;
    reward_record.last_claim_time = cur_time;
    for (auto & node_reward : reward_record.node_rewards) {
        node_reward.unclaimed = 0;
        node_reward.last_claim_time = cur_time;
    }
    update_vote_reward_record(account, reward_record);
}

void xtop_table_reward_claiming_contract_new::update_working_reward_record(common::xaccount_address_t const & account, xstake::xreward_node_record const & record) {
    base::xstream_t stream(base::xcontext_t::instance());
    record.serialize_to(stream);
    auto value_str = std::string((char *)stream.data(), stream.size());
    m_node_reward_prop.set(account.to_string(), value_str);
}

void xtop_table_reward_claiming_contract_new::update(common::xaccount_address_t const & node_account, const common::xlogic_time_t current_time, top::xstake::uint128_t reward) {
    auto const & self_address = recver();
    xdbg("[xtable_reward_claiming_contract_new_t::update] self_address: %s, account: %s, reward: [%llu, %u]",
         self_address.c_str(),
         node_account.c_str(),
         static_cast<uint64_t>(reward / xstake::REWARD_PRECISION),
         static_cast<uint32_t>(reward % xstake::REWARD_PRECISION));

    // update node rewards table
    auto value_str = m_node_reward_prop.get(node_account.to_string());
    xstake::xreward_node_record record;
    if (!value_str.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.c_str(), (uint32_t)value_str.size());
        record.serialize_from(stream);
    }

    record.m_accumulated += reward;
    record.m_unclaimed += reward;
    record.m_issue_time = current_time;

    base::xstream_t stream(base::xcontext_t::instance());
    record.serialize_to(stream);
    value_str = std::string((char *)stream.data(), stream.size());
    m_node_reward_prop.set(node_account.to_string(), value_str);
}

void xtop_table_reward_claiming_contract_new::recv_node_reward(const common::xlogic_time_t current_time, std::map<std::string, top::xstake::uint128_t> const & rewards) {
    XMETRICS_TIME_RECORD(XTABLE_REWARD_CLAIMING "recv_node_reward");

    auto const & source_address = sender();
    auto const & self_address = recver();

    xinfo("[xtable_reward_claiming_contract_new_t::recv_node_reward] source_address: %s, self_address: %s, current_time:%llu, rewards size: %d",
          source_address.c_str(),
          self_address.c_str(),
          current_time,
          rewards.size());

    XCONTRACT_ENSURE(sys_contract_zec_reward_addr == source_address.value(),
                     "[xtable_reward_claiming_contract_new_t::recv_node_reward] working reward is not from zec workload contract but from " + source_address.value());

    for (auto const & account_reward : rewards) {
        auto const & account = account_reward.first;
        auto const & reward = account_reward.second;

        xinfo("[xtable_reward_claiming_contract_new_t::recv_node_reward] account: %s, reward: [%llu, %u]\n",
              account.c_str(),
              static_cast<uint64_t>(reward / xstake::REWARD_PRECISION),
              static_cast<uint32_t>(reward % xstake::REWARD_PRECISION));

        // update node rewards
        update(common::xaccount_address_t{account}, current_time, reward);
    }
}

void xtop_table_reward_claiming_contract_new::claimNodeReward() {
    XMETRICS_TIME_RECORD(XTABLE_REWARD_CLAIMING "claim_node_reward");

    auto account = sender();
    auto reward_record = get_working_reward_record(account);
    auto cur_time = time();
    xinfo("[xtable_reward_claiming_contract_new_t::claimNodeReward] balance: %" PRIu64 ", account: %s, cur_time: %" PRIu64, balance(), account.c_str(), cur_time);
    auto min_node_reward = XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_node_reward);
    // transfer to account
    xinfo("[xtable_reward_claiming_contract_new_t::claimNodeReward] reward: [%" PRIu64 ", %" PRIu32 "], reward_str: %s, reward_upper: %" PRIu64 ", reward_lower: %" PRIu64
          ", last_claim_time: %" PRIu64 ", min_node_reward: %" PRIu64,
          static_cast<uint64_t>(reward_record.m_unclaimed / xstake::REWARD_PRECISION),
          static_cast<uint32_t>(reward_record.m_unclaimed % xstake::REWARD_PRECISION),
          reward_record.m_unclaimed.str().c_str(),
          reward_record.m_unclaimed.upper(),
          reward_record.m_unclaimed.lower(),
          reward_record.m_last_claim_time,
          min_node_reward);
    XCONTRACT_ENSURE(static_cast<uint64_t>(reward_record.m_unclaimed / xstake::REWARD_PRECISION) > min_node_reward, "claimNodeReward: node no enough reward");
    XMETRICS_PACKET_INFO(XTABLE_REWARD_CLAIMING "claim_node_reward",
                         "timer round",
                         std::to_string(cur_time),
                         "source address",
                         account.c_str(),
                         "reward",
                         std::to_string(static_cast<uint64_t>(reward_record.m_unclaimed / xstake::REWARD_PRECISION)));

    transfer(account, static_cast<uint64_t>(reward_record.m_unclaimed / xstake::REWARD_PRECISION), contract_common::xfollowup_transaction_schedule_type_t::immediately);
    reward_record.m_unclaimed -= reward_record.m_unclaimed / xstake::REWARD_PRECISION * xstake::REWARD_PRECISION;
    reward_record.m_last_claim_time = cur_time;
    update_working_reward_record(account, reward_record);
}

xstake::xreward_node_record xtop_table_reward_claiming_contract_new::get_working_reward_record(common::xaccount_address_t const & account) const {
    xstake::xreward_node_record record;
    auto value_str = m_node_reward_prop.get(account.to_string());
    if (!value_str.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.c_str(), (uint32_t)value_str.size());
        record.serialize_from(stream);
    }
    return record;
}

xstake::xreward_record xtop_table_reward_claiming_contract_new::get_vote_reward_record(common::xaccount_address_t const & account) const {
    xstake::xreward_record record;
    uint32_t sub_map_no = (utl::xxh32_t::digest(account.to_string()) % m_voter_dividend_reward_prop.size()) + 1;
    auto const & voter_dividend_reward_prop = get_voter_dividend_reward_property(sub_map_no);
    auto value_str = voter_dividend_reward_prop.get(account.to_string());

    if (!value_str.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.c_str(), (uint32_t)value_str.size());
        record.serialize_from(stream);
    }
    return record;
}

NS_END2

#undef XTABLE_REWARD_CLAIMING
#undef XCONTRACT_PREFIX
#undef XSYSCONTRACT_MODULE

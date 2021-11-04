// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xcontract_common/xproperties/xproperty_map.h"
#include "xstake/xstake_algorithm.h"
#include "xsystem_contract_runtime/xsystem_contract_runtime_helper.h"
#include "xsystem_contracts/xbasic_system_contract.h"

NS_BEG2(top, system_contracts)

class xtop_table_reward_claiming_contract_new : public xbasic_system_contract_t {
    using xbase_t = xbasic_system_contract_t;

public:
    xtop_table_reward_claiming_contract_new() = default;
    xtop_table_reward_claiming_contract_new(xtop_table_reward_claiming_contract_new const &) = delete;
    xtop_table_reward_claiming_contract_new & operator=(xtop_table_reward_claiming_contract_new const &) = delete;
    xtop_table_reward_claiming_contract_new(xtop_table_reward_claiming_contract_new &&) = default;
    xtop_table_reward_claiming_contract_new & operator=(xtop_table_reward_claiming_contract_new &&) = default;
    ~xtop_table_reward_claiming_contract_new() override = default;

    BEGIN_CONTRACT_API()
        DECLARE_API(xtop_table_reward_claiming_contract_new::setup);
        DECLARE_API(xtop_table_reward_claiming_contract_new::recv_node_reward);
        DECLARE_API(xtop_table_reward_claiming_contract_new::recv_voter_dividend_reward);
        DECLARE_API(xtop_table_reward_claiming_contract_new::claimNodeReward);
        DECLARE_API(xtop_table_reward_claiming_contract_new::claimVoterDividend);
    END_CONTRACT_API

    /**
     * @brief setup the contract
     */
    void setup();

    /**
     * @brief claim node reward. node reward includes:
     *        1. workload reward + vote reword if the account acts as an auditor
     *        2. workload reward if the account acts as a validator or an edge or an archive
     *        3. tcc reward if the account is a TCC account
     */
    void claimNodeReward();

    /**
     * @brief claim voter dividend reward.
     */
    void claimVoterDividend();

    /**
     * @brief receive node reward
     *
     * @param current_time
     * @param rewards
     */
    void recv_node_reward(const common::xlogic_time_t current_time, std::map<std::string, top::xstake::uint128_t> const & rewards);

    /**
     * @brief receive voter dividend reward
     *
     * @param current_time
     * @param rewards
     */
    void recv_voter_dividend_reward(const common::xlogic_time_t current_time, std::map<std::string, top::xstake::uint128_t> const & rewards);

private:
    /**
     * @brief update node reward record
     *
     * @param account
     * @param record
     */
    void update_working_reward_record(common::xaccount_address_t const & account, xstake::xreward_node_record const & record);

    /**
     * @brief update voter reward record
     *
     * @param account
     * @param record
     */
    void update_vote_reward_record(common::xaccount_address_t const & account, xstake::xreward_record const & record);

    /**
     * @brief Get the node reward record
     *
     * @param account
     * @return record
     */
    xstake::xreward_node_record get_working_reward_record(common::xaccount_address_t const & account) const;

    /**
     * @brief Get the voter reward record
     *
     * @param account
     * @return record
     */
    xstake::xreward_record get_vote_reward_record(common::xaccount_address_t const & account) const;

    /**
     * @brief update node reward
     *
     * @param node_account
     * @param current_time
     * @param reward
     */
    void update(common::xaccount_address_t const & node_account, const common::xlogic_time_t current_time, top::xstake::uint128_t reward);

    /**
     * @brief add node reward
     *
     * @param current_time
     * @param votes_table
     * @param rewards
     * @param adv_votes
     * @param record
     */
    void add_voter_reward(const common::xlogic_time_t current_time,
                          std::map<std::string, uint64_t> & votes_table,
                          std::map<std::string, top::xstake::uint128_t> const & rewards,
                          std::map<std::string, std::string> const & adv_votes,
                          xstake::xreward_record & record);
    
    /**
     * @brief voter_dividend_reward_property
     *
     * @param index
     * @return voter_dividend_reward_prop
     */
    contract_common::properties::xmap_property_t<std::string, std::string> & read_voter_dividend_reward_property_to_set(uint32_t index);
    
    /**
     * @brief voter_dividend_reward_property
     *
     * @param index
     * @return voter_dividend_reward_prop
     */
    contract_common::properties::xmap_property_t<std::string, std::string> const & get_voter_dividend_reward_property(uint32_t index) const;

    std::array<contract_common::properties::xmap_property_t<std::string, std::string>, 4>  m_voter_dividend_reward_prop = {{
        contract_common::properties::xmap_property_t<std::string, std::string>{xstake::XPORPERTY_CONTRACT_VOTER_DIVIDEND_REWARD_KEY1, this},
        contract_common::properties::xmap_property_t<std::string, std::string>{xstake::XPORPERTY_CONTRACT_VOTER_DIVIDEND_REWARD_KEY2, this},
        contract_common::properties::xmap_property_t<std::string, std::string>{xstake::XPORPERTY_CONTRACT_VOTER_DIVIDEND_REWARD_KEY3, this},
        contract_common::properties::xmap_property_t<std::string, std::string>{xstake::XPORPERTY_CONTRACT_VOTER_DIVIDEND_REWARD_KEY4, this}}};
    contract_common::properties::xmap_property_t<std::string, std::string> m_node_reward_prop{xstake::XPORPERTY_CONTRACT_NODE_REWARD_KEY, this};
};
using xtable_reward_claiming_contract_new_t = xtop_table_reward_claiming_contract_new;

NS_END2

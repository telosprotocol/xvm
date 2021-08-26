// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xdata/xtableblock.h"
#include "xstake/xstake_algorithm.h"
#include "xvm/xcontract/xcontract_base.h"
#include "xvm/xcontract/xcontract_exec.h"
#include "xvm/xcontract_helper.h"

NS_BEG2(top, xstake)

using namespace xvm;
using namespace xvm::xcontract;

struct xreward_onchain_param_t {
    uint32_t min_ratio_annual_total_reward;
    uint32_t additional_issue_year_ratio;
    uint32_t edge_reward_ratio;
    uint32_t archive_reward_ratio;
    uint32_t validator_reward_ratio;
    uint32_t auditor_reward_ratio;
    uint32_t vote_reward_ratio;
    uint32_t governance_reward_ratio;
    uint32_t auditor_group_zero_workload;
    uint32_t validator_group_zero_workload;
};

struct xreward_property_param_t {
    std::map<common::xcluster_address_t, cluster_workload_t> auditor_workloads_detail;
    std::map<common::xcluster_address_t, cluster_workload_t> validator_workloads_detail;
    std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, uint64_t>> votes_detail;
    xaccumulated_reward_record accumulated_reward_record;
    std::map<common::xaccount_address_t, xreg_node_info> map_nodes;
    uint64_t zec_vote_contract_height;
    uint64_t zec_workload_contract_height;
    uint64_t zec_reward_contract_height;
};
class xzec_reward_contract : public xcontract_base {
    using xbase_t = xcontract_base;

public:
    XDECLARE_DELETED_COPY_DEFAULTED_MOVE_SEMANTICS(xzec_reward_contract);
    XDECLARE_DEFAULTED_OVERRIDE_DESTRUCTOR(xzec_reward_contract);

    explicit xzec_reward_contract(common::xnetwork_id_t const & network_id);

    xcontract_base * clone() override {
        return new xzec_reward_contract(network_id());
    }

    /**
     * @brief setup the contract
     *
     */
    void setup();

    /**
     * @brief timer processing functions
     *
     * @param current_time chain timer round
     */
    void on_timer(const common::xlogic_time_t current_time);

    /**
     * @brief calaute reward from workload contract
     *
     * @param current_time  timer round
     * @param workload_str the workload report from workload contract
     */
    void calculate_reward(const common::xlogic_time_t current_time, const std::string & workload_str);

    BEGIN_CONTRACT_WITH_PARAM(xzec_reward_contract)
    CONTRACT_FUNCTION_PARAM(xzec_reward_contract, on_timer);
    CONTRACT_FUNCTION_PARAM(xzec_reward_contract, calculate_reward);
    END_CONTRACT_WITH_PARAM

private:
    /**
     * @brief check if we can calculate and dispatch rewards now
     *
     * @param current_time chain timer round
     * @return bool
     */
    bool reward_is_expire(const common::xlogic_time_t current_time, const xactivation_record & activation_record) const;

    /**
     * @brief check if we can calculate and dispatch rewards now
     *
     * @param current_time new_time_height
     * @param reward_issue_interval reward_issue_interval
     * @param activation_serialize activation_serialize
     * @param accumulated_reward_serialize accumulated_reward_serialize
     * @return bool
     */
    bool reward_is_expire_internal(const common::xlogic_time_t current_time,
                                   const uint32_t reward_issue_interval,
                                   const xactivation_record & activation_record,
                                   const std::string & accumulated_reward_serialize) const;

    /**
     * @brief call calc_nodes_rewards_v2 and dispatch_all_reward
     *
     * @param current_time chain timer round
     * @param activation_record activation_record
     */
    void reward(const common::xlogic_time_t current_time,
                const xactivation_record & activation_record,
                const xreward_onchain_param_t & onchain_param,
                xreward_property_param_t & property_param);

    /**
     * @brief Get the task id
     *
     * @return uint32_t
     */
    uint32_t get_task_id() const;

    /**
     * @brief get_task_id_internal
     *
     * @param tasks_map_str tasks_map_str
     * @return uint32_t
     */
    uint32_t get_task_id_internal(std::map<std::string, std::string> const & tasks_map_str) const;

    /**
     * @brief add task
     *
     * @param task_id task id
     * @param current_time chain timer round
     * @param contract contract address
     * @param action action to execute
     * @param params action parameters
     */
    void add_task(const uint32_t task_id, const common::xlogic_time_t current_time, const std::string & contract, const std::string & action, const std::string & params);

    /**
     * @brief add_task_internal
     *
     * @param task_id task id
     * @param current_time chain timer round
     * @param contract contract address
     * @param action action to execute
     * @param params action parameters
     */
    std::pair<std::string, std::string> add_task_internal(const uint32_t task_id,
                                                          const common::xlogic_time_t current_time,
                                                          const std::string & contract,
                                                          const std::string & action,
                                                          const std::string & params);

    /**
     * @brief execute all tasks
     *
     */
    void execute_task();

    /**
     * @brief execute_task_internal
     *
     * @param dispatch_tasks dispatch_tasks
     * @param transfer_map transfer_map
     * @param call_vec call_vec
     */
    void execute_task_internal(std::map<std::string, std::string> & dispatch_tasks,
                               std::map<std::string, uint64_t> & transfer_map,
                               std::vector<xreward_dispatch_task> & call_vec,
                               std::vector<std::string> & rm_vec);

    /**
     * @brief print all tasks
     *
     */
    void print_tasks();

    /**
     * @brief update accumulated issuance record
     *
     * @param record accumulated issuance record
     * @return void
     */
    void update_accumulated_record(const xaccumulated_reward_record & record);

    /**
     * @brief update accumulated issuance
     *
     * @param timer_round chain timer round
     * @param issuance issuance of this round
     * @param activation activation
     */
    void update_accumulated_issuance(const common::xlogic_time_t timer_round, const uint64_t issuance, const xactivation_record & activation);

    /**
     * @brief update_accumulated_issuance_internal
     *
     * @param issuance issuance
     * @param cur_year_issuances_str cur_year_issuances_str
     * @param total_issuances_str total_issuances_str
     * @param cur_year_issuances_str_new cur_year_issuances_str_new
     * @param total_issuances_str_new total_issuances_str_new
     */
    void update_accumulated_issuance_internal(const uint64_t issuance,
                                              const std::string & cur_year_issuances_str,
                                              const std::string & total_issuances_str,
                                              std::string & cur_year_issuances_str_new,
                                              std::string & total_issuances_str_new);

    /**
     * @brief update issuance detail
     *
     * @param issue_detail
     */
    void update_issuance_detail(xissue_detail const & issue_detail);

    /**
     * @brief
     *
     * @param current_time
     */
    void update_reg_contract_read_status(const common::xlogic_time_t current_time);

    /**
     * @brief
     *
     * @param current_time
     * @param last_read_time
     * @param last_read_height
     * @param latest_height
     * @param height_step_limitation
     * @param timeout_limitation
     * @param next_read_height
     * @return true
     * @return false
     */
    bool update_reg_contract_read_status_internal(const common::xlogic_time_t current_time,
                                                  const uint64_t last_read_time,
                                                  const uint64_t last_read_height,
                                                  const uint64_t latest_height,
                                                  const uint64_t height_step_limitation,
                                                  const common::xlogic_time_t timeout_limitation,
                                                  uint64_t & next_read_height);

    /**
     * @brief receive workload
     *
     * @param workload_str
     */
    void on_receive_workload(const std::string & workload_str);

    /**
     * @brief save workload
     *
     */
    void add_cluster_workload(const std::string & workload_str,
                              const std::map<std::string, std::string> & auditor_workload_str,
                              const std::map<std::string, std::string> & validator_workload_str,
                              std::map<std::string, std::string> & auditor_workload_str_change,
                              std::map<std::string, std::string> & validator_workload_str_change);
    // add by lon
    /**
     * @brief get_reward_onchain_param
     *
     * @param onchain_param onchain params
     */
    void get_reward_onchain_param(xreward_onchain_param_t & onchain_param);

    /**
     * @brief get reward params using in issuance calculation
     *
     * @param onchain_param onchain params
     * @param property_param property params
     */
    void get_reward_param(xreward_onchain_param_t & onchain_param, xreward_property_param_t & property_param);

    void calc_rewards(const common::xlogic_time_t current_time,
                      const common::xlogic_time_t activation_time,
                      const xreward_onchain_param_t & onchain_param,
                      xreward_property_param_t & property_param,
                      xissue_detail & issue_detail,
                      std::map<common::xaccount_address_t, top::xstake::uint128_t> & table_total_rewards,
                      std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, top::xstake::uint128_t>> & table_node_reward_detail,
                      std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, top::xstake::uint128_t>> & table_node_dividend_detail,
                      top::xstake::uint128_t & community_reward);
    /**
     * @brief calculate nodes rewards
     *
     * @param current_time current_time
     * @param activation_time activation_time
     * @param onchain_param onchain params get from get_reward_param
     * @param property_param property params get from get_reward_param
     * @param issue_detail record every node issuance detail
     * @param node_reward_detail record self reward of accounts
     * @param node_dividend_detail record dividend reward of accounts
     * @param community_reward record community reward to transfer
     */
    void calc_nodes_rewards_v5(const common::xlogic_time_t current_time,
                               const common::xlogic_time_t activation_time,
                               xreward_onchain_param_t const & onchain_param,
                               xreward_property_param_t & property_param,
                               xissue_detail & issue_detail,
                               std::map<common::xaccount_address_t, top::xstake::uint128_t> & node_reward_detail,
                               std::map<common::xaccount_address_t, top::xstake::uint128_t> & node_dividend_detail,
                               top::xstake::uint128_t & community_reward);

    /**
     * @brief calculate every table rewards
     *
     * @param property_param property params get from get_reward_param
     * @param node_reward_detail record self reward of accounts get from calc_nodes_rewards_v5
     * @param node_dividend_detail record dividend reward of accounts get from calc_nodes_rewards_v5
     * @param table_node_reward_detail record node reward details of every table
     * @param table_node_dividend_detail record dividend reward details of every table
     * @param table_total_rewards record total rewards of every table
     */
    void calc_table_rewards(xreward_property_param_t & property_param,
                            std::map<common::xaccount_address_t, top::xstake::uint128_t> const & node_reward_detail,
                            std::map<common::xaccount_address_t, top::xstake::uint128_t> const & node_dividend_detail,
                            std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, top::xstake::uint128_t>> & table_node_reward_detail,
                            std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, top::xstake::uint128_t>> & table_node_dividend_detail,
                            std::map<common::xaccount_address_t, top::xstake::uint128_t> & table_total_rewards);

    /**
     * @brief dispatch all reward calculated in calc_nodes_rewards
     *
     * @param current_time current time
     * @param table_total_rewards record to transfer calculated in calc_nodes_rewards
     * @param table_node_reward_detail node self reward detail calculated in calc_nodes_rewards
     * @param table_node_dividend_detail dividend reward detail calculated in calc_nodes_rewards
     * @param community_reward community reward calculated in calc_nodes_rewards
     * @param actual_issuance actual issuance this time
     */
    void dispatch_all_reward_v3(const common::xlogic_time_t current_time,
                                std::map<common::xaccount_address_t, top::xstake::uint128_t> const & table_total_rewards,
                                std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, top::xstake::uint128_t>> const & table_node_reward_detail,
                                std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, top::xstake::uint128_t>> const & table_node_dividend_detail,
                                const top::xstake::uint128_t & community_reward,
                                uint64_t & actual_issuance);

    /**
     * @brief update property
     *
     * @param current_time current time
     * @param actual_issuance issuance calculated in dispatch_all_reward
     * @param activation activation
     * @param accumulated_reward xaccumulated_reward_record
     * @param issue_detail issuance detial this time
     */
    void update_property(const common::xlogic_time_t current_time,
                         const uint64_t actual_issuance,
                         const xactivation_record & activation,
                         const xaccumulated_reward_record & accumulated_reward,
                         xissue_detail const & issue_detail);

    /**
     * @brief calculate issuance
     *
     * @param issue_time_length time start with system activation, end uptil now
     * @param min_ratio_annual_total_reward onchain_parameter
     * @param additional_issue_year_ratio onchain_parameter
     * @param record accumulated reward record
     * @return total reward issuance
     */
    top::xstake::uint128_t calc_total_issuance(const common::xlogic_time_t issue_time_length,
                                               const uint32_t min_ratio_annual_total_reward,
                                               const uint32_t additional_issue_year_ratio,
                                               xaccumulated_reward_record & record);

    /**
     * @brief Get the reserve reward object
     *
     * @param issued_until_last_year_end
     * @param minimum_issuance
     * @param issuance_rate
     * @return top::xstake::uint128_t
     */
    top::xstake::uint128_t calc_issuance_internal(top::xstake::uint128_t issued_until_last_year_end, top::xstake::uint128_t minimum_issuance, uint32_t issuance_rate);

    /**
     * @brief calculate node nums of different property(xreward_num_type_e) and different role type(xreward_role_type_e)
     *
     * @param map_nodes nodes to count
     * @return count resualt
     */
    std::vector<std::vector<uint32_t>> calc_role_nums(std::map<common::xaccount_address_t, xreg_node_info> const & map_nodes);

    /**
     * @brief set votes_detail into map_nodes and calculate total valid auditor votes
     *
     * @param votes_detail votes detail
     * @param map_nodes nodes detail
     * @param account_votes vote of every node in this round
     * @return total valid auditor votes
     */
    uint64_t calc_votes(std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, uint64_t>> const & votes_detail,
                        std::map<common::xaccount_address_t, xreg_node_info> & map_nodes,
                        std::map<common::xaccount_address_t, uint64_t> & account_votes);

    /**
     * @brief caculate vote of every node in this round
     *
     * @param votes_detail votes detail
     * @param map_nodes nodes detail
     * @return account_votes vote of every node in this round
     */
    std::map<common::xaccount_address_t, uint64_t> calc_votes(std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, uint64_t>> const & votes_detail,
                                                              std::map<common::xaccount_address_t, xreg_node_info> const & map_nodes);

    /**
     * @brief calculate zero workload reward
     *
     * @param is_auditor whether is auditor grouo
     * @param workloads_detail workloads detail of all groups
     * @param zero_workload zero workload value of group
     * @param group_reward reward of single group
     * @param zero_workload_account record zero workload account
     * @return total zero workload reward
     */
    top::xstake::uint128_t calc_zero_workload_reward(bool is_auditor,
                                                     std::map<common::xcluster_address_t, cluster_workload_t> & workloads_detail,
                                                     const uint32_t zero_workload,
                                                     const top::xstake::uint128_t group_reward,
                                                     std::vector<string> & zero_workload_account);

    /**
     * @brief calculate invalid workload group reward which roup nodes all invalid
     *
     * @param is_auditor whether is auditor grouo
     * @param map_nodes all nodes detail
     * @param group_reward reward of single group
     * @param workloads_detail workloads detail of all groups
     * @return total invalid workload group reward
     */
    top::xstake::uint128_t calc_invalid_workload_group_reward(bool is_auditor,
                                                              std::map<common::xaccount_address_t, xreg_node_info> const & map_nodes,
                                                              const top::xstake::uint128_t group_reward,
                                                              std::map<common::xcluster_address_t, cluster_workload_t> & workloads_detail);

    /**
     * @brief calculate calc edger worklaod rewards
     *
     * @param node node info
     * @param edge_num edger nums caculated in calc_role_nums
     * @param edge_workload_rewards edge total workloads
     * @param reward_to_self node self reward
     */
    void calc_edge_workload_rewards(xreg_node_info const & node,
                                    std::vector<uint32_t> const & edge_num,
                                    const top::xstake::uint128_t edge_workload_rewards,
                                    top::xstake::uint128_t & reward_to_self);

    /**
     * @brief calculate calc archiver worklaod rewards
     *
     * @param node node info
     * @param archive_num archiver nums caculated in calc_role_nums
     * @param archive_workload_rewards archive total workloads
     * @param reward_to_self node self reward
     */
    void calc_archive_workload_rewards(xreg_node_info const & node,
                                       std::vector<uint32_t> const & archive_num,
                                       const top::xstake::uint128_t archive_workload_rewards,
                                       top::xstake::uint128_t & reward_to_self);

    /**
     * @brief calculate calc validator worklaod rewards
     *
     * @param node node info
     * @param validator_num validator nums caculated in calc_role_nums
     * @param validator_workloads_detail workloads detail of all validator groups
     * @param validator_group_workload_rewards single roup reward of validator groups
     * @param reward_to_self node self reward
     */
    void calc_validator_workload_rewards(xreg_node_info const & node,
                                         std::vector<uint32_t> const & validator_num,
                                         std::map<common::xcluster_address_t, cluster_workload_t> const & validator_workloads_detail,
                                         const top::xstake::uint128_t validator_group_workload_rewards,
                                         top::xstake::uint128_t & reward_to_self);

    /**
     * @brief calculate calc auditor worklaod rewards
     *
     * @param node node info
     * @param auditor_num auditor nums caculated in calc_role_nums
     * @param auditor_workloads_detail workloads detail of all auditor groups
     * @param auditor_group_workload_rewards single roup reward of auditor groups
     * @param reward_to_self node self reward
     */
    void calc_auditor_workload_rewards(xreg_node_info const & node,
                                       std::vector<uint32_t> const & auditor_num,
                                       std::map<common::xcluster_address_t, cluster_workload_t> const & auditor_workloads_detail,
                                       const top::xstake::uint128_t auditor_group_workload_rewards,
                                       top::xstake::uint128_t & reward_to_self);

    /**
     * @brief calculate vote reward
     *
     * @param node node info
     * @param auditor_total_votes auditor total calid votes caculated in calc_votes
     * @param vote_rewards total vote reward
     * @param reward_to_self node self reward
     */
    void calc_vote_reward(xreg_node_info const & node, const uint64_t auditor_total_votes, const top::xstake::uint128_t vote_rewards, top::xstake::uint128_t & reward_to_self);

    /**
     * @brief record self reward
     *
     * @param table_address address of account
     * @param account node account
     * @param node_reward node self reward
     * @param table_total_rewards add node self reward into table
     * @param table_node_reward_detail record node self reward into table detail
     */
    void calc_table_node_reward_detail(common::xaccount_address_t const & table_address,
                                       common::xaccount_address_t const & account,
                                       top::xstake::uint128_t node_reward,
                                       std::map<common::xaccount_address_t, top::xstake::uint128_t> & table_total_rewards,
                                       std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, top::xstake::uint128_t>> & table_node_reward_detail);

    /**
     * @brief record dividend reward
     *
     * @param table_address address of voter table contract
     * @param account node account
     * @param reward account dividend reward
     * @param node_total_votes node total votes
     * @param votes_detail all votes detail
     * @param table_total_rewards add node self reward into table
     * @param table_node_dividend_detail record node dividend reward into table detail
     */
    void calc_table_node_dividend_detail(common::xaccount_address_t const & table_address,
                                         common::xaccount_address_t const & account,
                                         top::xstake::uint128_t const & reward,
                                         uint64_t node_total_votes,
                                         std::map<common::xaccount_address_t, uint64_t> const & votes_detail,
                                         std::map<common::xaccount_address_t, top::xstake::uint128_t> & table_total_rewards,
                                         std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, top::xstake::uint128_t>> & table_node_dividend_detail);

    /**
     * @brief calculate table contract address
     *
     * @param account account
     * @return address
     */
    common::xaccount_address_t calc_table_contract_address(common::xaccount_address_t const & account);

    /**
     * @brief calc_current_year
     *
     * @param time_since_active time_since_active
     * @return current_year
     */
    uint32_t calc_current_year(const common::xlogic_time_t time_since_active) const;

    /**
     * @brief reward_task_serialize
     *
     * @param task task
     * @return task_str
     */
    std::string reward_task_serialize(const xreward_dispatch_task & task) const;

    /**
     * @brief reward_task_deserialize
     *
     * @param task_str task_str
     * @return task
     */
    xreward_dispatch_task reward_task_deserialize(const std::string & task_str) const;

    /**
     * @brief accumulated_reward_serialize
     *
     * @param record record
     * @return record_str
     */
    std::string accumulated_reward_serialize(xaccumulated_reward_record const & record) const;

    /**
     * @brief accumulated_reward_deserialize
     *
     * @param record_str record_str
     * @return record
     */
    xaccumulated_reward_record accumulated_reward_deserialize(const std::string & accumulated_reward_serialize) const;

    /**
     * @brief get_activation_record
     *
     * @return record
     */
    xactivation_record get_activation_record() const;
};

NS_END2

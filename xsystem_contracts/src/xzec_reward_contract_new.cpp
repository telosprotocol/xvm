// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xreward/xzec_reward_contract_new.h"

#include "xchain_upgrade/xchain_data_processor.h"
#include "xdata/xgenesis_data.h"
#include "xstake/xstake_algorithm.h"

using namespace top::xstake;
using namespace top::base;

#if !defined(XZEC_MODULE)
#    define XZEC_MODULE "sysContract_"
#endif

#define XCONTRACT_PREFIX "reward_"

#define XREWARD_CONTRACT XZEC_MODULE XCONTRACT_PREFIX

#define VALID_EDGER(node) (node.get_deposit() > 0)
#define VALID_ARCHIVER(node) (node.get_deposit() > 0 && node.is_valid_archive_node())
#define VALID_AUDITOR(node) (node.get_deposit() > 0 && node.is_valid_auditor_node())
#define VALID_VALIDATOR(node) (node.get_deposit() > 0)

enum { total_idx = 0, valid_idx, deposit_zero_num, num_type_idx_num } xreward_num_type_e;
enum { edger_idx = 0, archiver_idx, auditor_idx, validator_idx, role_type_idx_num } xreward_role_type_e;

NS_BEG2(top, system_contracts)

void xtop_zec_reward_contract_new::setup() {
    //m_auditor_workload.initialize();
    //m_validator_workload.initialize();
    //m_accumulate_issuance.initialize();
    //m_reward_detail.initialize();
    //m_accumulate_issuance_yearly.initialize();
    //m_last_read_rec_reg_contract_height.initialize();
    //m_last_read_rec_reg_contract_time.initialize();

    // MAP_CREATE(XPORPERTY_CONTRACT_TASK_KEY);  // save dispatch tasks
    // std::vector<std::pair<std::string, std::string>> db_kv_111;
    // chain_data::xchain_data_processor_t::get_stake_map_property(address(), xstake::XPORPERTY_CONTRACT_TASK_KEY, db_kv_111);
    // for (auto const & _p : db_kv_111) {
    //     MAP_SET(XPORPERTY_CONTRACT_TASK_KEY, _p.first, _p.second);
    // }

    // MAP_CREATE(XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE);  // save issuance
    std::vector<std::pair<std::string, std::string>> db_kv_141;
    chain_data::xchain_data_processor_t::get_stake_map_property(address(), XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE, db_kv_141);
    for (auto const & _p : db_kv_141) {
        // MAP_SET(XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE, _p.first, _p.second);
        m_accumulate_issuance.set(_p.first, _p.second);
    }

    // STRING_CREATE(XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE_YEARLY);
    std::string db_kv_142;
    chain_data::xchain_data_processor_t::get_stake_string_property(address(), XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE_YEARLY, db_kv_142);
    if (!db_kv_142.empty()) {
        // STRING_SET(XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE_YEARLY, db_kv_142);
        m_accumulate_issuance_yearly.set(db_kv_142);
    } else {
        xaccumulated_reward_record record;
        update_accumulated_record(record);
    }

    // STRING_CREATE(XPROPERTY_LAST_READ_REC_REG_CONTRACT_BLOCK_HEIGHT);
    // std::string last_read_rec_reg_contract_height{"0"};
    // STRING_SET(XPROPERTY_LAST_READ_REC_REG_CONTRACT_BLOCK_HEIGHT, last_read_rec_reg_contract_height);
    m_last_read_rec_reg_contract_height.set(xstring_utl::tostring(0));
    // STRING_CREATE(XPROPERTY_LAST_READ_REC_REG_CONTRACT_LOGIC_TIME);
    // std::string last_read_rec_reg_contract_logic_time{"0"};
    // STRING_SET(XPROPERTY_LAST_READ_REC_REG_CONTRACT_LOGIC_TIME, last_read_rec_reg_contract_logic_time);
    m_last_read_rec_reg_contract_time.set(xstring_utl::tostring(0));

    // STRING_CREATE(XPROPERTY_REWARD_DETAIL);
    xissue_detail detail;
    update_issuance_detail(detail);
    // MAP_CREATE(XPORPERTY_CONTRACT_WORKLOAD_KEY);
    std::vector<std::pair<std::string, std::string>> db_kv_103;
    chain_data::xchain_data_processor_t::get_stake_map_property(address(), XPORPERTY_CONTRACT_WORKLOAD_KEY, db_kv_103);
    for (auto const & _p : db_kv_103) {
        // MAP_SET(XPORPERTY_CONTRACT_WORKLOAD_KEY, _p.first, _p.second);
        m_auditor_workload.set(_p.first, _p.second);
    }
    // MAP_CREATE(XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY);
    std::vector<std::pair<std::string, std::string>> db_kv_125;
    chain_data::xchain_data_processor_t::get_stake_map_property(address(), XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY, db_kv_125);
    for (auto const & _p : db_kv_125) {
        // MAP_SET(XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY, _p.first, _p.second);
        m_validator_workload.set(_p.first, _p.second);
    }
}

void xtop_zec_reward_contract_new::on_timer(const common::xlogic_time_t current_time) {
    XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "on_timer_Called", 1);
    XMETRICS_TIME_RECORD(XREWARD_CONTRACT "on_timer_ExecutionTime");

    std::string source_address = address().value();
    if (address().value() != source_address) {
        xwarn("[xtop_zec_reward_contract_new::on_timer] invalid call from %s", source_address.c_str());
        return;
    }

    // if (MAP_SIZE(XPORPERTY_CONTRACT_TASK_KEY) > 0) {
    //     execute_task();
    // } else {
    auto const activation_record = get_activation_record();
    XCONTRACT_ENSURE(current_time > activation_record.activation_time, "current_time <= activation_time");
    if (reward_is_expire(current_time, activation_record)) {
        xreward_onchain_param_t onchain_param;    // onchain params
        xreward_property_param_t property_param;  // property from self and other contracts
        get_reward_param(onchain_param, property_param);
        reward(current_time, activation_record, onchain_param, property_param);
    } else {
        update_reg_contract_read_status(current_time);
    }
    // }

    XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "on_timer_Executed", 1);
}

bool xtop_zec_reward_contract_new::update_reg_contract_read_status_internal(const common::xlogic_time_t cur_time,
                                                                            const uint64_t last_read_time,
                                                                            const uint64_t last_read_height,
                                                                            const uint64_t latest_height,
                                                                            const uint64_t height_step_limitation,
                                                                            const common::xlogic_time_t timeout_limitation,
                                                                            uint64_t & next_read_height) {
    if (latest_height < last_read_height)
        return false;

    if (latest_height - last_read_height >= height_step_limitation) {
        next_read_height = last_read_height + height_step_limitation;
        return true;
    } else if (cur_time - last_read_time > timeout_limitation) {
        next_read_height = latest_height;
        return true;
    }
    return false;
}

void xtop_zec_reward_contract_new::update_reg_contract_read_status(const common::xlogic_time_t cur_time) {
    bool update_rec_reg_contract_read_status{false};

    auto const last_read_height = static_cast<std::uint64_t>(std::stoull(m_last_read_rec_reg_contract_height.value()));
    auto const last_read_time = static_cast<std::uint64_t>(std::stoull(m_last_read_rec_reg_contract_time.value()));

    auto const height_step_limitation = XGET_ONCHAIN_GOVERNANCE_PARAMETER(cross_reading_rec_reg_contract_height_step_limitation);
    auto const timeout_limitation = XGET_ONCHAIN_GOVERNANCE_PARAMETER(cross_reading_rec_reg_contract_logic_timeout_limitation);

    // uint64_t latest_height = get_blockchain_height(sys_contract_rec_registration_addr);
    uint64_t latest_height = state()->state_height(common::xaccount_address_t{sys_contract_rec_registration_addr});
    xdbg("[xtop_zec_reward_contract_new::update_reg_contract_read_status] cur_time: %llu, last_read_time: %llu, last_read_height: %llu, latest_height: %" PRIu64,
         cur_time,
         last_read_time,
         last_read_height,
         latest_height);
    XCONTRACT_ENSURE(latest_height >= last_read_height, u8"xtop_zec_reward_contract_new::update_reg_contract_read_status latest_height < last_read_height");
    if (latest_height == last_read_height) {
        XMETRICS_PACKET_INFO(XREWARD_CONTRACT "update_status", "next_read_height", last_read_height, "current_time", cur_time)
        // STRING_SET(XPROPERTY_LAST_READ_REC_REG_CONTRACT_LOGIC_TIME, std::to_string(cur_time));
        m_last_read_rec_reg_contract_time.set(xstring_utl::tostring(cur_time));
        return;
    }
    // calc current_read_height:
    uint64_t next_read_height = last_read_height;
    update_rec_reg_contract_read_status =
        update_reg_contract_read_status_internal(cur_time, last_read_time, last_read_height, latest_height, height_step_limitation, timeout_limitation, next_read_height);
    xinfo("[xtop_zec_reward_contract_new::update_reg_contract_read_status] next_read_height: %" PRIu64 ", latest_height: %llu, update_rec_reg_contract_read_status: %d",
          next_read_height,
          latest_height,
          update_rec_reg_contract_read_status);

    if (update_rec_reg_contract_read_status) {
        // xauto_ptr<xblock_t> block_ptr = get_block_by_height(sys_contract_rec_registration_addr, next_read_height);
        XCONTRACT_ENSURE(state()->block_exist(common::xaccount_address_t{sys_contract_rec_registration_addr}, next_read_height) == true, "fail to get the rec_reg data");
        XMETRICS_PACKET_INFO(XREWARD_CONTRACT "update_status", "next_read_height", next_read_height, "current_time", cur_time)
        // STRING_SET(XPROPERTY_LAST_READ_REC_REG_CONTRACT_BLOCK_HEIGHT, std::to_string(next_read_height));
        // STRING_SET(XPROPERTY_LAST_READ_REC_REG_CONTRACT_LOGIC_TIME, std::to_string(cur_time));
        m_last_read_rec_reg_contract_height.set(xstring_utl::tostring(next_read_height));
        m_last_read_rec_reg_contract_time.set(xstring_utl::tostring(cur_time));
    }
    return;
}

void xtop_zec_reward_contract_new::calculate_reward(const common::xlogic_time_t current_time, const std::string & workload_str) {
    XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "calculate_reward_Called", 1);
    XMETRICS_TIME_RECORD(XREWARD_CONTRACT "calculate_reward_ExecutionTime");
    auto const & source_address = address().value();
    xinfo("[xtop_zec_reward_contract_new::calculate_reward] time: %lu, called from address: %s, pid:%d", current_time, source_address.c_str(), getpid());
    if (sys_contract_zec_workload_addr != source_address) {
        xwarn("[xtop_zec_reward_contract_new::calculate_reward] from invalid address: %s\n", source_address.c_str());
        return;
    }

    // std::map<std::string, std::string> auditor_workload_str;
    // std::map<std::string, std::string> validator_workload_str;
    auto auditor_workload_str = m_auditor_workload.value();
    auto validator_workload_str = m_validator_workload.value();
    // MAP_COPY_GET(XPORPERTY_CONTRACT_WORKLOAD_KEY, auditor_workload_str);
    // MAP_COPY_GET(XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY, validator_workload_str);

    std::map<std::string, std::string> auditor_workload_str_change;
    std::map<std::string, std::string> validator_workload_str_change;
    add_cluster_workload(workload_str, auditor_workload_str, validator_workload_str, auditor_workload_str_change, validator_workload_str_change);

    for (auto const item : auditor_workload_str_change) {
        // MAP_SET(XPORPERTY_CONTRACT_WORKLOAD_KEY, item.first, item.second);
        m_auditor_workload.set(item.first, item.second);
    }
    for (auto const item : validator_workload_str_change) {
        // MAP_SET(XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY, item.first, item.second);
        m_validator_workload.set(item.first, item.second);
    }
}

void xtop_zec_reward_contract_new::add_cluster_workload(const std::string & workload_str,
                                                        const std::map<std::string, std::string> & auditor_workload_str,
                                                        const std::map<std::string, std::string> & validator_workload_str,
                                                        std::map<std::string, std::string> & auditor_workload_str_change,
                                                        std::map<std::string, std::string> & validator_workload_str_change) {
    std::map<common::xcluster_address_t, xgroup_workload_t> workload_info;
    {
        xstream_t stream(xcontext_t::instance(), (uint8_t *)workload_str.data(), workload_str.size());
        MAP_OBJECT_DESERIALZE2(stream, workload_info);
        xdbg("[xtop_zec_reward_contract_new::calculate_reward] workload_str size: %zu\n", workload_info.size());
    }

    for (auto const & workload : workload_info) {
        auto const cluster_id = workload.first;
        auto const leader_cnt = workload.second.m_leader_count;
        std::string cluster_id_str;
        {
            xstream_t stream(xcontext_t::instance());
            stream << cluster_id;
            cluster_id_str = std::string((const char *)stream.data(), stream.size());
        }
        cluster_workload_t workload_add;
        if (common::has<common::xnode_type_t::auditor>(cluster_id.type())) {
            auto it = auditor_workload_str.find(cluster_id_str);
            if (it == auditor_workload_str.end()) {
                xdbg("[xtop_zec_reward_contract_new::calculate_reward] auditor cluster_id not exist: %s", cluster_id.to_string().c_str());
            } else {
                xstream_t stream(xcontext_t::instance(), (uint8_t *)it->second.data(), it->second.size());
                workload_add.serialize_from(stream);
            }
        } else if (common::has<common::xnode_type_t::validator>(cluster_id.type())) {
            auto it = validator_workload_str.find(cluster_id_str);
            if (it == validator_workload_str.end()) {
                xdbg("[xtop_zec_reward_contract_new::calculate_reward] validator cluster_id not exist: %s", cluster_id.to_string().c_str());
            } else {
                xstream_t stream(xcontext_t::instance(), (uint8_t *)it->second.data(), it->second.size());
                workload_add.serialize_from(stream);
            }
        } else {
            // invalid group
            xwarn("[xzec_workload_contract_v2::accumulate_workload] invalid group id: %d", workload.first.group_id().value());
            continue;
        }
        for (auto const & leader_count_info : leader_cnt) {
            auto const & leader = leader_count_info.first;
            auto const & work = leader_count_info.second;

            workload_add.m_leader_count[leader] += work;
            workload_add.cluster_total_workload += work;
            xdbg("[xtop_zec_reward_contract_new::add_cluster_workload] cluster_id: %s, leader: %s, work: %u, leader total workload: %u, group total workload: %d\n",
                 cluster_id.to_string().c_str(),
                 leader_count_info.first.c_str(),
                 work,
                 workload_add.m_leader_count[leader],
                 workload_add.cluster_total_workload);
        }
        xstream_t stream(xcontext_t::instance());
        workload_add.serialize_to(stream);
        if (common::has<common::xnode_type_t::auditor>(cluster_id.type())) {
            auditor_workload_str_change[cluster_id_str] = std::string((const char *)stream.data(), stream.size());
        } else if (common::has<common::xnode_type_t::validator>(cluster_id.type())) {
            validator_workload_str_change[cluster_id_str] = std::string((const char *)stream.data(), stream.size());
        }
    }
}

void xtop_zec_reward_contract_new::reward(const common::xlogic_time_t current_time,
                                          const xactivation_record & activation_record,
                                          const xreward_onchain_param_t & onchain_param,
                                          xreward_property_param_t & property_param) {
    xdbg("[xtop_zec_reward_contract_new::reward] pid:%d\n", getpid());
    // step1 calculate node and table rewards
    std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, top::xstake::uint128_t>> table_nodes_rewards;  // <table, <node, reward>>
    std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, top::xstake::uint128_t>> table_vote_rewards;   // <table, <node be voted, reward>>
    std::map<common::xaccount_address_t, top::xstake::uint128_t> contract_rewards;                                           // <table, total reward>
    top::xstake::uint128_t community_reward;                                                                                 // community reward
    xissue_detail issue_detail;                                                                                              // issue details this round
    calc_rewards(
        current_time, activation_record.activation_time, onchain_param, property_param, issue_detail, contract_rewards, table_nodes_rewards, table_vote_rewards, community_reward);
    // step2 dispatch rewards
    uint64_t actual_issuance;
    dispatch_all_reward_v3(current_time, contract_rewards, table_nodes_rewards, table_vote_rewards, community_reward, actual_issuance);
    // step3 update property
    update_property(current_time, actual_issuance, activation_record, property_param.accumulated_reward_record, issue_detail);
}

bool xtop_zec_reward_contract_new::reward_is_expire_internal(const common::xlogic_time_t new_time_height,
                                                             const uint32_t reward_issue_interval,
                                                             const xactivation_record & activation_record,
                                                             const std::string & accumulated_reward_serialize) const {
    if (accumulated_reward_serialize.empty()) {
        xwarn("[xtop_zec_reward_contract_new::reward_is_expire_internal] accumulated_reward_serialize empty");
        return false;
    }
    xaccumulated_reward_record accumulated_reward = accumulated_reward_deserialize(accumulated_reward_serialize);
    if (activation_record.activated == 0) {
        xinfo("[xtop_zec_reward_contract_new::reward_is_expire_internal] mainnet not activated: %d, time height: %llu", activation_record.activated, new_time_height);
        return false;
    }
    const common::xlogic_time_t old_time_height = activation_record.activation_time + accumulated_reward.last_issuance_time;
    xinfo("[xtop_zec_reward_contract_new::reward_is_expire_internal] new_time_height %llu, old_time_height %llu, reward_issue_interval: %u\n",
          new_time_height,
          old_time_height,
          reward_issue_interval);
    if (new_time_height <= old_time_height || new_time_height < old_time_height + reward_issue_interval) {
        return false;
    }
    xinfo("[xtop_zec_reward_contract_new::reward_is_expire_internal] will reward!");
    return true;
}

bool xtop_zec_reward_contract_new::reward_is_expire(const common::xlogic_time_t onchain_timer_round, const xactivation_record & activation_record) const {
    // std::string const & accumulated_reward_serialize = STRING_GET2(XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE_YEARLY);
    auto const & accumulated_reward_serialize = m_accumulate_issuance_yearly.value();
    const uint32_t reward_issue_interval = XGET_ONCHAIN_GOVERNANCE_PARAMETER(reward_issue_interval);
    return reward_is_expire_internal(onchain_timer_round, reward_issue_interval, activation_record, accumulated_reward_serialize);
}
#if 0
uint32_t xtop_zec_reward_contract_new::get_task_id_internal(std::map<std::string, std::string> const & tasks_map_str) const {
    uint32_t task_id = 0;
    if (tasks_map_str.size() > 0) {
        auto it = tasks_map_str.end();
        it--;
        task_id = xstring_utl::touint32(it->first);
        task_id++;
    }
    return task_id;
}

uint32_t xtop_zec_reward_contract_new::get_task_id() const {
    std::map<std::string, std::string> tasks_map_str;
    try {
        MAP_COPY_GET(XPORPERTY_CONTRACT_TASK_KEY, tasks_map_str);
    } catch (std::runtime_error & e) {
        xwarn("[xtop_zec_reward_contract_new::get_task_id] get XPORPERTY_CONTRACT_TASK_KEY error:%s", e.what());
    }
    return get_task_id_internal(tasks_map_str);
}

std::pair<std::string, std::string> xtop_zec_reward_contract_new::add_task_internal(const uint32_t task_id,
                                                                            const common::xlogic_time_t onchain_timer_round,
                                                                            const std::string & contract,
                                                                            const std::string & action,
                                                                            const std::string & params) {
    xreward_dispatch_task task;
    task.onchain_timer_round = onchain_timer_round;
    task.contract = contract;
    task.action = action;
    task.params = params;
    std::stringstream ss;
    ss << std::setw(10) << std::setfill('0') << task_id;
    return std::make_pair(ss.str(), reward_task_serialize(task));
}

void xtop_zec_reward_contract_new::add_task(const uint32_t task_id,
                                    const common::xlogic_time_t onchain_timer_round,
                                    const std::string & contract,
                                    const std::string & action,
                                    const std::string & params) {
    auto pair = add_task_internal(task_id, onchain_timer_round, contract, action, params);
    try {
        MAP_SET(XPORPERTY_CONTRACT_TASK_KEY, pair.first, pair.second);
    } catch (std::runtime_error & e) {
        xwarn("[xtop_zec_reward_contract_new::add_task] set XPORPERTY_CONTRACT_TASK_KEY error:%s", e.what());
    }
}

void xtop_zec_reward_contract_new::execute_task_internal(std::map<std::string, std::string> & dispatch_tasks,
                                                 std::map<std::string, uint64_t> & transfer_map,
                                                 std::vector<xreward_dispatch_task> & call_vec,
                                                 std::vector<std::string> & rm_vec) {
    XMETRICS_TIME_RECORD(XREWARD_CONTRACT "execute_task_ExecutionTime");

    xreward_dispatch_task task;

    xdbg("[xtop_zec_reward_contract_new::execute_task] map size: %d\n", dispatch_tasks.size());
    XMETRICS_COUNTER_SET(XREWARD_CONTRACT "currentTaskCnt", dispatch_tasks.size());

    const int32_t task_num_per_round = 16;
    for (auto i = 0; i < task_num_per_round; i++) {
        auto it = dispatch_tasks.begin();
        if (it == dispatch_tasks.end())
            return;

        xstream_t stream(xcontext_t::instance(), (uint8_t *)it->second.c_str(), (uint32_t)it->second.size());
        task.serialize_from(stream);

        XMETRICS_PACKET_INFO(XREWARD_CONTRACT "executeTask",
                             "id",
                             it->first,
                             "logicTime",
                             task.onchain_timer_round,
                             "targetContractAddr",
                             task.contract,
                             "action",
                             task.action,
                             "onChainParamTaskNumPerRound",
                             task_num_per_round);

        // debug output
        if (task.action == XREWARD_CLAIMING_ADD_NODE_REWARD || task.action == XREWARD_CLAIMING_ADD_VOTER_DIVIDEND_REWARD) {
            xstream_t stream_params(xcontext_t::instance(), (uint8_t *)task.params.c_str(), (uint32_t)task.params.size());
            uint64_t onchain_timer_round;
            std::map<std::string, top::xstake::uint128_t> rewards;
            stream_params >> onchain_timer_round;
            stream_params >> rewards;
            for (auto const & r : rewards) {
                xinfo("[xtop_zec_reward_contract_new::execute_task] contract: %s, action: %s, account: %s, reward: [%llu, %u], onchain_timer_round: %llu\n",
                      task.contract.c_str(),
                      task.action.c_str(),
                      r.first.c_str(),
                      static_cast<uint64_t>(r.second / REWARD_PRECISION),
                      static_cast<uint32_t>(r.second % REWARD_PRECISION),
                      task.onchain_timer_round);
            }
        } else if (task.action == XTRANSFER_ACTION) {
            std::map<std::string, uint64_t> issuances;
            xstream_t seo_stream(xcontext_t::instance(), (uint8_t *)task.params.c_str(), (uint32_t)task.params.size());
            seo_stream >> issuances;
            for (auto const & issue : issuances) {
                xinfo("[xtop_zec_reward_contract_new::execute_task] action: %s, contract account: %s, issuance: %llu, onchain_timer_round: %llu\n",
                      task.action.c_str(),
                      issue.first.c_str(),
                      issue.second,
                      task.onchain_timer_round);
                // TRANSFER(issue.first, issue.second);
                transfer_map.insert(std::make_pair(issue.first, issue.second));
            }
        }

        if (task.action != XTRANSFER_ACTION) {
            // CALL(common::xaccount_address_t{task.contract}, task.action, task.params);
            call_vec.emplace_back(task);
        }
        rm_vec.emplace_back(it->first);
        dispatch_tasks.erase(it);
    }
}

void xtop_zec_reward_contract_new::execute_task() {
    std::map<std::string, std::string> dispatch_tasks;
    try {
        MAP_COPY_GET(XPORPERTY_CONTRACT_TASK_KEY, dispatch_tasks);
    } catch (std::runtime_error & e) {
        xwarn("[xtop_zec_reward_contract_new::update_accumulated_issuance] set XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE error:%s", e.what());
    }
    std::map<std::string, uint64_t> transfer_map;
    std::vector<xreward_dispatch_task> call_vec;
    std::vector<std::string> rm_vec;
    execute_task_internal(dispatch_tasks, transfer_map, call_vec, rm_vec);
    for (auto it = transfer_map.begin(); it != transfer_map.end(); it++) {
        TRANSFER(it->first, it->second);
    }
    for (auto it = call_vec.begin(); it != call_vec.end(); it++) {
        CALL(common::xaccount_address_t{it->contract}, it->action, it->params);
    }
    for (auto it = rm_vec.begin(); it != rm_vec.end(); it++) {
        MAP_REMOVE(XPORPERTY_CONTRACT_TASK_KEY, *it);
    }
}

void xtop_zec_reward_contract_new::print_tasks() {
    std::map<std::string, std::string> dispatch_tasks;
    MAP_COPY_GET(XPORPERTY_CONTRACT_TASK_KEY, dispatch_tasks);

    xreward_dispatch_task task;
    for (auto const & p : dispatch_tasks) {
        xstream_t stream(xcontext_t::instance(), (uint8_t *)p.second.c_str(), (uint32_t)p.second.size());
        task.serialize_from(stream);

        xdbg("[xtop_zec_reward_contract_new::print_tasks] task id: %s, onchain_timer_round: %llu, contract: %s, action: %s\n",
             p.first.c_str(),
             task.onchain_timer_round,
             task.contract.c_str(),
             task.action.c_str());

        if (task.action == XREWARD_CLAIMING_ADD_NODE_REWARD || task.action == XREWARD_CLAIMING_ADD_VOTER_DIVIDEND_REWARD) {
            xstream_t stream_params(xcontext_t::instance(), (uint8_t *)task.params.c_str(), (uint32_t)task.params.size());
            uint64_t onchain_timer_round;
            std::map<std::string, top::xstake::uint128_t> rewards;
            stream_params >> onchain_timer_round;
            stream_params >> rewards;
            for (auto const & r : rewards) {
                xdbg("[xtop_zec_reward_contract_new::print_tasks] account: %s, reward: [%llu, %u]\n",
                     r.first.c_str(),
                     static_cast<uint64_t>(r.second / REWARD_PRECISION),
                     static_cast<uint32_t>(r.second % REWARD_PRECISION));
            }
        } else if (task.action == XTRANSFER_ACTION) {
            std::map<std::string, uint64_t> issuances;
            xstream_t seo_stream(xcontext_t::instance(), (uint8_t *)task.params.c_str(), (uint32_t)task.params.size());
            seo_stream >> issuances;
            for (auto const & issue : issuances) {
                xdbg("[xtop_zec_reward_contract_new::print_tasks] contract account: %s, issuance: %llu\n", issue.first.c_str(), issue.second);
            }
        }
    }
}
#endif

uint32_t xtop_zec_reward_contract_new::calc_current_year(const common::xlogic_time_t time_since_active) const {
    return time_since_active / TIMER_BLOCK_HEIGHT_PER_YEAR + 1;
}

void xtop_zec_reward_contract_new::update_accumulated_issuance_internal(const uint64_t issuance,
                                                                        const std::string & cur_year_issuances_str,
                                                                        const std::string & total_issuances_str,
                                                                        std::string & cur_year_issuances_str_new,
                                                                        std::string & total_issuances_str_new) {
    uint64_t cur_year_issuances{0};
    uint64_t total_issuances{0};
    if (!cur_year_issuances_str.empty()) {
        cur_year_issuances = xstring_utl::touint64(cur_year_issuances_str);
    }
    cur_year_issuances += issuance;

    if (!total_issuances_str.empty()) {
        total_issuances = xstring_utl::touint64(total_issuances_str);
    }
    total_issuances += issuance;

    cur_year_issuances_str_new = xstring_utl::tostring(cur_year_issuances);
    total_issuances_str_new = xstring_utl::tostring(total_issuances);
}

void xtop_zec_reward_contract_new::update_accumulated_issuance(const common::xlogic_time_t timer_round, const uint64_t issuance, const xactivation_record & activation_record) {
    xinfo("[xtop_zec_reward_contract_new::update_accumulated_issuance] actual_issuance: %lu, current_time: %lu", issuance, timer_round);
    XCONTRACT_ENSURE(timer_round >= activation_record.activation_time, "timer_round < activation_time, error");
    auto current_year = calc_current_year(timer_round - activation_record.activation_time);

    // std::string cur_year_issuances_str;
    // std::string total_issuances_str;
    std::string cur_year_issuances_str_new;
    std::string total_issuances_str_new;

    // MAP_GET2(XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE, xstring_utl::tostring(current_year), cur_year_issuances_str);
    // MAP_GET2(XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE, "total", total_issuances_str);
    auto cur_year_issuances_str = m_accumulate_issuance.get(xstring_utl::tostring(current_year));
    auto total_issuances_str = m_accumulate_issuance.get(std::string{"total"});

    update_accumulated_issuance_internal(issuance,
                                         {cur_year_issuances_str.begin(), cur_year_issuances_str.end()},
                                         {total_issuances_str.begin(), total_issuances_str.end()},
                                         cur_year_issuances_str_new,
                                         total_issuances_str_new);

    try {
        // MAP_SET(XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE, xstring_utl::tostring(current_year), cur_year_issuances_str_new);
        m_accumulate_issuance.set(xstring_utl::tostring(current_year), cur_year_issuances_str_new);
    } catch (std::runtime_error & e) {
        xwarn("[xtop_zec_reward_contract_new::update_accumulated_issuance] set XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE error:%s", e.what());
    }
    try {
        // MAP_SET(XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE, "total", total_issuances_str_new);
        m_accumulate_issuance.set(std::string{"total"}, total_issuances_str_new);
    } catch (std::runtime_error & e) {
        xwarn("[xtop_zec_reward_contract_new::update_accumulated_issuance] set XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE error:%s", e.what());
    }
    xinfo("[xtop_zec_reward_contract_new::update_accumulated_issuance] get stored accumulated issuance, year: %d, issuance: [%" PRIu64 ", total issuance: [%" PRIu64
          ", timer round : %" PRIu64 "\n",
          current_year,
          xstring_utl::touint64(cur_year_issuances_str_new),
          xstring_utl::touint64(total_issuances_str_new),
          timer_round);
}

void xtop_zec_reward_contract_new::update_accumulated_record(xaccumulated_reward_record const & record) {
    xinfo("[xtop_zec_reward_contract_new::update_accumulated_record] accumulated_reward_record, current_time: %lu, issued_until_last_year_end: [%lu, %u]",
          record.last_issuance_time,
          static_cast<uint64_t>(record.issued_until_last_year_end / REWARD_PRECISION),
          static_cast<uint32_t>(record.issued_until_last_year_end % REWARD_PRECISION));
    try {
        // STRING_SET(XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE_YEARLY, accumulated_reward_serialize(record));
        m_accumulate_issuance_yearly.set(accumulated_reward_serialize(record));
    } catch (std::runtime_error & e) {
        xwarn("[xtop_zec_reward_contract_new::update_issuance_detail] STRING_SET XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE_YEARLY error:%s", e.what());
    }
}

void xtop_zec_reward_contract_new::update_issuance_detail(xissue_detail const & issue_detail) {
    xinfo(
        "[xtop_zec_reward_contract_new::update_issuance_detail] issue_detail, onchain_timer_round: %llu, m_zec_vote_contract_height: %llu, m_zec_workload_contract_height: %llu, "
        "m_zec_reward_contract_height: %llu, m_edge_reward_ratio: %u, m_archive_reward_ratio: %u m_validator_reward_ratio: %u, m_auditor_reward_ratio: %u, m_vote_reward_ratio: "
        "%u, m_governance_reward_ratio: %u",
        issue_detail.onchain_timer_round,
        issue_detail.m_zec_vote_contract_height,
        issue_detail.m_zec_workload_contract_height,
        issue_detail.m_zec_reward_contract_height,
        issue_detail.m_edge_reward_ratio,
        issue_detail.m_archive_reward_ratio,
        issue_detail.m_validator_reward_ratio,
        issue_detail.m_auditor_reward_ratio,
        issue_detail.m_vote_reward_ratio,
        issue_detail.m_governance_reward_ratio);
    try {
        // STRING_SET(XPROPERTY_REWARD_DETAIL, issue_detail.to_string());
        m_reward_detail.set(issue_detail.to_string());
    } catch (std::runtime_error & e) {
        xwarn("[xtop_zec_reward_contract_new::update_issuance_detail] STRING_SET XPROPERTY_REWARD_DETAIL error:%s", e.what());
    }
}

void xtop_zec_reward_contract_new::get_reward_onchain_param(xreward_onchain_param_t & onchain_param) {
    onchain_param.min_ratio_annual_total_reward = XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_ratio_annual_total_reward);
    onchain_param.additional_issue_year_ratio = XGET_ONCHAIN_GOVERNANCE_PARAMETER(additional_issue_year_ratio);
    onchain_param.edge_reward_ratio = XGET_ONCHAIN_GOVERNANCE_PARAMETER(edge_reward_ratio);
    onchain_param.archive_reward_ratio = XGET_ONCHAIN_GOVERNANCE_PARAMETER(archive_reward_ratio);
    onchain_param.validator_reward_ratio = XGET_ONCHAIN_GOVERNANCE_PARAMETER(validator_reward_ratio);
    onchain_param.auditor_reward_ratio = XGET_ONCHAIN_GOVERNANCE_PARAMETER(auditor_reward_ratio);
    onchain_param.vote_reward_ratio = XGET_ONCHAIN_GOVERNANCE_PARAMETER(vote_reward_ratio);
    onchain_param.governance_reward_ratio = XGET_ONCHAIN_GOVERNANCE_PARAMETER(governance_reward_ratio);
    onchain_param.auditor_group_zero_workload = XGET_ONCHAIN_GOVERNANCE_PARAMETER(auditor_group_zero_workload);
    onchain_param.validator_group_zero_workload = XGET_ONCHAIN_GOVERNANCE_PARAMETER(validator_group_zero_workload);
    xdbg("[xtop_zec_reward_contract_new::get_reward_param] min_ratio_annual_total_reward: %u", onchain_param.min_ratio_annual_total_reward);
    xdbg("[xtop_zec_reward_contract_new::get_reward_param] additional_issue_year_ratio: %u", onchain_param.additional_issue_year_ratio);
    xdbg("[xtop_zec_reward_contract_new::get_reward_param] edge_reward_ratio: %u", onchain_param.edge_reward_ratio);
    xdbg("[xtop_zec_reward_contract_new::get_reward_param] archive_reward_ratio: %u", onchain_param.archive_reward_ratio);
    xdbg("[xtop_zec_reward_contract_new::get_reward_param] validator_reward_ratio: %u", onchain_param.validator_reward_ratio);
    xdbg("[xtop_zec_reward_contract_new::get_reward_param] auditor_reward_ratio: %u", onchain_param.auditor_reward_ratio);
    xdbg("[xtop_zec_reward_contract_new::get_reward_param] vote_reward_ratio: %u", onchain_param.min_ratio_annual_total_reward);
    xdbg("[xtop_zec_reward_contract_new::get_reward_param] governance_reward_ratio: %u", onchain_param.governance_reward_ratio);
    xdbg("[xtop_zec_reward_contract_new::get_reward_param] auditor_group_zero_workload: %u", onchain_param.auditor_group_zero_workload);
    xdbg("[xtop_zec_reward_contract_new::get_reward_param] validator_group_zero_workload: %u", onchain_param.validator_group_zero_workload);
    auto total_ratio = onchain_param.edge_reward_ratio + onchain_param.archive_reward_ratio + onchain_param.validator_reward_ratio + onchain_param.auditor_reward_ratio +
                       onchain_param.vote_reward_ratio + onchain_param.governance_reward_ratio;
    XCONTRACT_ENSURE(total_ratio == 100, "onchain reward total ratio not 100!");
}

void xtop_zec_reward_contract_new::get_reward_param(xreward_onchain_param_t & onchain_param, xreward_property_param_t & property_param) {
    // get onchain param
    get_reward_onchain_param(onchain_param);
    property_param.zec_vote_contract_height = state()->state_height(common::xaccount_address_t{sys_contract_zec_vote_addr});
    property_param.zec_workload_contract_height = state()->state_height(common::xaccount_address_t{sys_contract_zec_workload_addr});
    property_param.zec_reward_contract_height = state()->state_height(common::xaccount_address_t{sys_contract_zec_reward_addr});

    xinfo("[xtop_zec_reward_contract_new::get_reward_param] m_zec_vote_contract_height: %u, m_zec_workload_contract_height: %u, m_zec_reward_contract_height: %u",
          property_param.zec_vote_contract_height,
          property_param.zec_workload_contract_height,
          property_param.zec_reward_contract_height);
    // get map nodes
    // std::map<std::string, std::string> map_nodes;
    auto const last_read_height = static_cast<std::uint64_t>(std::stoull(m_last_read_rec_reg_contract_height.value()));
    // GET_MAP_PROPERTY(XPORPERTY_CONTRACT_REG_KEY, map_nodes, last_read_height, sys_contract_rec_registration_addr);
    contract_common::properties::xmap_property_t<std::string, std::string> reg_prop{XPORPERTY_CONTRACT_REG_KEY, this};
    auto map_nodes = reg_prop.clone(common::xaccount_address_t{sys_contract_rec_registration_addr});
    XCONTRACT_ENSURE(map_nodes.size() != 0, "MAP GET PROPERTY XPORPERTY_CONTRACT_REG_KEY empty");
    xinfo("[xtop_zec_reward_contract_new::get_reward_param] last_read_height: %llu, map_nodes size: %d", last_read_height, map_nodes.size());
    for (auto const & entity : map_nodes) {
        auto const & account = entity.first;
        auto const & value_str = entity.second;
        xreg_node_info node;
        xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
        node.serialize_from(stream);
        common::xaccount_address_t address{account};
        property_param.map_nodes[address] = node;
    }
    // get workload
    auto auditor_clusters_workloads = m_auditor_workload.value();
    auto validator_clusters_workloads = m_validator_workload.value();
    // MAP_COPY_GET(XPORPERTY_CONTRACT_WORKLOAD_KEY, auditor_clusters_workloads);
    // MAP_COPY_GET(XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY, validator_clusters_workloads);
    m_auditor_workload.clear();
    m_validator_workload.clear();
    // CLEAR(enum_type_t::map, XPORPERTY_CONTRACT_WORKLOAD_KEY);
    // CLEAR(enum_type_t::map, XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY);
    for (auto it = auditor_clusters_workloads.begin(); it != auditor_clusters_workloads.end(); it++) {
        auto const & key_str = it->first;
        common::xcluster_address_t cluster_address;
        xstream_t key_stream(xcontext_t::instance(), (uint8_t *)key_str.data(), key_str.size());
        key_stream >> cluster_address;
        auto const & value_str = it->second;
        cluster_workload_t workload;
        xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
        workload.serialize_from(stream);
        property_param.auditor_workloads_detail[cluster_address] = workload;
    }
    for (auto it = validator_clusters_workloads.begin(); it != validator_clusters_workloads.end(); it++) {
        auto const & key_str = it->first;
        common::xcluster_address_t cluster_address;
        xstream_t key_stream(xcontext_t::instance(), (uint8_t *)key_str.data(), key_str.size());
        key_stream >> cluster_address;
        auto const & value_str = it->second;
        cluster_workload_t workload;
        xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
        workload.serialize_from(stream);
        property_param.validator_workloads_detail[cluster_address] = workload;
    }
    xinfo("[xtop_zec_reward_contract_new::get_reward_param] auditor_group_count: %d, validator_group_count: %d",
          auditor_clusters_workloads.size(),
          validator_clusters_workloads.size());
    // get vote
    // std::map<std::string, std::string> contract_auditor_votes;
    // MAP_COPY_GET(XPORPERTY_CONTRACT_TICKETS_KEY, contract_auditor_votes, sys_contract_zec_vote_addr);
    contract_common::properties::xmap_property_t<std::string, std::string> tickets_prop{XPORPERTY_CONTRACT_TICKETS_KEY, this};
    auto contract_auditor_votes = tickets_prop.clone(common::xaccount_address_t{sys_contract_zec_vote_addr});
    for (auto & contract_auditor_vote : contract_auditor_votes) {
        auto const & contract = contract_auditor_vote.first;
        auto const & auditor_votes_str = contract_auditor_vote.second;
        std::map<std::string, std::string> auditor_votes;
        xstream_t stream(xcontext_t::instance(), (uint8_t *)auditor_votes_str.data(), auditor_votes_str.size());
        stream >> auditor_votes;
        common::xaccount_address_t address{contract};
        std::map<common::xaccount_address_t, uint64_t> votes_detail;
        for (auto & votes : auditor_votes) {
            votes_detail.insert({common::xaccount_address_t{votes.first}, xstring_utl::touint64(votes.second)});
        }
        property_param.votes_detail[address] = votes_detail;
    }
    xinfo("[xtop_zec_reward_contract_new::get_reward_param] votes_detail_count: %d", property_param.votes_detail.size());
    // get accumulated reward
    // std::string value_str = STRING_GET(XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE_YEARLY);
    auto value_str = m_accumulate_issuance_yearly.value();
    if (value_str.size() != 0) {
        xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.c_str(), (uint32_t)value_str.size());
        property_param.accumulated_reward_record.serialize_from(stream);
    }
    xinfo("[xtop_zec_reward_contract_new::get_reward_param] accumulated_reward_record: %lu, [%lu, %u]",
          property_param.accumulated_reward_record.last_issuance_time,
          static_cast<uint64_t>(property_param.accumulated_reward_record.issued_until_last_year_end / REWARD_PRECISION),
          static_cast<uint32_t>(property_param.accumulated_reward_record.issued_until_last_year_end % REWARD_PRECISION));
}

top::xstake::uint128_t xtop_zec_reward_contract_new::calc_issuance_internal(top::xstake::uint128_t issued_until_last_year_end,
                                                                            top::xstake::uint128_t minimum_issuance,
                                                                            uint32_t issuance_rate) {
    top::xstake::uint128_t reserve_reward = 0;
    top::xstake::uint128_t total_reserve = static_cast<top::xstake::uint128_t>(TOTAL_RESERVE) * REWARD_PRECISION;
    if (total_reserve > issued_until_last_year_end) {
        reserve_reward = std::max((total_reserve - issued_until_last_year_end) * issuance_rate / 100, minimum_issuance);
    } else {
        reserve_reward = minimum_issuance;
    }
    return reserve_reward;
}

/**
 * @brief calculate issuance
 *
 * @param total_height currnet time - actived time
 * @param min_ratio_annual_total_reward onchain_parameter
 * @param issuance_rate onchain_parameter
 * @param record accumulated reward record
 * @return total reward issuance
 */
top::xstake::uint128_t xtop_zec_reward_contract_new::calc_total_issuance(const common::xlogic_time_t total_height,
                                                                         const uint32_t min_ratio_annual_total_reward,
                                                                         const uint32_t issuance_rate,
                                                                         xaccumulated_reward_record & record) {
    auto minimum_issuance = static_cast<top::xstake::uint128_t>(TOTAL_ISSUANCE) * min_ratio_annual_total_reward / 100 * REWARD_PRECISION;
    uint64_t & last_issuance_time = record.last_issuance_time;
    top::xstake::uint128_t & issued_until_last_year_end = record.issued_until_last_year_end;
    
    if (0 == total_height) {
        return 0;
    }

    top::xstake::uint128_t additional_issuance = 0;
    uint64_t issued_clocks = 0;  // from last issuance to last year end

    uint64_t call_duration_height = total_height - last_issuance_time;
    uint32_t current_year = total_height / TIMER_BLOCK_HEIGHT_PER_YEAR + 1;
    uint32_t last_issuance_year = last_issuance_time / TIMER_BLOCK_HEIGHT_PER_YEAR + 1;
    xdbg("[xtop_zec_reward_contract_new::calc_issuance] last_issuance_time: %llu, current_year: %u, last_issuance_year:%u", last_issuance_time, current_year, last_issuance_year);
    while (last_issuance_year < current_year) {
        uint64_t remaining_clocks = TIMER_BLOCK_HEIGHT_PER_YEAR - last_issuance_time % TIMER_BLOCK_HEIGHT_PER_YEAR;
        if (remaining_clocks > 0) {
            auto reserve_reward = calc_issuance_internal(issued_until_last_year_end, minimum_issuance, issuance_rate);
            additional_issuance += reserve_reward * remaining_clocks / TIMER_BLOCK_HEIGHT_PER_YEAR;
            xinfo(
                "[xtop_zec_reward_contract_new::calc_issuance] cross year, last_issuance_year: %u, reserve_reward: [%llu, %u], remaining_clocks: %llu, issued_clocks: %u, "
                "additional_issuance: [%llu, %u], issued_until_last_year_end: [%llu, %u]",
                last_issuance_year,
                static_cast<uint64_t>(reserve_reward / REWARD_PRECISION),
                static_cast<uint32_t>(reserve_reward % REWARD_PRECISION),
                remaining_clocks,
                issued_clocks,
                static_cast<uint64_t>(additional_issuance / REWARD_PRECISION),
                static_cast<uint32_t>(additional_issuance % REWARD_PRECISION),
                static_cast<uint64_t>(issued_until_last_year_end / REWARD_PRECISION),
                static_cast<uint32_t>(issued_until_last_year_end % REWARD_PRECISION));
            issued_clocks += remaining_clocks;
            last_issuance_time += remaining_clocks;
            issued_until_last_year_end += reserve_reward;
        }
        last_issuance_year++;
    }

    top::xstake::uint128_t reserve_reward = 0;
    if (call_duration_height > issued_clocks) {
        reserve_reward = calc_issuance_internal(issued_until_last_year_end, minimum_issuance, issuance_rate);
        additional_issuance += reserve_reward * (call_duration_height - issued_clocks) / TIMER_BLOCK_HEIGHT_PER_YEAR;
    }

    xinfo("[xtop_zec_reward_contract_new::calc_issuance] additional_issuance: [%" PRIu64 ", %u], call_duration_height: %" PRId64 ", issued_clocks: %" PRId64
          ", total_height: %" PRId64 ", current_year: %" PRIu32 ", last_issuance_year: %" PRIu32
          ", pid: %d"
          ", reserve_reward: [%llu, %u], last_issuance_time: %llu, issued_until_last_year_end: [%llu, %u], TIMER_BLOCK_HEIGHT_PER_YEAR: %llu",
          static_cast<uint64_t>(additional_issuance / REWARD_PRECISION),
          static_cast<uint32_t>(additional_issuance % REWARD_PRECISION),
          call_duration_height,
          issued_clocks,
          total_height,
          current_year,
          last_issuance_year,
          getpid(),
          static_cast<uint64_t>(reserve_reward / REWARD_PRECISION),
          static_cast<uint32_t>(reserve_reward % REWARD_PRECISION),
          last_issuance_time,
          static_cast<uint64_t>(issued_until_last_year_end / REWARD_PRECISION),
          static_cast<uint32_t>(issued_until_last_year_end % REWARD_PRECISION),
          TIMER_BLOCK_HEIGHT_PER_YEAR);

    last_issuance_time = total_height;
    return additional_issuance;
}

std::vector<std::vector<uint32_t>> xtop_zec_reward_contract_new::calc_role_nums(std::map<common::xaccount_address_t, xreg_node_info> const & map_nodes) {
    std::vector<std::vector<uint32_t>> role_nums;
    // vector init
    role_nums.resize(role_type_idx_num);
    for (uint i = 0; i < role_nums.size(); i++) {
        role_nums[i].resize(num_type_idx_num);
        for (uint j = 0; j < role_nums[i].size(); j++) {
            role_nums[i][j] = 0;
        }
    }

    // calc nums
    for (auto const & entity : map_nodes) {
        auto const & account = entity.first;
        auto const & node = entity.second;

#if 0
        // old statistical method
        if (node.get_deposit() > 0 && node.is_edge_node()) {
            role_nums[edger_idx][valid_idx]++;
        }
        if (node.get_deposit() > 0 && node.is_valid_archive_node()) {
            role_nums[archiver_idx][valid_idx]++;
        }
        if (node.get_deposit() > 0 && node.is_valid_auditor_node()) {
            role_nums[auditor_idx][valid_idx]++;
        }
        if (node.get_deposit() > 0 && node.is_validator_node()) {
            role_nums[validator_idx][valid_idx]++;
        }
#else
        // now statistical method
        if (node.is_edge_node()) {
            // total edger nums
            role_nums[edger_idx][total_idx]++;
            // valid edger nums
            if (VALID_EDGER(node)) {
                role_nums[edger_idx][valid_idx]++;
            }
            // deposit zero edger nums
            if (node.get_deposit() == 0) {
                role_nums[edger_idx][deposit_zero_num]++;
            }
        }
        if (node.is_archive_node()) {
            // total archiver nums
            role_nums[archiver_idx][total_idx]++;
            // valid archiver nums
            if (VALID_ARCHIVER(node)) {
                role_nums[archiver_idx][valid_idx]++;
            }
            // deposit zero archiver nums
            if (node.get_deposit() == 0) {
                role_nums[archiver_idx][deposit_zero_num]++;
            }
        }
        if (node.is_auditor_node()) {
            // total auditor nums
            role_nums[auditor_idx][total_idx]++;
            // valid auditor nums
            if (VALID_AUDITOR(node)) {
                role_nums[auditor_idx][valid_idx]++;
            }
            // deposit zero auditor nums
            if (node.get_deposit() == 0) {
                role_nums[auditor_idx][deposit_zero_num]++;
            }
        }
        if (node.is_validator_node()) {
            // total validator nums
            role_nums[validator_idx][total_idx]++;
            // valid validator nums
            if (VALID_VALIDATOR(node)) {
                role_nums[validator_idx][valid_idx]++;
            }
            // deposit zero validator nums
            if (node.get_deposit() == 0) {
                role_nums[validator_idx][deposit_zero_num]++;
            }
        }
#endif
    }

    return role_nums;
}

uint64_t xtop_zec_reward_contract_new::calc_votes(std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, uint64_t>> const & votes_detail,
                                                  std::map<common::xaccount_address_t, xreg_node_info> & map_nodes,
                                                  std::map<common::xaccount_address_t, uint64_t> & account_votes) {
    for (auto & entity : map_nodes) {
        auto const & account = entity.first;
        auto & node = entity.second;
        uint64_t node_total_votes = 0;
        for (auto const & vote_detail : votes_detail) {
            auto const & contract = vote_detail.first;
            auto const & vote = vote_detail.second;
            auto iter = vote.find(account);
            if (iter != vote.end()) {
                account_votes[account] += iter->second;
                node_total_votes += iter->second;
            }
        }
        node.m_vote_amount = node_total_votes;
        xdbg("[xtop_zec_reward_contract_new::calc_votes] map_nodes: account: %s, deposit: %llu, node_type: %s, votes: %llu",
             node.m_account.c_str(),
             node.get_deposit(),
             node.m_genesis_node ? "advance,validator,edge" : common::to_string(node.m_registered_role).c_str(),
             node.m_vote_amount);
    }
    // valid auditor only
    uint64_t total_votes = 0;
    for (auto const & entity : votes_detail) {
        auto const & vote = entity.second;

        for (auto const & entity2 : vote) {
            xreg_node_info node;
            auto it = map_nodes.find(common::xaccount_address_t{entity2.first});
            if (it == map_nodes.end()) {
                xwarn("[xtop_zec_reward_contract_new::calc_votes] account %s not in map_nodes", entity2.first.c_str());
                continue;
            } else {
                node = it->second;
            }

            if (node.get_deposit() > 0 && node.is_valid_auditor_node()) {
                total_votes += entity2.second;
            }
        }
    }

    return total_votes;
}

// std::map<common::xaccount_address_t, uint64_t> xtop_zec_reward_contract_new::calc_votes(
//     std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, uint64_t>> const & votes_detail,
//     std::map<common::xaccount_address_t, xreg_node_info> const & map_nodes) {
//     std::map<common::xaccount_address_t, uint64_t> account_votes;
//     for (auto & entity : map_nodes) {
//         auto const & account = entity.first;
//         auto & node = entity.second;
//         for (auto const & vote_detail : votes_detail) {
//             auto const & contract = vote_detail.first;
//             auto const & vote = vote_detail.second;
//             auto iter = vote.find(account);
//             if (iter != vote.end()) {
//                 account_votes[account] += iter->second;
//             }
//         }
//     }

//     return account_votes;
// }

top::xstake::uint128_t xtop_zec_reward_contract_new::calc_zero_workload_reward(bool is_auditor,
                                                                               std::map<common::xcluster_address_t, cluster_workload_t> & workloads_detail,
                                                                               const uint32_t zero_workload,
                                                                               const top::xstake::uint128_t group_reward,
                                                                               std::vector<std::string> & zero_workload_account) {
    top::xstake::uint128_t zero_workload_rewards = 0;

    for (auto it = workloads_detail.begin(); it != workloads_detail.end();) {
        if (it->second.cluster_total_workload <= zero_workload) {
            xinfo(
                "[xtop_zec_reward_contract_new::calc_zero_workload_reward] is auditor %d, cluster id: %s, cluster size: %lu, cluster_total_workload: %u <= zero_workload_val %u "
                "and will "
                "be ignored\n",
                is_auditor,
                it->first.to_string().c_str(),
                it->second.m_leader_count.size(),
                it->second.cluster_total_workload,
                zero_workload);
            for (auto it2 = it->second.m_leader_count.begin(); it2 != it->second.m_leader_count.end(); it2++) {
                zero_workload_account.push_back(it2->first);
            }
            workloads_detail.erase(it++);
            zero_workload_rewards += group_reward;
        } else {
            it++;
        }
    }

    return zero_workload_rewards;
}

top::xstake::uint128_t xtop_zec_reward_contract_new::calc_invalid_workload_group_reward(bool is_auditor,
                                                                                        std::map<common::xaccount_address_t, xreg_node_info> const & map_nodes,
                                                                                        const top::xstake::uint128_t group_reward,
                                                                                        std::map<common::xcluster_address_t, cluster_workload_t> & workloads_detail) {
    top::xstake::uint128_t invalid_group_reward = 0;

    for (auto it = workloads_detail.begin(); it != workloads_detail.end();) {
        for (auto it2 = it->second.m_leader_count.begin(); it2 != it->second.m_leader_count.end();) {
            xreg_node_info node;
            auto it3 = map_nodes.find(common::xaccount_address_t{it2->first});
            if (it3 == map_nodes.end()) {
                xinfo("[xtop_zec_reward_contract_new::calc_invalid_workload_group_reward] account: %s not in map nodes", it2->first.c_str());
                it->second.cluster_total_workload -= it2->second;
                it->second.m_leader_count.erase(it2++);
                continue;
            } else {
                node = it3->second;
            }
            if (is_auditor) {
                if (node.get_deposit() == 0 || !node.is_valid_auditor_node()) {
                    xinfo("[xtop_zec_reward_contract_new::calc_invalid_workload_group_reward] account: %s is not a valid auditor, deposit: %llu, votes: %llu",
                          it2->first.c_str(),
                          node.get_deposit(),
                          node.m_vote_amount);
                    it->second.cluster_total_workload -= it2->second;
                    it->second.m_leader_count.erase(it2++);
                } else {
                    it2++;
                }
            } else {
                if (node.get_deposit() == 0 || !node.is_validator_node()) {
                    xinfo("[xtop_zec_reward_contract_new::calc_invalid_workload_group_reward] account: %s is not a valid validator, deposit: %lu",
                          it2->first.c_str(),
                          node.get_deposit());
                    it->second.cluster_total_workload -= it2->second;
                    it->second.m_leader_count.erase(it2++);
                } else {
                    it2++;
                }
            }
        }
        if (it->second.m_leader_count.size() == 0) {
            xinfo("[xtop_zec_reward_contract_new::calc_invalid_workload_group_reward] is auditor %d, cluster id: %s, all node invalid, will be ignored\n",
                  is_auditor,
                  it->first.to_string().c_str());
            workloads_detail.erase(it++);
            invalid_group_reward += group_reward;
        } else {
            it++;
        }
    }

    return invalid_group_reward;
}

void xtop_zec_reward_contract_new::calc_edge_workload_rewards(xreg_node_info const & node,
                                                              std::vector<uint32_t> const & edge_num,
                                                              const top::xstake::uint128_t edge_workload_rewards,
                                                              top::xstake::uint128_t & reward_to_self) {
    XCONTRACT_ENSURE(edge_num.size() == num_type_idx_num, "edge_num not 3");
    auto divide_num = edge_num[valid_idx];
    reward_to_self = 0;
    if (0 == divide_num) {
        return;
    }
    if (!VALID_EDGER(node)) {
        return;
    }
    reward_to_self = edge_workload_rewards / divide_num;
    xdbg("[xtop_zec_reward_contract_new::calc_edge_worklaod_rewards] account: %s, edge reward: [%llu, %u]",
         node.m_account.c_str(),
         static_cast<uint64_t>(reward_to_self / REWARD_PRECISION),
         static_cast<uint32_t>(reward_to_self % REWARD_PRECISION));

    return;
}

void xtop_zec_reward_contract_new::calc_archive_workload_rewards(xreg_node_info const & node,
                                                                 std::vector<uint32_t> const & archive_num,
                                                                 const top::xstake::uint128_t archive_workload_rewards,
                                                                 top::xstake::uint128_t & reward_to_self) {
    XCONTRACT_ENSURE(archive_num.size() == num_type_idx_num, "archive_num not 3");
    auto divide_num = archive_num[valid_idx];
    reward_to_self = 0;
    if (0 == divide_num) {
        return;
    }
    if (!VALID_ARCHIVER(node)) {
        return;
    }
    reward_to_self = archive_workload_rewards / divide_num;
    xdbg("[xtop_zec_reward_contract_new::calc_archive_worklaod_rewards] account: %s, archive reward: [%llu, %u]",
         node.m_account.c_str(),
         static_cast<uint64_t>(reward_to_self / REWARD_PRECISION),
         static_cast<uint32_t>(reward_to_self % REWARD_PRECISION));

    return;
}

void xtop_zec_reward_contract_new::calc_auditor_workload_rewards(xreg_node_info const & node,
                                                                 std::vector<uint32_t> const & auditor_num,
                                                                 std::map<common::xcluster_address_t, cluster_workload_t> const & auditor_workloads_detail,
                                                                 const top::xstake::uint128_t auditor_group_workload_rewards,
                                                                 top::xstake::uint128_t & reward_to_self) {
    xdbg("[xtop_zec_reward_contract_new::calc_auditor_worklaod_rewards] account: %s, %d clusters report workloads, cluster_total_rewards: [%llu, %u]\n",
         node.m_account.c_str(),
         auditor_workloads_detail.size(),
         static_cast<uint64_t>(auditor_group_workload_rewards / REWARD_PRECISION),
         static_cast<uint32_t>(auditor_group_workload_rewards % REWARD_PRECISION));
    XCONTRACT_ENSURE(auditor_num.size() == num_type_idx_num, "auditor_num array not 3");
    auto divide_num = auditor_num[valid_idx];
    reward_to_self = 0;
    if (0 == divide_num) {
        return;
    }
    if (!VALID_AUDITOR(node)) {
        return;
    }
    for (auto & workload : auditor_workloads_detail) {
        xdbg("[xtop_zec_reward_contract_new::calc_auditor_worklaod_rewards] account: %s, cluster id: %s, cluster size: %d, cluster_total_workload: %u\n",
             node.m_account.c_str(),
             workload.first.to_string().c_str(),
             workload.second.m_leader_count.size(),
             workload.second.cluster_total_workload);
        auto it = workload.second.m_leader_count.find(node.m_account.value());
        if (it != workload.second.m_leader_count.end()) {
            auto const & work = it->second;
            reward_to_self += auditor_group_workload_rewards * work / workload.second.cluster_total_workload;
            xdbg(
                "[xtop_zec_reward_contract_new::calc_auditor_worklaod_rewards] account: %s, cluster_id: %s, work: %d, total_workload: %u, cluster_total_rewards: [%lu, %u], "
                "reward: [%lu, "
                "%u]\n",
                node.m_account.c_str(),
                workload.first.to_string().c_str(),
                work,
                workload.second.cluster_total_workload,
                static_cast<uint64_t>(auditor_group_workload_rewards / REWARD_PRECISION),
                static_cast<uint32_t>(auditor_group_workload_rewards % REWARD_PRECISION),
                static_cast<uint64_t>(reward_to_self / REWARD_PRECISION),
                static_cast<uint32_t>(reward_to_self % REWARD_PRECISION));
        }
    }

    return;
}

void xtop_zec_reward_contract_new::calc_validator_workload_rewards(xreg_node_info const & node,
                                                                   std::vector<uint32_t> const & validator_num,
                                                                   std::map<common::xcluster_address_t, cluster_workload_t> const & validator_workloads_detail,
                                                                   const top::xstake::uint128_t validator_group_workload_rewards,
                                                                   top::xstake::uint128_t & reward_to_self) {
    xdbg("[xtop_zec_reward_contract_new::calc_validator_worklaod_rewards] account: %s, %d clusters report workloads, cluster_total_rewards: [%llu, %u]\n",
         node.m_account.c_str(),
         validator_workloads_detail.size(),
         static_cast<uint64_t>(validator_group_workload_rewards / REWARD_PRECISION),
         static_cast<uint32_t>(validator_group_workload_rewards % REWARD_PRECISION));
    XCONTRACT_ENSURE(validator_num.size() == num_type_idx_num, "validator_num array not 3");
    auto divide_num = validator_num[valid_idx];
    reward_to_self = 0;
    if (0 == divide_num) {
        return;
    }
    if (!VALID_VALIDATOR(node)) {
        return;
    }
    for (auto & workload : validator_workloads_detail) {
        xdbg("[xtop_zec_reward_contract_new::calc_validator_worklaod_rewards] account: %s, cluster id: %s, cluster size: %d, cluster_total_workload: %u\n",
             node.m_account.c_str(),
             workload.first.to_string().c_str(),
             workload.second.m_leader_count.size(),
             workload.second.cluster_total_workload);
        auto it = workload.second.m_leader_count.find(node.m_account.value());
        if (it != workload.second.m_leader_count.end()) {
            auto const & work = it->second;
            reward_to_self += validator_group_workload_rewards * work / workload.second.cluster_total_workload;
            xdbg(
                "[xtop_zec_reward_contract_new::calc_validator_worklaod_rewards] account: %s, cluster_id: %s, work: %d, total_workload: %d, cluster_total_rewards: [%llu, %u], "
                "reward: "
                "[%llu, %u]\n",
                node.m_account.c_str(),
                workload.first.to_string().c_str(),
                work,
                workload.second.cluster_total_workload,
                static_cast<uint64_t>(validator_group_workload_rewards / REWARD_PRECISION),
                static_cast<uint32_t>(validator_group_workload_rewards % REWARD_PRECISION),
                static_cast<uint64_t>(reward_to_self / REWARD_PRECISION),
                static_cast<uint32_t>(reward_to_self % REWARD_PRECISION));
        }
    }

    return;
}

void xtop_zec_reward_contract_new::calc_vote_reward(xreg_node_info const & node,
                                                    const uint64_t auditor_total_votes,
                                                    const top::xstake::uint128_t vote_rewards,
                                                    top::xstake::uint128_t & reward_to_self) {
    reward_to_self = 0;
    if (node.is_valid_auditor_node() && node.get_deposit() > 0) {
        XCONTRACT_ENSURE(auditor_total_votes > 0, "total_votes = 0 while valid auditor num > 0");
        reward_to_self = vote_rewards * node.m_vote_amount / auditor_total_votes;
        xdbg("[xtop_zec_reward_contract_new::calc_nodes_rewards_v4] account: %s, node_vote_reward: [%llu, %u], node deposit: %llu, auditor_total_votes: %llu, node_votes: %llu",
             node.m_account.c_str(),
             static_cast<uint64_t>(reward_to_self / REWARD_PRECISION),
             static_cast<uint32_t>(reward_to_self % REWARD_PRECISION),
             node.get_deposit(),
             auditor_total_votes,
             node.m_vote_amount);
    }
    return;
}

void xtop_zec_reward_contract_new::calc_table_node_dividend_detail(
    common::xaccount_address_t const & table_address,
    common::xaccount_address_t const & account,
    top::xstake::uint128_t const & reward,
    uint64_t node_total_votes,
    std::map<common::xaccount_address_t, uint64_t> const & vote_detail,
    std::map<common::xaccount_address_t, top::xstake::uint128_t> & table_total_rewards,
    std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, top::xstake::uint128_t>> & table_node_dividend_detail) {
    auto iter = vote_detail.find(account);
    if (iter != vote_detail.end()) {
        auto reward_to_voter = reward * iter->second / node_total_votes;
        xdbg(
            "[calc_table_node_dividend_detail] account: %s, contract: %s, table votes: %llu, adv_total_votes: %llu, adv_reward_to_voters: [%llu, %u], adv_reward_to_contract: "
            "[%llu, %u]\n",
            account.c_str(),
            iter->first.c_str(),
            iter->second,
            node_total_votes,
            static_cast<uint64_t>(reward / REWARD_PRECISION),
            static_cast<uint32_t>(reward % REWARD_PRECISION),
            static_cast<uint64_t>(reward_to_voter / REWARD_PRECISION),
            static_cast<uint32_t>(reward_to_voter % REWARD_PRECISION));
        if (reward_to_voter > 0) {
            table_total_rewards[table_address] += reward_to_voter;
            table_node_dividend_detail[table_address][account] += reward_to_voter;
        }
    }
}

void xtop_zec_reward_contract_new::calc_table_node_reward_detail(
    common::xaccount_address_t const & table_address,
    common::xaccount_address_t const & account,
    top::xstake::uint128_t node_reward,
    std::map<common::xaccount_address_t, top::xstake::uint128_t> & table_total_rewards,
    std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, top::xstake::uint128_t>> & table_node_reward_detail) {
    table_total_rewards[table_address] += node_reward;
    table_node_reward_detail[table_address][account] = node_reward;
    xdbg("[xtop_zec_reward_contract_new::calc_table_node_reward_detail] node reward, pid:%d, reward_contract: %s, account: %s, reward: [%llu, %u]\n",
         getpid(),
         table_address.c_str(),
         account.c_str(),
         static_cast<uint64_t>(node_reward / REWARD_PRECISION),
         static_cast<uint32_t>(node_reward % REWARD_PRECISION));
}

common::xaccount_address_t xtop_zec_reward_contract_new::calc_table_contract_address(common::xaccount_address_t const & account) {
    uint32_t table_id = 0;

    if (is_sys_sharding_contract_address(account) && !xdatautil::extract_table_id_from_address(account.value(), table_id)) {
        xwarn("[xtop_zec_reward_contract_new::calc_table_contract_address] EXTRACT_TABLE_ID failed, node reward pid: %d, account: %s\n", getpid(), account.c_str());
        return {};
    }
    // auto const & table_address = CALC_CONTRACT_ADDRESS(sys_contract_sharding_reward_claiming_addr, table_id);
    // auto const & table_address = contract::xcontract_address_map_t::calc_cluster_address(common::xaccount_address_t{ sys_contract_sharding_reward_claiming_addr },
    // table_id).value()
    auto const & table_address = data::make_address_by_prefix_and_subaddr(sys_contract_sharding_reward_claiming_addr, uint16_t(table_id));
    return common::xaccount_address_t{table_address};
}

void xtop_zec_reward_contract_new::calc_nodes_rewards_v5(const common::xlogic_time_t current_time,
                                                         const common::xlogic_time_t activation_time,
                                                         const xreward_onchain_param_t & onchain_param,
                                                         xreward_property_param_t & property_param,
                                                         xissue_detail & issue_detail,
                                                         std::map<common::xaccount_address_t, top::xstake::uint128_t> & node_reward_detail,
                                                         std::map<common::xaccount_address_t, top::xstake::uint128_t> & node_dividend_detail,
                                                         top::xstake::uint128_t & community_reward) {
    // step 1: calculate issuance
    auto issue_time_length = current_time - activation_time;
    auto total_issuance =
        calc_total_issuance(issue_time_length, onchain_param.min_ratio_annual_total_reward, onchain_param.additional_issue_year_ratio, property_param.accumulated_reward_record);
    XCONTRACT_ENSURE(total_issuance > 0, "total issuance = 0");
    auto edge_workload_rewards = total_issuance * onchain_param.edge_reward_ratio / 100;
    auto archive_workload_rewards = total_issuance * onchain_param.archive_reward_ratio / 100;
    auto auditor_total_workload_rewards = total_issuance * onchain_param.auditor_reward_ratio / 100;
    auto validator_total_workload_rewards = total_issuance * onchain_param.validator_reward_ratio / 100;
    auto vote_rewards = total_issuance * onchain_param.vote_reward_ratio / 100;
    auto governance_rewards = total_issuance * onchain_param.governance_reward_ratio / 100;
    community_reward = governance_rewards;
    auto auditor_group_count = property_param.auditor_workloads_detail.size();
    auto validator_group_count = property_param.validator_workloads_detail.size();
    top::xstake::uint128_t auditor_group_workload_rewards = 0;
    top::xstake::uint128_t validator_group_workload_rewards = 0;
    if (auditor_group_count != 0) {
        auditor_group_workload_rewards = auditor_total_workload_rewards / auditor_group_count;
    }
    if (validator_group_count != 0) {
        validator_group_workload_rewards = validator_total_workload_rewards / validator_group_count;
    }

    // fill issue_detail
    {
        issue_detail.onchain_timer_round = current_time;
        issue_detail.m_zec_vote_contract_height = property_param.zec_vote_contract_height;
        issue_detail.m_zec_workload_contract_height = property_param.zec_workload_contract_height;
        issue_detail.m_zec_reward_contract_height = property_param.zec_reward_contract_height;
        issue_detail.m_auditor_group_count = property_param.auditor_workloads_detail.size();
        issue_detail.m_validator_group_count = property_param.validator_workloads_detail.size();
        issue_detail.m_edge_reward_ratio = onchain_param.edge_reward_ratio;
        issue_detail.m_archive_reward_ratio = onchain_param.archive_reward_ratio;
        issue_detail.m_validator_reward_ratio = onchain_param.validator_reward_ratio;
        issue_detail.m_auditor_reward_ratio = onchain_param.auditor_reward_ratio;
        issue_detail.m_vote_reward_ratio = onchain_param.vote_reward_ratio;
        issue_detail.m_governance_reward_ratio = onchain_param.governance_reward_ratio;
    }

    // step 2: calculate different votes and role nums
    std::map<common::xaccount_address_t, uint64_t> account_votes;
    auto auditor_total_votes = calc_votes(property_param.votes_detail, property_param.map_nodes, account_votes);
    std::vector<std::vector<uint32_t>> role_nums = calc_role_nums(property_param.map_nodes);

    xinfo(
        "[xtop_zec_reward_contract_new::calc_nodes_rewards] issue_time_length: %llu, "
        "total issuance: [%llu, %u], "
        "edge workload rewards: [%llu, %u], total edge num: %d, valid edge num: %d, "
        "archive workload rewards: [%llu, %u], total archive num: %d, valid archive num: %d, "
        "auditor workload rewards: [%llu, %u], auditor workload grop num: %d, auditor group workload rewards: [%llu, %u], total auditor num: %d, valid auditor num: %d, "
        "validator workload rewards: [%llu, %u], validator workload grop num: %d, validator group workload rewards: [%llu, %u], total validator num: %d, valid validator num: %d,  "
        "vote rewards: [%llu, %u], "
        "governance rewards: [%llu, %u], ",
        issue_time_length,
        static_cast<uint64_t>(total_issuance / REWARD_PRECISION),
        static_cast<uint32_t>(total_issuance % REWARD_PRECISION),
        static_cast<uint64_t>(edge_workload_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(edge_workload_rewards % REWARD_PRECISION),
        role_nums[edger_idx][total_idx],
        role_nums[edger_idx][valid_idx],
        static_cast<uint64_t>(archive_workload_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(archive_workload_rewards % REWARD_PRECISION),
        role_nums[archiver_idx][total_idx],
        role_nums[archiver_idx][valid_idx],
        static_cast<uint64_t>(auditor_total_workload_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(auditor_total_workload_rewards % REWARD_PRECISION),
        auditor_group_count,
        static_cast<uint64_t>(auditor_group_workload_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(auditor_group_workload_rewards % REWARD_PRECISION),
        role_nums[auditor_idx][total_idx],
        role_nums[auditor_idx][valid_idx],
        static_cast<uint64_t>(validator_total_workload_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(validator_total_workload_rewards % REWARD_PRECISION),
        validator_group_count,
        static_cast<uint64_t>(validator_group_workload_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(validator_group_workload_rewards % REWARD_PRECISION),
        role_nums[validator_idx][total_idx],
        role_nums[validator_idx][valid_idx],
        static_cast<uint64_t>(vote_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(vote_rewards % REWARD_PRECISION),
        static_cast<uint64_t>(governance_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(governance_rewards % REWARD_PRECISION));
    // step 3: calculate reward
    if (0 == role_nums[edger_idx][valid_idx]) {
        community_reward += edge_workload_rewards;
    }
    if (0 == role_nums[archiver_idx][valid_idx]) {
        community_reward += archive_workload_rewards;
    }
    if (0 == role_nums[auditor_idx][valid_idx]) {
        community_reward += vote_rewards;
    }
    // 3.1 zero workload
    std::vector<std::string> zero_workload_account;
    // node which deposit = 0 whose rewards do not given to community yet
    if (auditor_group_workload_rewards != 0) {
        community_reward += calc_invalid_workload_group_reward(true, property_param.map_nodes, auditor_group_workload_rewards, property_param.auditor_workloads_detail);
        community_reward += calc_zero_workload_reward(
            true, property_param.auditor_workloads_detail, onchain_param.auditor_group_zero_workload, auditor_group_workload_rewards, zero_workload_account);
    }
    if (validator_group_workload_rewards != 0) {
        community_reward += calc_invalid_workload_group_reward(false, property_param.map_nodes, validator_group_workload_rewards, property_param.validator_workloads_detail);
        community_reward += calc_zero_workload_reward(
            false, property_param.validator_workloads_detail, onchain_param.validator_group_zero_workload, validator_group_workload_rewards, zero_workload_account);
    }

    // TODO: voter to zero workload account has no workload reward
    for (auto const & entity : property_param.map_nodes) {
        auto const & account = entity.first;
        auto const & node = entity.second;

        top::xstake::uint128_t self_reward = 0;
        top::xstake::uint128_t dividend_reward = 0;

        // 3.2 workload reward
        if (node.is_edge_node()) {
            top::xstake::uint128_t reward_to_self = 0;
            calc_edge_workload_rewards(node, role_nums[edger_idx], edge_workload_rewards, reward_to_self);
            if (reward_to_self != 0) {
                issue_detail.m_node_rewards[account.to_string()].m_edge_reward = reward_to_self;
                self_reward += reward_to_self;
            }
        }
        if (node.is_archive_node()) {
            top::xstake::uint128_t reward_to_self = 0;
            calc_archive_workload_rewards(node, role_nums[archiver_idx], archive_workload_rewards, reward_to_self);
            if (reward_to_self != 0) {
                issue_detail.m_node_rewards[account.to_string()].m_archive_reward = reward_to_self;
                self_reward += reward_to_self;
            }
        }
        if (node.is_auditor_node()) {
            top::xstake::uint128_t reward_to_self = 0;
            calc_auditor_workload_rewards(node, role_nums[auditor_idx], property_param.auditor_workloads_detail, auditor_group_workload_rewards, reward_to_self);
            if (reward_to_self != 0) {
                issue_detail.m_node_rewards[account.to_string()].m_auditor_reward = reward_to_self;
                self_reward += reward_to_self;
            }
        }
        if (node.is_validator_node()) {
            top::xstake::uint128_t reward_to_self = 0;
            calc_validator_workload_rewards(node, role_nums[validator_idx], property_param.validator_workloads_detail, validator_group_workload_rewards, reward_to_self);
            if (reward_to_self != 0) {
                issue_detail.m_node_rewards[account.to_string()].m_validator_reward = reward_to_self;
                self_reward += reward_to_self;
            }
        }
        // 3.3 vote reward
        if (node.is_valid_auditor_node() && node.get_deposit() > 0 && auditor_total_votes > 0) {
            top::xstake::uint128_t reward_to_self = 0;
            calc_vote_reward(node, auditor_total_votes, vote_rewards, reward_to_self);
            if (reward_to_self != 0) {
                issue_detail.m_node_rewards[account.to_string()].m_vote_reward = reward_to_self;
                self_reward += reward_to_self;
            }
        }
        // 3.4 dividend reward = (workload reward + vote reward) * ratio
        if (node.m_support_ratio_numerator > 0 && account_votes[account] > 0) {
            dividend_reward = self_reward * node.m_support_ratio_numerator / node.m_support_ratio_denominator;
            self_reward -= dividend_reward;
        }
        issue_detail.m_node_rewards[account.to_string()].m_self_reward = self_reward;
        // 3.5 calc table reward
        if (self_reward > 0) {
            node_reward_detail[account] = self_reward;
        }
        if (dividend_reward > 0) {
            node_dividend_detail[account] = dividend_reward;
        }
    }
    XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "calc_nodes_rewards_Executed", 1);
}

void xtop_zec_reward_contract_new::calc_table_rewards(
    xreward_property_param_t & property_param,
    std::map<common::xaccount_address_t, top::xstake::uint128_t> const & node_reward_detail,
    std::map<common::xaccount_address_t, top::xstake::uint128_t> const & node_dividend_detail,
    std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, top::xstake::uint128_t>> & table_node_reward_detail,
    std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, top::xstake::uint128_t>> & table_node_dividend_detail,
    std::map<common::xaccount_address_t, top::xstake::uint128_t> & table_total_rewards) {
    std::map<common::xaccount_address_t, uint64_t> account_votes;
    calc_votes(property_param.votes_detail, property_param.map_nodes, account_votes);
    for (auto reward : node_reward_detail) {
        common::xaccount_address_t table_address = calc_table_contract_address(common::xaccount_address_t{reward.first});
        if (table_address.empty()) {
            continue;
        }
        calc_table_node_reward_detail(table_address, reward.first, reward.second, table_total_rewards, table_node_reward_detail);
    }
    for (auto reward : node_dividend_detail) {
        for (auto & vote_detail : property_param.votes_detail) {
            auto const & voter = vote_detail.first;
            auto const & votes = vote_detail.second;
            common::xaccount_address_t table_address = calc_table_contract_address(common::xaccount_address_t{voter});
            if (table_address.empty()) {
                continue;
            }
            calc_table_node_dividend_detail(table_address, reward.first, reward.second, account_votes[reward.first], votes, table_total_rewards, table_node_dividend_detail);
        }
    }
}

void xtop_zec_reward_contract_new::calc_rewards(const common::xlogic_time_t current_time,
                                                const common::xlogic_time_t activation_time,
                                                const xreward_onchain_param_t & onchain_param,
                                                xreward_property_param_t & property_param,
                                                xissue_detail & issue_detail,
                                                std::map<common::xaccount_address_t, top::xstake::uint128_t> & table_total_rewards,
                                                std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, top::xstake::uint128_t>> & table_node_reward_detail,
                                                std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, top::xstake::uint128_t>> & table_node_dividend_detail,
                                                top::xstake::uint128_t & community_reward) {
    std::map<common::xaccount_address_t, top::xstake::uint128_t> node_reward_detail;    // <node, self reward>
    std::map<common::xaccount_address_t, top::xstake::uint128_t> node_dividend_detail;  // <node, dividend reward>
    calc_nodes_rewards_v5(current_time, activation_time, onchain_param, property_param, issue_detail, node_reward_detail, node_dividend_detail, community_reward);
    calc_table_rewards(property_param, node_reward_detail, node_dividend_detail, table_node_reward_detail, table_node_dividend_detail, table_total_rewards);
}

void xtop_zec_reward_contract_new::dispatch_all_reward_v3(
    const common::xlogic_time_t current_time,
    std::map<common::xaccount_address_t, top::xstake::uint128_t> const & table_total_rewards,
    std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, top::xstake::uint128_t>> const & table_node_reward_detail,
    std::map<common::xaccount_address_t, std::map<common::xaccount_address_t, top::xstake::uint128_t>> const & table_node_dividend_detail,
    top::xstake::uint128_t const & community_reward,
    uint64_t & actual_issuance) {
    XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "dispatch_all_reward_Called", 1);
    XMETRICS_TIME_RECORD(XREWARD_CONTRACT "dispatch_all_reward");
    xdbg("[xtop_zec_reward_contract_new::dispatch_all_reward] pid:%d\n", getpid());
    // dispatch table reward
    uint64_t issuance = 0;
    // uint32_t task_id = get_task_id();
    for (auto const & entity : table_total_rewards) {
        auto const & contract = entity.first;
        auto const & total_award = entity.second;

        uint64_t reward = static_cast<uint64_t>(total_award / REWARD_PRECISION);
        if (total_award % REWARD_PRECISION != 0) {
            reward += 1;
        }
        issuance += reward;
        // std::map<std::string, uint64_t> issuances;
        std::error_code ec;
        transfer(contract, reward, contract_common::xfollowup_transaction_schedule_type_t::immediately, ec);
        XCONTRACT_ENSURE(!ec, "transfer generated error!");
        // issuances.emplace(contract.to_string(), reward);
        // xstream_t seo_stream(xcontext_t::instance());
        // seo_stream << issuances;

        // add_task(task_id, current_time, contract.to_string(), XTRANSFER_ACTION, std::string((char *)seo_stream.data(), seo_stream.size()));
        // task_id++;
    }
    xinfo("[xtop_zec_reward_contract_new::dispatch_all_reward] actual issuance: %lu", issuance);
    // dispatch community reward
    uint64_t common_funds = static_cast<uint64_t>(community_reward / REWARD_PRECISION);
    if (common_funds > 0) {
        issuance += common_funds;
        std::error_code ec;
        transfer(common::xaccount_address_t{sys_contract_rec_tcc_addr}, common_funds, contract_common::xfollowup_transaction_schedule_type_t::immediately, ec);
        XCONTRACT_ENSURE(!ec, "transfer generated error!");
        // std::map<std::string, uint64_t> issuances;
        // issuances.emplace(sys_contract_rec_tcc_addr, common_funds);
        // xstream_t seo_stream(xcontext_t::instance());
        // seo_stream << issuances;

        // add_task(task_id, current_time, sys_contract_rec_tcc_addr, XTRANSFER_ACTION, std::string((char *)seo_stream.data(), seo_stream.size()));
        // task_id++;
    }
    xinfo("[xtop_zec_reward_contract_new::dispatch_all_reward] common_funds: %lu", common_funds);
    // generate tasks
    const int task_limit = 1000;
    xinfo("[xtop_zec_reward_contract_new::dispatch_all_reward] pid: %d, table_node_reward_detail size: %d\n", getpid(), table_node_reward_detail.size());
    for (auto & entity : table_node_reward_detail) {
        auto const & contract = entity.first;
        auto const & account_awards = entity.second;

        int count = 0;
        std::map<std::string, top::xstake::uint128_t> account_awards2;
        for (auto it = account_awards.begin(); it != account_awards.end(); it++) {
            account_awards2[it->first.to_string()] = it->second;
            // if (++count % task_limit == 0) {
            //     xstream_t reward_stream(xcontext_t::instance());
            //     reward_stream << current_time;
            //     reward_stream << account_awards2;
                // add_task(task_id, current_time, contract.to_string(), XREWARD_CLAIMING_ADD_NODE_REWARD, std::string((char *)reward_stream.data(), reward_stream.size()));
            //     task_id++;

            //     account_awards2.clear();
            // }
        }
        xstream_t reward_stream(xcontext_t::instance());
        reward_stream << current_time;
        reward_stream << account_awards2;
        call(contract, "recv_node_reward", std::string((char *)reward_stream.data(), reward_stream.size()), contract_common::xfollowup_transaction_schedule_type_t::immediately);
        // if (account_awards2.size() > 0) {
        //     xstream_t reward_stream(xcontext_t::instance());
        //     reward_stream << current_time;
        //     reward_stream << account_awards2;
        //     add_task(task_id, current_time, contract.to_string(), XREWARD_CLAIMING_ADD_NODE_REWARD, std::string((char *)reward_stream.data(), reward_stream.size()));
        //     task_id++;
        // }
    }
    xinfo("[xtop_zec_reward_contract_new::dispatch_all_reward] pid: %d, table_node_dividend_detail size: %d\n", getpid(), table_node_dividend_detail.size());
    for (auto const & entity : table_node_dividend_detail) {
        auto const & contract = entity.first;
        auto const & auditor_vote_rewards = entity.second;

        int count = 0;
        std::map<std::string, top::xstake::uint128_t> auditor_vote_rewards2;
        for (auto it = auditor_vote_rewards.begin(); it != auditor_vote_rewards.end(); it++) {
            auditor_vote_rewards2[it->first.to_string()] = it->second;
            // if (++count % task_limit == 0) {
            //     xstream_t reward_stream(xcontext_t::instance());
            //     reward_stream << current_time;
            //     reward_stream << auditor_vote_rewards2;
            //     add_task(task_id, current_time, contract.to_string(), XREWARD_CLAIMING_ADD_VOTER_DIVIDEND_REWARD, std::string((char *)reward_stream.data(), reward_stream.size()));
            //     task_id++;

            //     auditor_vote_rewards2.clear();
            // }
        }
        // if (auditor_vote_rewards2.size() > 0) {
        //     xstream_t reward_stream(xcontext_t::instance());
        //     reward_stream << current_time;
        //     reward_stream << auditor_vote_rewards2;
            // add_task(task_id, current_time, contract.to_string(), XREWARD_CLAIMING_ADD_VOTER_DIVIDEND_REWARD, std::string((char *)reward_stream.data(), reward_stream.size()));
        //     task_id++;
        // }
        xstream_t reward_stream(xcontext_t::instance());
        reward_stream << current_time;
        reward_stream << auditor_vote_rewards2;
        call(contract, "recv_voter_dividend_reward", std::string((char *)reward_stream.data(), reward_stream.size()), contract_common::xfollowup_transaction_schedule_type_t::immediately);

    }

    actual_issuance = issuance;
// #if defined(DEBUG)
//     print_tasks();
// #endif

    XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "dispatch_all_reward_Executed", 1);
    return;
}

void xtop_zec_reward_contract_new::update_property(const common::xlogic_time_t current_time,
                                                   const uint64_t actual_issuance,
                                                   const xactivation_record & activation_record,
                                                   const xaccumulated_reward_record & record,
                                                   xissue_detail const & issue_detail) {
    update_accumulated_issuance(current_time, actual_issuance, activation_record);
    update_accumulated_record(record);
    update_issuance_detail(issue_detail);
}

std::string xtop_zec_reward_contract_new::reward_task_serialize(const xreward_dispatch_task & task) const {
    xstream_t stream{xcontext_t::instance()};
    task.serialize_to(stream);
    return std::string((char *)stream.data(), stream.size());
}

xreward_dispatch_task xtop_zec_reward_contract_new::reward_task_deserialize(const std::string & task_str) const {
    xreward_dispatch_task task;
    if (!task_str.empty()) {
        xstream_t stream{xcontext_t::instance(), (uint8_t *)task_str.c_str(), (uint32_t)task_str.size()};
        task.serialize_from(stream);
    }
    return task;
}

std::string xtop_zec_reward_contract_new::accumulated_reward_serialize(const xaccumulated_reward_record & record) const {
    xstream_t stream{xcontext_t::instance()};
    record.serialize_to(stream);
    return std::string((char *)stream.data(), stream.size());
}

xaccumulated_reward_record xtop_zec_reward_contract_new::accumulated_reward_deserialize(const std::string & record_str) const {
    xaccumulated_reward_record record;
    if (!record_str.empty()) {
        xstream_t stream{xcontext_t::instance(), (uint8_t *)record_str.c_str(), (uint32_t)record_str.size()};
        record.serialize_from(stream);
    }
    return record;
}

xactivation_record xtop_zec_reward_contract_new::get_activation_record() {
    xactivation_record activation_record;
    // std::string activation_str = STRING_GET2(XPORPERTY_CONTRACT_GENESIS_STAGE_KEY, sys_contract_rec_registration_addr);
    contract_common::properties::xstring_property_t active_prop{XPORPERTY_CONTRACT_GENESIS_STAGE_KEY, this};
    auto activation_str = active_prop.value();

    XCONTRACT_ENSURE(!activation_str.empty(), "activation_str");
    xstream_t stream{xcontext_t::instance(), (uint8_t *)activation_str.c_str(), (uint32_t)activation_str.size()};
    activation_record.serialize_from(stream);
    return activation_record;
}

NS_END2

#undef XREWARD_CONTRACT
#undef XCONTRACT_PREFIX
#undef XZEC_MODULE

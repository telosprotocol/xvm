// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xreward/xzec_reward_contract.h"

#include "xbase/xutl.h"
#include "xbasic/xutility.h"
#include "xchain_upgrade/xchain_upgrade_center.h"
#include "xdata/xgenesis_data.h"
#include "xstake/xstake_algorithm.h"
#include "xdata/xworkload_info.h"
#include "xstore/xstore_error.h"

#include <iomanip>

using top::base::xcontext_t;
using top::base::xstream_t;
using top::base::xstring_utl;
using namespace top::data;

#if !defined(XZEC_MODULE)
#    define XZEC_MODULE "sysContract_"
#endif

#define XCONTRACT_PREFIX "reward_"

#define XREWARD_CONTRACT XZEC_MODULE XCONTRACT_PREFIX

NS_BEG2(top, xstake)

xzec_reward_contract::xzec_reward_contract(common::xnetwork_id_t const & network_id) : xbase_t{network_id} {}

void xzec_reward_contract::setup() {
    MAP_CREATE(XPORPERTY_CONTRACT_TASK_KEY);     // save dispatch tasks
    MAP_CREATE(XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE); // save issuance
    MAP_SET(XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE, "total", "0"); //set total accumulated issuance

    STRING_CREATE(XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE_YEARLY);
    xaccumulated_reward_record record;
    update_accumulated_record(record);

    STRING_CREATE(XPROPERTY_LAST_READ_REC_REG_CONTRACT_BLOCK_HEIGHT);
    std::string last_read_rec_reg_contract_height{"0"};
    STRING_SET(XPROPERTY_LAST_READ_REC_REG_CONTRACT_BLOCK_HEIGHT, last_read_rec_reg_contract_height);
    STRING_CREATE(XPROPERTY_LAST_READ_REC_REG_CONTRACT_LOGIC_TIME);
    std::string last_read_rec_reg_contract_logic_time{"0"};
    STRING_SET(XPROPERTY_LAST_READ_REC_REG_CONTRACT_LOGIC_TIME, last_read_rec_reg_contract_logic_time);
}

void xzec_reward_contract::on_timer(const uint64_t onchain_timer_round) {
    XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "on_timer_Called", 1);
    XMETRICS_TIME_RECORD(XREWARD_CONTRACT "on_timer_ExecutionTime");

    std::string source_address = SOURCE_ADDRESS();
    if (SELF_ADDRESS().value() != source_address) {
        xwarn("[xzec_reward_contract::on_timer] invalid call from %s", source_address.c_str());
        return;
    }

    if (MAP_SIZE(XPORPERTY_CONTRACT_TASK_KEY) > 0) {
        execute_task();
    } else {
        chain_upgrade::xtop_chain_fork_config_center fork_config_center;
        auto fork_config = fork_config_center.chain_fork_config();
        if (chain_upgrade::xtop_chain_fork_config_center::is_forked(fork_config.reward_fork_spark, onchain_timer_round)) {
            if (reward_is_expire_v2(onchain_timer_round)) {
                reward(onchain_timer_round, "");
            } else {
                update_reg_contract_read_status(onchain_timer_round);
            }
        } else if (chain_upgrade::xtop_chain_fork_config_center::is_forked(fork_config.reward_fork_point, onchain_timer_round)) {
            update_reg_contract_read_status(onchain_timer_round);
        }
    }

    XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "on_timer_Executed", 1);
}

void xzec_reward_contract::update_reg_contract_read_status(const uint64_t cur_time) {
    bool update_rec_reg_contract_read_status{false};

    auto const last_read_height = static_cast<std::uint64_t>(std::stoull(STRING_GET(XPROPERTY_LAST_READ_REC_REG_CONTRACT_BLOCK_HEIGHT)));
    auto const last_read_time = static_cast<std::uint64_t>(std::stoull(STRING_GET(XPROPERTY_LAST_READ_REC_REG_CONTRACT_LOGIC_TIME)));

    auto const height_step_limitation = XGET_ONCHAIN_GOVERNANCE_PARAMETER(cross_reading_rec_reg_contract_height_step_limitation);
    auto const timeout_limitation = XGET_ONCHAIN_GOVERNANCE_PARAMETER(cross_reading_rec_reg_contract_logic_timeout_limitation);

    uint64_t latest_height = get_blockchain_height(sys_contract_rec_registration_addr);
    xdbg("[xzec_reward_contract::update_reg_contract_read_status] cur_time: %llu, last_read_time: %llu, last_read_height: %llu, latest_height: %" PRIu64,
        cur_time,
        last_read_time,
        last_read_height,
        latest_height);
    XCONTRACT_ENSURE(latest_height >= last_read_height, u8"xzec_reward_contract::update_reg_contract_read_status latest_height < last_read_height");
    if (latest_height == last_read_height) {
        XMETRICS_PACKET_INFO(XREWARD_CONTRACT "update_status", "next_read_height", last_read_height, "current_time", cur_time)
        STRING_SET(XPROPERTY_LAST_READ_REC_REG_CONTRACT_LOGIC_TIME, std::to_string(cur_time));
        return;
    }
    // calc current_read_height:
    uint64_t next_read_height = last_read_height;
    if (latest_height - last_read_height >= height_step_limitation) {
        next_read_height = last_read_height + height_step_limitation;
        update_rec_reg_contract_read_status = true;
    } else if (cur_time - last_read_time > timeout_limitation) {
        next_read_height = latest_height;
        update_rec_reg_contract_read_status = true;
    }
    xinfo("[xzec_reward_contract::update_reg_contract_read_status] next_read_height: %" PRIu64 ", latest_height: %llu, update_rec_reg_contract_read_status: %d",
        next_read_height, latest_height, update_rec_reg_contract_read_status);

    base::xauto_ptr<xblock_t> block_ptr = get_block_by_height(sys_contract_rec_registration_addr, next_read_height);
    std::string result;
    if (block_ptr == nullptr) {
        xwarn("[xzec_reward_contract::update_reg_contract_read_status] fail to get the rec_reg data. next_read_block height: %" PRIu64 ", latest_height: %llu",
            next_read_height,
            latest_height);
        return;
    }

    if (update_rec_reg_contract_read_status) {
        XMETRICS_PACKET_INFO(XREWARD_CONTRACT "update_status", "next_read_height", next_read_height, "current_time", cur_time)
        STRING_SET(XPROPERTY_LAST_READ_REC_REG_CONTRACT_BLOCK_HEIGHT, std::to_string(next_read_height));
        STRING_SET(XPROPERTY_LAST_READ_REC_REG_CONTRACT_LOGIC_TIME, std::to_string(cur_time));
    }
    return;
}

void xzec_reward_contract::calculate_reward(common::xlogic_time_t timer_round, std::string const& workload_str) {
    std::string source_address = SOURCE_ADDRESS();
    xinfo("[xzec_reward_contract::calculate_reward] called from address: %s", source_address.c_str());
    if (sys_contract_zec_workload_addr != source_address) {
        xwarn("[xzec_reward_contract::calculate_reward] from invalid address: %s\n", source_address.c_str());
        return;
    }

    uint64_t onchain_timer_round = TIME();

    auto const & fork_config = chain_upgrade::xchain_fork_config_center_t::chain_fork_config();
    if (chain_upgrade::xchain_fork_config_center_t::is_forked(fork_config.reward_fork_spark, onchain_timer_round)) {
        on_receive_workload(workload_str);
    } else {
        auto height_change = reward_is_expire(onchain_timer_round);
        if (height_change) {
            reward(onchain_timer_round, workload_str);
        }
    }
}

void xzec_reward_contract::reward(const uint64_t onchain_timer_round, std::string const& workload_str) {
    xdbg("[xzec_reward_contract::reward] pid:%d\n", getpid());

    std::map<std::string, std::map<std::string, top::xstake::uint128_t >> table_nodes_rewards;
    std::map<std::string, std::map<std::string, top::xstake::uint128_t >> table_vote_rewards;
    std::map<std::string, top::xstake::uint128_t> contract_rewards;

    chain_upgrade::xtop_chain_fork_config_center fork_config_center;
    auto fork_config = fork_config_center.chain_fork_config();
    if (chain_upgrade::xtop_chain_fork_config_center::is_forked(fork_config.reward_fork_detail, onchain_timer_round)) {
        calc_nodes_rewards_v4(table_nodes_rewards, contract_rewards, table_vote_rewards, onchain_timer_round);
    } else if (chain_upgrade::xtop_chain_fork_config_center::is_forked(fork_config.reward_fork_spark, onchain_timer_round)) {
        calc_nodes_rewards_v3(table_nodes_rewards, contract_rewards, table_vote_rewards, onchain_timer_round);
    } else if (chain_upgrade::xtop_chain_fork_config_center::is_forked(fork_config.reward_fork_point, onchain_timer_round)) {
        calc_nodes_rewards_v2(table_nodes_rewards, contract_rewards, table_vote_rewards, onchain_timer_round, workload_str);
    } else {
        calc_nodes_rewards(table_nodes_rewards, contract_rewards, table_vote_rewards, onchain_timer_round, workload_str);
    }

    if (chain_upgrade::xtop_chain_fork_config_center::is_forked(fork_config.reward_fork_spark, onchain_timer_round)) {
        dispatch_all_reward_v2(table_nodes_rewards, contract_rewards, table_vote_rewards, onchain_timer_round);
    } else {
        dispatch_all_reward(table_nodes_rewards, contract_rewards, table_vote_rewards, onchain_timer_round);
    }
}

void xzec_reward_contract::calc_nodes_rewards(std::map<std::string, std::map<std::string, top::xstake::uint128_t >> & table_nodes_rewards,
                                              std::map<std::string, top::xstake::uint128_t> & contract_rewards,
                                              std::map<std::string, std::map<std::string, top::xstake::uint128_t >> & contract_auditor_vote_rewards,
                                              const uint64_t onchain_timer_round, std::string const& workload_str) {
    XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "calc_nodes_rewards_Called", 1);
    XMETRICS_TIME_RECORD(XREWARD_CONTRACT "calc_nodes_rewards_ExecutionTime");

    auto add_table_node_reward = [&](common::xaccount_address_t const & account, top::xstake::uint128_t node_reward) {
        if (node_reward == 0)
            return;

        uint32_t table_id = 0;
        if (!EXTRACT_TABLE_ID(account, table_id)) {
            xdbg("[xzec_reward_contract::calc_nodes_rewards][xzec_reward_contract::add_table_node_reward] node reward pid: %d, account: %s, node_reward: [%llu, %u]\n",
                getpid(), account.c_str(), static_cast<uint64_t>(node_reward / REWARD_PRECISION), static_cast<uint32_t>(node_reward % REWARD_PRECISION));
            return;
        }

        auto const & reward_contract = CALC_CONTRACT_ADDRESS(sys_contract_sharding_reward_claiming_addr, table_id);
        xdbg("[xzec_reward_contract::calc_nodes_rewards][xzec_reward_contract::add_table_node_reward] node reward, pid:%d, reward_contract: %s, account: %s, reward: [%llu, %u]\n",
             getpid(),
             reward_contract.c_str(),
             account.c_str(),
             static_cast<uint64_t>(node_reward / REWARD_PRECISION), static_cast<uint32_t>(node_reward % REWARD_PRECISION));

        contract_rewards[reward_contract] += node_reward;
        table_nodes_rewards[reward_contract][account.value()] = node_reward;
    };

    auto get_adv_total_votes = [&](std::map<std::string, std::map<std::string, std::string>> const & contract_auditor_votes, common::xaccount_address_t const & account) {
        uint64_t adv_total_votes = 0;

        for (auto const & contract_auditor_vote : contract_auditor_votes) {
            auto const & contract = contract_auditor_vote.first;
            auto const & auditor_votes = contract_auditor_vote.second;

            auto iter = auditor_votes.find(account.value());
            if (iter != auditor_votes.end()) {
                adv_total_votes += base::xstring_utl::touint64(iter->second);
            }
        }

        return adv_total_votes;
    };

    auto add_table_vote_reward = [&](common::xaccount_address_t const & account,
                                   uint64_t adv_total_votes,
                                   top::xstake::uint128_t const & adv_reward_to_voters,
                                   std::map<std::string, std::map<std::string, std::string>> const & contract_auditor_votes) {
        if (adv_total_votes == 0)
            return;

        for (auto & contract_auditor_vote : contract_auditor_votes) {
            auto const & contract = contract_auditor_vote.first;
            auto const & auditor_votes = contract_auditor_vote.second;

            uint32_t table_id = 0;
            if (!xdatautil::extract_table_id_from_address(contract, table_id)) {
                xwarn("[xzec_reward_contract::calc_nodes_rewards][xzec_reward_contract::add_table_vote_reward] extract_table_id_from_address %s  failed!\n", contract.c_str());
                continue;
            }
            auto const & reward_contract = CALC_CONTRACT_ADDRESS(sys_contract_sharding_reward_claiming_addr, table_id);
            auto iter = auditor_votes.find(account.value());
            if (iter != auditor_votes.end()) {
                auto adv_reward_to_contract = adv_reward_to_voters * base::xstring_utl::touint64(iter->second) / adv_total_votes;
                xdbg("[xzec_reward_contract::calc_nodes_rewards][add_table_vote_reward] account: %s, contract: %s, table votes: %llu, adv_total_votes: %llu, adv_reward_to_voters: [%llu, %u], adv_reward_to_contract: [%llu, %u]\n",
                    account.c_str(),
                    contract.c_str(),
                    base::xstring_utl::touint64(iter->second),
                    adv_total_votes,
                    static_cast<uint64_t>(adv_reward_to_voters / REWARD_PRECISION),
                    static_cast<uint32_t>(adv_reward_to_voters % REWARD_PRECISION),
                    static_cast<uint64_t>(adv_reward_to_contract / REWARD_PRECISION),
                    static_cast<uint32_t>(adv_reward_to_contract % REWARD_PRECISION));
                if (adv_reward_to_contract > 0) {
                    contract_rewards[reward_contract] += adv_reward_to_contract;
                    contract_auditor_vote_rewards[reward_contract][account.value()] += adv_reward_to_contract;
                }
            }
        }
    };

    auto add_workload_reward = [&](bool is_auditor, bool is_seed, common::xaccount_address_t const & account, top::xstake::uint128_t const & cluster_total_rewards, std::map<std::string, std::string> const & clusters_workloads, top::xstake::uint128_t & seed_node_rewards, top::xstake::uint128_t & node_reward) {
        uint32_t zero_workload_val = 0;
        if (is_auditor) {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(cluster_zero_workload);
        } else {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(shard_zero_workload);
        }
        xdbg("[xzec_reward_contract::calc_nodes_rewards][add_workload_reward] account: %s, is_auditor: %d, is_seed: %d, %d clusters report workloads, cluster_total_rewards: [%llu, %u], zero_workload_val: %u\n",
             account.c_str(),
             is_auditor,
             is_seed,
             clusters_workloads.size(),
             static_cast<uint64_t>(cluster_total_rewards / REWARD_PRECISION),
             static_cast<uint32_t>(cluster_total_rewards % REWARD_PRECISION),
             zero_workload_val);

        for (auto & cluster_workloads : clusters_workloads) {
            auto const & key_str = cluster_workloads.first;
            common::xcluster_address_t cluster;
            xstream_t key_stream(xcontext_t::instance(), (uint8_t *)key_str.data(), key_str.size());
            key_stream >> cluster;
            auto const & value_str = cluster_workloads.second;
            xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
            cluster_workload_t workload;
            workload.serialize_from(stream);
            xdbg("[xzec_reward_contract::calc_nodes_rewards][add_workload_reward] account: %s, auditor cluster id: %s, cluster size: %d, cluster_total_workload: %u\n",
                 account.c_str(),
                 cluster.to_string().c_str(),
                 workload.m_leader_count.size(),
                 workload.cluster_total_workload);
            if (workload.cluster_total_workload <= zero_workload_val)
                continue;
            auto it = workload.m_leader_count.find(account.value());
            if (it != workload.m_leader_count.end()) {
                auto const & work = it->second;
                auto workload_reward = cluster_total_rewards * work / workload.cluster_total_workload;
                if (!is_seed) {
                    auto reward_type = is_auditor ? xreward_type::auditor_reward : xreward_type::validator_reward;
                    //add_node_reward(account, reward_type, workload_reward);
                    node_reward += workload_reward;
                } else {
                    seed_node_rewards += workload_reward;
                }

                xdbg("[xzec_reward_contract::calc_nodes_rewards][add_workload_reward] account: %s, cluster_id: %s, work: %d, total_workload: %d, cluster_total_rewards: [%llu, %u], reward: [%llu, %u], is_seed: %u\n",
                     account.c_str(),
                     cluster.to_string().c_str(),
                     work,
                     workload.cluster_total_workload,
                     static_cast<uint64_t>(cluster_total_rewards / xstake::REWARD_PRECISION),
                     static_cast<uint32_t>(cluster_total_rewards % xstake::REWARD_PRECISION),
                     static_cast<uint64_t>(workload_reward / xstake::REWARD_PRECISION),
                     static_cast<uint32_t>(workload_reward % xstake::REWARD_PRECISION),
                     is_seed);
            }
        }
    };

    auto zero_workload_reward = [&](bool validator, top::xstake::uint128_t const & workload_total_reward, const std::map<std::string, std::string> & clusters_workloads, top::xstake::uint128_t & zero_workload_rewards) {
        std::size_t cluster_size;
        uint8_t group_id_begin;
        bool zero_workload = false;
        uint32_t zero_workload_val = 0;
        if (validator) {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(shard_zero_workload);
            cluster_size = XGET_ONCHAIN_GOVERNANCE_PARAMETER(validator_group_count);
            group_id_begin = common::xvalidator_group_id_begin.value();
        } else {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(cluster_zero_workload);
            cluster_size = XGET_ONCHAIN_GOVERNANCE_PARAMETER(auditor_group_count);
            group_id_begin = common::xauditor_group_id_begin.value();
        }
        if (cluster_size == 0) {
            xwarn("[xzec_reward_contract::calc_nodes_rewards][zero_workload_reward] validator_workload: %d, cluster_size zero", validator);
            return;
        }
        top::xstake::uint128_t cluster_total_rewards = workload_total_reward / cluster_size;  // averaged by all clusters
        for (auto group_id = group_id_begin; group_id < group_id_begin + cluster_size; group_id++) {
            zero_workload = true;
            for (auto & cluster_workloads : clusters_workloads) {
                auto const & key_str = cluster_workloads.first;
                xstream_t stream(xcontext_t::instance(), (uint8_t *)key_str.data(), key_str.size());
                common::xcluster_address_t cluster;
                stream >> cluster;
                if (group_id == cluster.group_id().value()) {
                    auto const & value_str = cluster_workloads.second;
                    xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
                    cluster_workload_t workload;
                    workload.serialize_from(stream);
                    if (workload.cluster_total_workload > zero_workload_val) {
                        zero_workload = false;
                    }
                    xdbg("[xzec_reward_contract::calc_nodes_rewards][zero_workload_reward] group %u has workload %u, zero_workload: %d", group_id, workload.cluster_total_workload, zero_workload);
                    break;
                }
            }
            if (zero_workload) {
                zero_workload_rewards += cluster_total_rewards;
            }
            xdbg("[xzec_reward_contract::calc_nodes_rewards][zero_workload_reward] validator: %d, cluster_size: %u, group %u, zero_workload: %d, cluster_total_rewards: [%llu, %u], zero_workload_rewards: [%llu, %u]",
                 validator,
                 cluster_size,
                 group_id,
                 zero_workload,
                 static_cast<uint64_t>(cluster_total_rewards / xstake::REWARD_PRECISION),
                 static_cast<uint32_t>(cluster_total_rewards % xstake::REWARD_PRECISION),
                 static_cast<uint64_t>(zero_workload_rewards / xstake::REWARD_PRECISION),
                 static_cast<uint32_t>(zero_workload_rewards % xstake::REWARD_PRECISION));
        }
    };

    std::map<std::string, std::string> auditor_clusters_workloads;
    std::map<std::string, std::string> validator_clusters_workloads;
    bool auditor_workload, validator_workload;
    base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)workload_str.data(), workload_str.size());
    stream >> auditor_workload;
    if (!auditor_workload)  MAP_DESERIALIZE_SIMPLE(stream, auditor_clusters_workloads);
    stream >> validator_workload;
    if (!validator_workload)  MAP_DESERIALIZE_SIMPLE(stream, validator_clusters_workloads);

    // contract auditor votes
    std::map<std::string, std::string> contract_auditor_votes2;
    MAP_COPY_GET(XPORPERTY_CONTRACT_TICKETS_KEY, contract_auditor_votes2, sys_contract_zec_vote_addr);

    xdbg("[xzec_reward_contract::calc_vote_rewards] contract_auditor_votes2 size: %d", contract_auditor_votes2.size());
    // transform to map of map struct
    std::map<std::string, std::map<std::string, std::string>> contract_auditor_votes;
    for (auto & contract_auditor_vote : contract_auditor_votes2) {
        auto const & contract = contract_auditor_vote.first;
        auto const & auditor_votes_str = contract_auditor_vote.second;

        std::map<std::string, std::string> auditor_votes;
        xstream_t stream(xcontext_t::instance(), (uint8_t *)auditor_votes_str.data(), auditor_votes_str.size());
        stream >> auditor_votes;
        contract_auditor_votes[contract] = auditor_votes;
    }

    // archive rewards / edge rewards
    std::map<std::string, std::string> map_nodes;
    try {
        MAP_COPY_GET(XPORPERTY_CONTRACT_REG_KEY, map_nodes, sys_contract_rec_registration_addr);
    } catch (std::runtime_error & e) {
        xdbg("[xzec_reward_contract::calc_nodes_rewards] MAP COPY GET error:%s", e.what());
    }

    uint64_t cur_time = onchain_timer_round;
    uint64_t activation_time = get_activated_time();
    int64_t total_height = cur_time - activation_time;
    xreg_node_info node;
    uint32_t edge_num = 0;
    uint32_t archive_num = 0;
    uint32_t total_auditor_nodes = 0;
    auto issuance = calc_issuance(total_height);
    auto auditor_total_rewards      = get_reward(issuance, xreward_type::auditor_reward);
    auto validator_total_rewards    = get_reward(issuance, xreward_type::validator_reward);
    auto edge_total_rewards         = get_reward(issuance, xreward_type::edge_reward);
    auto archive_total_rewards      = get_reward(issuance, xreward_type::archive_reward);
    auto total_vote_rewards         = get_reward(issuance, xreward_type::vote_reward);
    auto governance_rewards         = get_reward(issuance, xreward_type::governance_reward);
    std::size_t auditor_group_count = XGET_ONCHAIN_GOVERNANCE_PARAMETER(auditor_group_count);
    XCONTRACT_ENSURE(auditor_group_count > 0, "auditor group count equals zero");
    top::xstake::uint128_t auditor_group_rewards = auditor_total_rewards / auditor_group_count;
    std::size_t validator_group_count = XGET_ONCHAIN_GOVERNANCE_PARAMETER(validator_group_count);
    XCONTRACT_ENSURE(validator_group_count > 0, "validator group count equals zero");
    top::xstake::uint128_t validator_group_rewards = validator_total_rewards / validator_group_count;
    top::xstake::uint128_t zero_workload_rewards = 0;
    top::xstake::uint128_t seed_node_rewards = 0;

    for (auto const & entity : map_nodes) {
        auto const & account = entity.first;
        auto const & value_str = entity.second;
        xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
        node.serialize_from(stream);
        if (node.get_deposit() > 0 && node.is_edge_node()) {
            edge_num++;
        }
        if (node.get_deposit() > 0 && node.is_archive_node()) {
            archive_num++;
        }
        if (node.is_auditor_node()) {
            total_auditor_nodes++;
        }
    }
    // count all votes
    uint64_t all_tickets = 0;
    for (auto const & entity : contract_auditor_votes) {
        auto const & auditor_votes = entity.second;

        for (auto const & entity2 : auditor_votes) {
            all_tickets += base::xstring_utl::touint64(entity2.second);
        }
    }

    xinfo(
        "[xzec_reward_contract::calc_nodes_rewards] cur_time: %llu, activation_time: %llu, "
        "issuance: [%llu, %u], "
        "edge total rewards: [%llu, %u], edge num: %d, "
        "archive_total_rewards: [%llu, %u], archive num: %d, "
        "auditor_total_rewards: [%llu, %u], validator_total_rewards: [%llu, %u], "
        "total_vote_rewards: [%llu, %u], governance_rewards: [%llu, %u], all_tickets: %llu, total_auditor_nodes: %u\n",
        cur_time,
        activation_time,
        static_cast<uint64_t>(issuance / REWARD_PRECISION),
        static_cast<uint32_t>(issuance % REWARD_PRECISION),
        static_cast<uint64_t>(edge_total_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(edge_total_rewards % REWARD_PRECISION),
        edge_num,
        static_cast<uint64_t>(archive_total_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(archive_total_rewards % REWARD_PRECISION),
        archive_num,
        static_cast<uint64_t>(auditor_total_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(auditor_total_rewards % REWARD_PRECISION),
        static_cast<uint64_t>(validator_total_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(validator_total_rewards % REWARD_PRECISION),
        static_cast<uint64_t>(total_vote_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(total_vote_rewards % REWARD_PRECISION),
        static_cast<uint64_t>(governance_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(governance_rewards % REWARD_PRECISION),
        all_tickets,
        total_auditor_nodes);
    for (auto const & entity : map_nodes) {
        auto const & account = entity.first;
        auto const & value_str = entity.second;
        top::xstake::uint128_t node_reward = 0;
        xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
        node.serialize_from(stream);
        if (edge_num > 0 && node.is_edge_node()) {
            if (node.get_deposit() > 0) {
                //add_node_reward(account, xreward_type::edge_reward, edge_total_rewards / edge_num);
                auto edge_reward = edge_total_rewards / edge_num;
                xdbg("[xzec_reward_contract::calc_nodes_rewards] account: %s, edge reward: [%llu, %u]",
                    account.c_str(),
                    static_cast<uint64_t>(edge_reward / xstake::REWARD_PRECISION),
                    static_cast<uint32_t>(edge_reward % xstake::REWARD_PRECISION));
                node_reward += edge_reward;
            }
        }
        if (archive_num > 0 && node.is_archive_node()) {
            if (node.get_deposit() > 0) {
                //add_node_reward(account, xreward_type::archive_reward, archive_total_rewards / archive_num);
                auto archive_reward = archive_total_rewards / archive_num;
                xdbg("[xzec_reward_contract::calc_nodes_rewards] account: %s, archive reward: [%llu, %u]",
                    account.c_str(),
                    static_cast<uint64_t>(archive_reward / xstake::REWARD_PRECISION),
                    static_cast<uint32_t>(archive_reward % xstake::REWARD_PRECISION));
                node_reward += archive_reward;
            }
        }
        if (node.is_validator_node()) {
            add_workload_reward(false, node.get_deposit() == 0, node.m_account, validator_group_rewards, validator_clusters_workloads, seed_node_rewards, node_reward);
        }
        if (node.is_auditor_node()) {
            add_workload_reward(true, node.get_deposit() == 0, node.m_account, auditor_group_rewards, auditor_clusters_workloads, seed_node_rewards, node_reward);
            auto adv_total_votes = get_adv_total_votes(contract_auditor_votes, node.m_account);
            // vote reward
            if (all_tickets > 0) {
                auto node_vote_reward = adv_total_votes * total_vote_rewards / all_tickets;
                xdbg("[xzec_reward_contract::calc_nodes_rewards] account: %s, node_vote_reward: [%llu, %u], node deposit: %llu, all_tickets: %llu, adv_total_votes: %llu",
                        account.c_str(),
                        static_cast<uint64_t>(node_vote_reward / REWARD_PRECISION),
                        static_cast<uint32_t>(node_vote_reward % REWARD_PRECISION),
                        node.get_deposit(),
                        all_tickets,
                        adv_total_votes);
                if (node.get_deposit() > 0) {
                    node_reward += node_vote_reward;
                    if (adv_total_votes > 0 && node.m_support_ratio_numerator > 0) {
                        auto adv_reward_to_self = node_reward * (node.m_support_ratio_denominator - node.m_support_ratio_numerator) / node.m_support_ratio_denominator;
                        auto adv_reward_to_voters = node_reward - adv_reward_to_self;
                        add_table_vote_reward(node.m_account, adv_total_votes, adv_reward_to_voters, contract_auditor_votes);
                        node_reward = adv_reward_to_self;
                    }
                } else {
                    seed_node_rewards += node_vote_reward;
                    xdbg("[xzec_reward_contract::calc_nodes_rewards] account: %s, node_vote_reward: [%llu, %u], seed_node_rewards: [%llu, %u]",
                        account.c_str(),
                        static_cast<uint64_t>(node_vote_reward / REWARD_PRECISION),
                        static_cast<uint32_t>(node_vote_reward % REWARD_PRECISION),
                        static_cast<uint64_t>(seed_node_rewards / REWARD_PRECISION),
                        static_cast<uint32_t>(seed_node_rewards % REWARD_PRECISION));
                }
            } else if (total_auditor_nodes > 0) {
                auto node_vote_reward = total_vote_rewards / total_auditor_nodes;
                if (node.get_deposit() > 0) {
                    node_reward += node_vote_reward;
                    xdbg("[xzec_reward_contract::calc_nodes_rewards] account: %s, node_vote_reward: [%llu, %u], node deposit: %llu",
                        account.c_str(),
                        static_cast<uint64_t>(node_vote_reward / REWARD_PRECISION),
                        static_cast<uint32_t>(node_vote_reward % REWARD_PRECISION),
                        node.get_deposit());
                } else {
                    seed_node_rewards += node_vote_reward;
                    xdbg("[xzec_reward_contract::calc_nodes_rewards] account: %s, node_vote_reward: [%llu, %u], seed_node_rewards: [%llu, %u]",
                        account.c_str(),
                        static_cast<uint64_t>(node_vote_reward / REWARD_PRECISION),
                        static_cast<uint32_t>(node_vote_reward % REWARD_PRECISION),
                        static_cast<uint64_t>(seed_node_rewards / REWARD_PRECISION),
                        static_cast<uint32_t>(seed_node_rewards % REWARD_PRECISION));
                }
            }
        }
        add_table_node_reward(node.m_account, node_reward);
    }
    if (edge_num == 0) seed_node_rewards += edge_total_rewards;
    if (archive_num == 0) seed_node_rewards += archive_total_rewards;
    zero_workload_reward(true, validator_total_rewards, validator_clusters_workloads, zero_workload_rewards);
    zero_workload_reward(false, auditor_total_rewards, auditor_clusters_workloads, zero_workload_rewards);

    // clear accumulated workloads
    // CLEAR(enum_type_t::map, XPORPERTY_CONTRACT_WORKLOAD_KEY);
    // CLEAR(enum_type_t::map, XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY);
    // CALL(common::xaccount_address_t{sys_contract_zec_workload_addr}, "clear_workload", std::string(""));
    // uint32_t task_id = get_task_id();
    // add_task(task_id, onchain_timer_round, sys_contract_zec_workload_addr, XZEC_WORKLOAD_CLEAR_WORKLOAD_ACTION, std::string(""));
    // task_id++;

    // governance rewards
    // request additional issuance
    uint64_t common_funds = static_cast<uint64_t>( (governance_rewards + zero_workload_rewards + seed_node_rewards) / REWARD_PRECISION );
    if ( common_funds > 0 ) {
        uint32_t task_id = get_task_id();
        std::map<std::string, uint64_t> issuances;
        issuances.emplace(sys_contract_rec_tcc_addr, common_funds);
        base::xstream_t seo_stream(base::xcontext_t::instance());
        seo_stream << issuances;
        add_task(task_id, onchain_timer_round, "", XTRANSFER_ACTION, std::string((char *)seo_stream.data(), seo_stream.size()));
        task_id++;
        update_accumulated_issuance(common_funds, onchain_timer_round);
        XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "calc_nodes_rewards_Executed", 1);
    }
}

void xzec_reward_contract::calc_nodes_rewards_v2(std::map<std::string, std::map<std::string, top::xstake::uint128_t >> & table_nodes_rewards,
                                              std::map<std::string, top::xstake::uint128_t> & contract_rewards,
                                              std::map<std::string, std::map<std::string, top::xstake::uint128_t >> & contract_auditor_vote_rewards,
                                              const uint64_t onchain_timer_round, std::string const& workload_str) {
    XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "calc_nodes_rewards_Called", 1);
    XMETRICS_TIME_RECORD(XREWARD_CONTRACT "calc_nodes_rewards_ExecutionTime");

    auto add_table_node_reward = [&](common::xaccount_address_t const & account, top::xstake::uint128_t node_reward) {
        if (node_reward == 0)
            return;

        uint32_t table_id = 0;
        if (!EXTRACT_TABLE_ID(account, table_id)) {
            xwarn("[xzec_reward_contract::calc_nodes_rewards_v2][xzec_reward_contract::add_table_node_reward] node reward pid: %d, account: %s, node_reward: [%llu, %u]\n",
                getpid(), account.c_str(), static_cast<uint64_t>(node_reward / REWARD_PRECISION), static_cast<uint32_t>(node_reward % REWARD_PRECISION));
            return;
        }

        auto const & reward_contract = CALC_CONTRACT_ADDRESS(sys_contract_sharding_reward_claiming_addr, table_id);
        xdbg("[xzec_reward_contract::calc_nodes_rewards_v2][xzec_reward_contract::add_table_node_reward] node reward, pid:%d, reward_contract: %s, account: %s, reward: [%llu, %u]\n",
             getpid(),
             reward_contract.c_str(),
             account.c_str(),
             static_cast<uint64_t>(node_reward / REWARD_PRECISION), static_cast<uint32_t>(node_reward % REWARD_PRECISION));

        contract_rewards[reward_contract] += node_reward;
        table_nodes_rewards[reward_contract][account.value()] = node_reward;
    };

    auto get_adv_total_votes = [&](std::map<std::string, std::map<std::string, std::string>> const & contract_auditor_votes, common::xaccount_address_t const & account) {
        uint64_t adv_total_votes = 0;

        for (auto const & contract_auditor_vote : contract_auditor_votes) {
            auto const & contract = contract_auditor_vote.first;
            auto const & auditor_votes = contract_auditor_vote.second;

            auto iter = auditor_votes.find(account.value());
            if (iter != auditor_votes.end()) {
                adv_total_votes += base::xstring_utl::touint64(iter->second);
            }
        }

        return adv_total_votes;
    };

    auto add_table_vote_reward = [&](common::xaccount_address_t const & account,
                                   uint64_t adv_total_votes,
                                   top::xstake::uint128_t const & adv_reward_to_voters,
                                   std::map<std::string, std::map<std::string, std::string>> const & contract_auditor_votes) {
        if (adv_total_votes == 0)
            return;

        for (auto & contract_auditor_vote : contract_auditor_votes) {
            auto const & contract = contract_auditor_vote.first;
            auto const & auditor_votes = contract_auditor_vote.second;

            uint32_t table_id = 0;
            if (!xdatautil::extract_table_id_from_address(contract, table_id)) {
                xwarn("[xzec_reward_contract::calc_nodes_rewards_v2][xzec_reward_contract::add_table_vote_reward] extract_table_id_from_address %s  failed!\n", contract.c_str());
                continue;
            }
            auto const & reward_contract = CALC_CONTRACT_ADDRESS(sys_contract_sharding_reward_claiming_addr, table_id);
            auto iter = auditor_votes.find(account.value());
            if (iter != auditor_votes.end()) {
                auto adv_reward_to_contract = adv_reward_to_voters * base::xstring_utl::touint64(iter->second) / adv_total_votes;
                xdbg("[xzec_reward_contract::calc_nodes_rewards_v2][add_table_vote_reward] account: %s, contract: %s, table votes: %llu, adv_total_votes: %llu, adv_reward_to_voters: [%llu, %u], adv_reward_to_contract: [%llu, %u]\n",
                    account.c_str(),
                    contract.c_str(),
                    base::xstring_utl::touint64(iter->second),
                    adv_total_votes,
                    static_cast<uint64_t>(adv_reward_to_voters / REWARD_PRECISION),
                    static_cast<uint32_t>(adv_reward_to_voters % REWARD_PRECISION),
                    static_cast<uint64_t>(adv_reward_to_contract / REWARD_PRECISION),
                    static_cast<uint32_t>(adv_reward_to_contract % REWARD_PRECISION));
                if (adv_reward_to_contract > 0) {
                    contract_rewards[reward_contract] += adv_reward_to_contract;
                    contract_auditor_vote_rewards[reward_contract][account.value()] += adv_reward_to_contract;
                }
            }
        }
    };

    auto add_workload_reward = [&](bool is_auditor, common::xaccount_address_t const & account, top::xstake::uint128_t const & cluster_total_rewards, std::map<std::string, std::string> const & clusters_workloads, top::xstake::uint128_t & seed_node_rewards, top::xstake::uint128_t & node_reward) {
        uint32_t zero_workload_val = 0;
        if (is_auditor) {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(cluster_zero_workload);
        } else {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(shard_zero_workload);
        }
        xdbg("[xzec_reward_contract::calc_nodes_rewards_v2][add_workload_reward] account: %s, is_auditor: %d, %d clusters report workloads, cluster_total_rewards: [%llu, %u], zero_workload_val: %u\n",
             account.c_str(),
             is_auditor,
             clusters_workloads.size(),
             static_cast<uint64_t>(cluster_total_rewards / REWARD_PRECISION),
             static_cast<uint32_t>(cluster_total_rewards % REWARD_PRECISION),
             zero_workload_val);

        for (auto & cluster_workloads : clusters_workloads) {
            auto const & key_str = cluster_workloads.first;
            common::xcluster_address_t cluster;
            xstream_t key_stream(xcontext_t::instance(), (uint8_t *)key_str.data(), key_str.size());
            key_stream >> cluster;
            auto const & value_str = cluster_workloads.second;
            xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
            cluster_workload_t workload;
            workload.serialize_from(stream);
            xdbg("[xzec_reward_contract::calc_nodes_rewards_v2][add_workload_reward] account: %s, cluster id: %s, cluster size: %d, cluster_total_workload: %u\n",
                 account.c_str(),
                 cluster.to_string().c_str(),
                 workload.m_leader_count.size(),
                 workload.cluster_total_workload);
            if (workload.cluster_total_workload <= zero_workload_val)
                continue;
            auto it = workload.m_leader_count.find(account.value());
            if (it != workload.m_leader_count.end()) {
                auto const & work = it->second;
                auto workload_reward = cluster_total_rewards * work / workload.cluster_total_workload;
                node_reward += workload_reward;

                xdbg("[xzec_reward_contract::calc_nodes_rewards_v2][add_workload_reward] account: %s, cluster_id: %s, work: %d, total_workload: %d, cluster_total_rewards: [%llu, %u], reward: [%llu, %u]\n",
                     account.c_str(),
                     cluster.to_string().c_str(),
                     work,
                     workload.cluster_total_workload,
                     static_cast<uint64_t>(cluster_total_rewards / xstake::REWARD_PRECISION),
                     static_cast<uint32_t>(cluster_total_rewards % xstake::REWARD_PRECISION),
                     static_cast<uint64_t>(workload_reward / xstake::REWARD_PRECISION),
                     static_cast<uint32_t>(workload_reward % xstake::REWARD_PRECISION));
            }
        }
    };

    auto zero_workload_reward = [&](bool validator, top::xstake::uint128_t const & workload_total_reward, const std::map<std::string, std::string> & clusters_workloads, top::xstake::uint128_t & zero_workload_rewards) {
        std::size_t cluster_size;
        uint8_t group_id_begin;
        bool zero_workload = false;
        uint32_t zero_workload_val = 0;
        if (validator) {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(shard_zero_workload);
            cluster_size = XGET_ONCHAIN_GOVERNANCE_PARAMETER(validator_group_count);
            group_id_begin = common::xvalidator_group_id_begin.value();
        } else {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(cluster_zero_workload);
            cluster_size = XGET_ONCHAIN_GOVERNANCE_PARAMETER(auditor_group_count);
            group_id_begin = common::xauditor_group_id_begin.value();
        }
        if (cluster_size == 0) {
            xwarn("[xzec_reward_contract::calc_nodes_rewards_v2][zero_workload_reward] validator_workload: %d, cluster_size zero", validator);
            return;
        }
        top::xstake::uint128_t cluster_total_rewards = workload_total_reward / cluster_size;  // averaged by all clusters
        for (auto group_id = group_id_begin; group_id < group_id_begin + cluster_size; group_id++) {
            zero_workload = true;
            for (auto & cluster_workloads : clusters_workloads) {
                auto const & key_str = cluster_workloads.first;
                xstream_t stream(xcontext_t::instance(), (uint8_t *)key_str.data(), key_str.size());
                common::xcluster_address_t cluster;
                stream >> cluster;
                if (group_id == cluster.group_id().value()) {
                    auto const & value_str = cluster_workloads.second;
                    xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
                    cluster_workload_t workload;
                    workload.serialize_from(stream);
                    if (workload.cluster_total_workload > zero_workload_val) {
                        zero_workload = false;
                    }
                    xdbg("[xzec_reward_contract::calc_nodes_rewards_v2][zero_workload_reward] group %u has workload %u, zero_workload: %d", group_id, workload.cluster_total_workload, zero_workload);
                    break;
                }
            }
            if (zero_workload) {
                zero_workload_rewards += cluster_total_rewards;
            }
            xdbg("[xzec_reward_contract::calc_nodes_rewards_v2][zero_workload_reward] validator: %d, cluster_size: %u, group %u, zero_workload: %d, cluster_total_rewards: [%llu, %u], zero_workload_rewards: [%llu, %u]",
                 validator,
                 cluster_size,
                 group_id,
                 zero_workload,
                 static_cast<uint64_t>(cluster_total_rewards / xstake::REWARD_PRECISION),
                 static_cast<uint32_t>(cluster_total_rewards % xstake::REWARD_PRECISION),
                 static_cast<uint64_t>(zero_workload_rewards / xstake::REWARD_PRECISION),
                 static_cast<uint32_t>(zero_workload_rewards % xstake::REWARD_PRECISION));
        }
    };

    // preprocess workload
    auto preprocess_workload = [&](bool is_auditor, std::map<std::string, std::string> & clusters_workloads, std::map<std::string, xreg_node_info> const & map_nodes) {
        uint32_t zero_workload_val = 0;
        if (is_auditor) {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(cluster_zero_workload);
        } else {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(shard_zero_workload);
        }

        xdbg("[xzec_reward_contract::calc_nodes_rewards_v2][preprocess_workload] is_auditor: %u, total group num: %d\n",
            is_auditor,
            clusters_workloads.size());
        for (auto it = clusters_workloads.begin(); it != clusters_workloads.end(); ) {
            auto const & key_str = it->first;
            common::xcluster_address_t cluster;
            xstream_t key_stream(xcontext_t::instance(), (uint8_t *)key_str.data(), key_str.size());
            key_stream >> cluster;
            auto const & value_str = it->second;
            xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
            cluster_workload_t workload;
            bool workload_changed = false;
            workload.serialize_from(stream);
            xdbg("[xzec_reward_contract::calc_nodes_rewards_v2][preprocess_workload] is_auditor: %u, auditor cluster id: %s, cluster size: %d, cluster_total_workload: %u\n",
                 is_auditor,
                 cluster.to_string().c_str(),
                 workload.m_leader_count.size(),
                 workload.cluster_total_workload);
            if (workload.cluster_total_workload <= zero_workload_val) {
                xinfo("[xzec_reward_contract::calc_nodes_rewards_v2][preprocess_workload] is_auditor: %u, cluster id: %s, cluster size: %d, cluster_total_workload: %u, cluster workloads are <= zero_workload_val and will be ignored\n",
                    is_auditor,
                    cluster.to_string().c_str(),
                    workload.m_leader_count.size(),
                    workload.cluster_total_workload);
                clusters_workloads.erase(it++);
                continue;
            }

            for (auto it2 = workload.m_leader_count.begin(); it2 != workload.m_leader_count.end(); ) {
                xreg_node_info node;
                if (get_node_info(map_nodes, it2->first, node) != 0) {
                    xinfo("[xzec_reward_contract::calc_nodes_rewards_v2][preprocess_workload] account: %s not in map nodes", it2->first.c_str());
                    workload.cluster_total_workload -= it2->second;
                    workload.m_leader_count.erase(it2++);
                    workload_changed = true;
                    continue;
                }

                if (is_auditor) {
                    if (node.get_deposit() == 0 || !node.is_valid_auditor_node()) {
                        xinfo("[xzec_reward_contract::calc_nodes_rewards_v2][preprocess_workload] account: %s is not a valid auditor, deposit: %llu, votes: %llu",
                            it2->first.c_str(), node.get_deposit(), node.m_vote_amount);
                        workload.cluster_total_workload -= it2->second;
                        workload.m_leader_count.erase(it2++);
                        workload_changed = true;
                    } else {
                        it2++;
                    }
                } else {
                    if (node.get_deposit() == 0 || !node.is_validator_node()) {
                        xinfo("[xzec_reward_contract::calc_nodes_rewards_v2][preprocess_workload] account: %s is not a valid validator, deposit: %llu",
                            it2->first.c_str(), node.get_deposit());
                        workload.cluster_total_workload -= it2->second;
                        workload.m_leader_count.erase(it2++);
                        workload_changed = true;
                    } else {
                        it2++;
                    }
                }
            } // end of group
            if (workload.m_leader_count.size() == 0) {
                clusters_workloads.erase(it++);
            } else {
                if (workload_changed) {
                    xstream_t stream(xcontext_t::instance());
                    workload.serialize_to(stream);
                    it->second = std::string((const char*)stream.data(), stream.size());
                }
                it++;
            }
        }
    };

    std::map<std::string, std::string> auditor_clusters_workloads;
    std::map<std::string, std::string> validator_clusters_workloads;
    base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)workload_str.data(), workload_str.size());
    MAP_DESERIALIZE_SIMPLE(stream, auditor_clusters_workloads);
    MAP_DESERIALIZE_SIMPLE(stream, validator_clusters_workloads);

    // contract auditor votes
    std::map<std::string, std::string> contract_auditor_votes2;
    MAP_COPY_GET(XPORPERTY_CONTRACT_TICKETS_KEY, contract_auditor_votes2, sys_contract_zec_vote_addr);

    xdbg("[xzec_reward_contract::calc_nodes_rewards_v2] contract_auditor_votes2 size: %d", contract_auditor_votes2.size());
    // transform to map of map struct
    std::map<std::string, std::map<std::string, std::string>> contract_auditor_votes;
    for (auto & contract_auditor_vote : contract_auditor_votes2) {
        auto const & contract = contract_auditor_vote.first;
        auto const & auditor_votes_str = contract_auditor_vote.second;

        std::map<std::string, std::string> auditor_votes;
        xstream_t stream(xcontext_t::instance(), (uint8_t *)auditor_votes_str.data(), auditor_votes_str.size());
        stream >> auditor_votes;
        contract_auditor_votes[contract] = auditor_votes;
    }

    uint64_t cur_time = onchain_timer_round;
    uint64_t activation_time = get_activated_time();
    int64_t total_height = cur_time - activation_time;
    uint32_t edge_num = 0;
    uint32_t archive_num = 0;
    uint32_t total_auditor_nodes = 0;
    auto issuance = calc_issuance(total_height);
    auto auditor_total_rewards      = get_reward(issuance, xreward_type::auditor_reward);
    auto validator_total_rewards    = get_reward(issuance, xreward_type::validator_reward);
    auto edge_total_rewards         = get_reward(issuance, xreward_type::edge_reward);
    auto archive_total_rewards      = get_reward(issuance, xreward_type::archive_reward);
    auto total_vote_rewards         = get_reward(issuance, xreward_type::vote_reward);
    auto governance_rewards         = get_reward(issuance, xreward_type::governance_reward);
    std::size_t auditor_group_count = XGET_ONCHAIN_GOVERNANCE_PARAMETER(auditor_group_count);
    XCONTRACT_ENSURE(auditor_group_count > 0, "auditor group count equals zero");
    top::xstake::uint128_t auditor_group_rewards = auditor_total_rewards / auditor_group_count;
    std::size_t validator_group_count = XGET_ONCHAIN_GOVERNANCE_PARAMETER(validator_group_count);
    XCONTRACT_ENSURE(validator_group_count > 0, "validator group count equals zero");
    top::xstake::uint128_t validator_group_rewards = validator_total_rewards / validator_group_count;
    top::xstake::uint128_t zero_workload_rewards = 0;
    top::xstake::uint128_t seed_node_rewards = 0;

    // transform map_nodes
    std::map<std::string, xreg_node_info> map_nodes;
    {
        std::map<std::string, std::string> map_nodes2;
        auto const last_read_height = static_cast<std::uint64_t>(std::stoull(STRING_GET(XPROPERTY_LAST_READ_REC_REG_CONTRACT_BLOCK_HEIGHT)));
        GET_MAP_PROPERTY(XPORPERTY_CONTRACT_REG_KEY, map_nodes2, last_read_height, sys_contract_rec_registration_addr);
        //MAP_COPY_GET(XPORPERTY_CONTRACT_REG_KEY, map_nodes2, sys_contract_rec_registration_addr);
        xdbg("[xzec_reward_contract::calc_nodes_rewards_v2] last_read_height: %llu, map_nodes2 size: %d",
            last_read_height, map_nodes2.size());

        for (auto const & entity : map_nodes2) {
            auto const & account = entity.first;
            auto const & value_str = entity.second;
            xreg_node_info node;
            xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
            node.serialize_from(stream);
            node.m_vote_amount = get_adv_total_votes(contract_auditor_votes, node.m_account);
            map_nodes[account] = node;
            xdbg("[xzec_reward_contract::calc_nodes_rewards_v2] map_nodes: account: %s, deposit: %llu, node_type: %s, votes: %llu",
                node.m_account.c_str(),
                node.get_deposit(),
                node.m_genesis_node ? "advance,validator,edge" : common::to_string(node.m_registered_role).c_str(),
                node.m_vote_amount);
            if (node.get_deposit() > 0 && node.is_edge_node()) {
                edge_num++;
            }
            if (node.get_deposit() > 0 && node.is_valid_archive_node()) {
                archive_num++;
            }
            if (node.get_deposit() > 0 && node.is_valid_auditor_node()) {
                total_auditor_nodes++;
            }
        }
    }

    // preprocess workload
    preprocess_workload(false, validator_clusters_workloads, map_nodes);
    preprocess_workload(true, auditor_clusters_workloads, map_nodes);

    // count all votes
    uint64_t all_tickets = 0;
    for (auto const & entity : contract_auditor_votes) {
        auto const & auditor_votes = entity.second;

        for (auto const & entity2 : auditor_votes) {
            xreg_node_info node;
            if (get_node_info(map_nodes, entity2.first, node) != 0)  {
                xwarn("[xzec_reward_contract::calc_nodes_rewards_v2] account %s not in map_nodes", entity2.first.c_str());
                continue;
            }

            if (node.get_deposit() > 0 && node.is_valid_auditor_node()) {
                all_tickets += base::xstring_utl::touint64(entity2.second);
            }
        }
    }
    if (total_auditor_nodes > 0) {
        xassert(all_tickets > 0);
    }

    xinfo(
        "[xzec_reward_contract::calc_nodes_rewards_v2] cur_time: %llu, activation_time: %llu, "
        "issuance: [%llu, %u], "
        "edge total rewards: [%llu, %u], edge num: %d, "
        "archive_total_rewards: [%llu, %u], archive num: %d, "
        "auditor_total_rewards: [%llu, %u], validator_total_rewards: [%llu, %u], "
        "total_vote_rewards: [%llu, %u], governance_rewards: [%llu, %u], all_tickets: %llu, total_auditor_nodes: %u\n",
        cur_time,
        activation_time,
        static_cast<uint64_t>(issuance / REWARD_PRECISION),
        static_cast<uint32_t>(issuance % REWARD_PRECISION),
        static_cast<uint64_t>(edge_total_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(edge_total_rewards % REWARD_PRECISION),
        edge_num,
        static_cast<uint64_t>(archive_total_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(archive_total_rewards % REWARD_PRECISION),
        archive_num,
        static_cast<uint64_t>(auditor_total_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(auditor_total_rewards % REWARD_PRECISION),
        static_cast<uint64_t>(validator_total_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(validator_total_rewards % REWARD_PRECISION),
        static_cast<uint64_t>(total_vote_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(total_vote_rewards % REWARD_PRECISION),
        static_cast<uint64_t>(governance_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(governance_rewards % REWARD_PRECISION),
        all_tickets,
        total_auditor_nodes);
    for (auto const & entity : map_nodes) {
        auto const & account = entity.first;
        auto const & node = entity.second;
        top::xstake::uint128_t node_reward = 0;

        if (edge_num > 0 && node.is_edge_node() && node.get_deposit() > 0) {
            //add_node_reward(account, xreward_type::edge_reward, edge_total_rewards / edge_num);
            auto edge_reward = edge_total_rewards / edge_num;
            xdbg("[xzec_reward_contract::calc_nodes_rewards_v2] account: %s, edge reward: [%llu, %u]",
                account.c_str(),
                static_cast<uint64_t>(edge_reward / xstake::REWARD_PRECISION),
                static_cast<uint32_t>(edge_reward % xstake::REWARD_PRECISION));
            node_reward += edge_reward;
        }
        if (archive_num > 0 && node.is_valid_archive_node() && node.get_deposit() > 0) {
            //add_node_reward(account, xreward_type::archive_reward, archive_total_rewards / archive_num);
            auto archive_reward = archive_total_rewards / archive_num;
            xdbg("[xzec_reward_contract::calc_nodes_rewards_v2] account: %s, archive reward: [%llu, %u]",
                account.c_str(),
                static_cast<uint64_t>(archive_reward / xstake::REWARD_PRECISION),
                static_cast<uint32_t>(archive_reward % xstake::REWARD_PRECISION));
            node_reward += archive_reward;
        }
        if (node.is_validator_node() && node.get_deposit() > 0) {
            add_workload_reward(false, node.m_account, validator_group_rewards, validator_clusters_workloads, seed_node_rewards, node_reward);
        }
        auto adv_total_votes = node.m_vote_amount;
        if (node.is_valid_auditor_node() && node.get_deposit() > 0) {
            add_workload_reward(true, node.m_account, auditor_group_rewards, auditor_clusters_workloads, seed_node_rewards, node_reward);
            // vote reward
            xassert(all_tickets > 0);
            auto node_vote_reward = adv_total_votes * total_vote_rewards / all_tickets;
            xdbg("[xzec_reward_contract::calc_nodes_rewards_v2] account: %s, node_vote_reward: [%llu, %u], node deposit: %llu, all_tickets: %llu, adv_total_votes: %llu",
                    account.c_str(),
                    static_cast<uint64_t>(node_vote_reward / REWARD_PRECISION),
                    static_cast<uint32_t>(node_vote_reward % REWARD_PRECISION),
                    node.get_deposit(),
                    all_tickets,
                    adv_total_votes);
            node_reward += node_vote_reward;
        }
        // vote dividend
        if (adv_total_votes > 0 && node.m_support_ratio_numerator > 0) {
            auto adv_reward_to_self = node_reward * (node.m_support_ratio_denominator - node.m_support_ratio_numerator) / node.m_support_ratio_denominator;
            auto adv_reward_to_voters = node_reward - adv_reward_to_self;
            add_table_vote_reward(node.m_account, adv_total_votes, adv_reward_to_voters, contract_auditor_votes);
            node_reward = adv_reward_to_self;
        }
        add_table_node_reward(node.m_account, node_reward);
    }
    if (edge_num == 0) seed_node_rewards += edge_total_rewards;
    if (archive_num == 0) seed_node_rewards += archive_total_rewards;
    if (total_auditor_nodes == 0) seed_node_rewards += total_vote_rewards;
    zero_workload_reward(true, validator_total_rewards, validator_clusters_workloads, zero_workload_rewards);
    zero_workload_reward(false, auditor_total_rewards, auditor_clusters_workloads, zero_workload_rewards);

    // clear accumulated workloads
    // CLEAR(enum_type_t::map, XPORPERTY_CONTRACT_WORKLOAD_KEY);
    // CLEAR(enum_type_t::map, XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY);
    // CALL(common::xaccount_address_t{sys_contract_zec_workload_addr}, "clear_workload", std::string(""));
    // uint32_t task_id = get_task_id();
    // add_task(task_id, onchain_timer_round, sys_contract_zec_workload_addr, XZEC_WORKLOAD_CLEAR_WORKLOAD_ACTION, std::string(""));
    // task_id++;

    // governance rewards
    // request additional issuance
    uint64_t common_funds = static_cast<uint64_t>( (governance_rewards + zero_workload_rewards + seed_node_rewards) / REWARD_PRECISION );
    if ( common_funds > 0 ) {
        uint32_t task_id = get_task_id();
        std::map<std::string, uint64_t> issuances;
        issuances.emplace(sys_contract_rec_tcc_addr, common_funds);
        base::xstream_t seo_stream(base::xcontext_t::instance());
        seo_stream << issuances;
        add_task(task_id, onchain_timer_round, "", XTRANSFER_ACTION, std::string((char *)seo_stream.data(), seo_stream.size()));
        task_id++;
        update_accumulated_issuance(common_funds, onchain_timer_round);
        XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "calc_nodes_rewards_Executed", 1);
    }
}

void xzec_reward_contract::calc_nodes_rewards_v3(std::map<std::string, std::map<std::string, top::xstake::uint128_t >> & table_nodes_rewards,
                                              std::map<std::string, top::xstake::uint128_t> & contract_rewards,
                                              std::map<std::string, std::map<std::string, top::xstake::uint128_t >> & contract_auditor_vote_rewards,
                                              const uint64_t onchain_timer_round) {
    XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "calc_nodes_rewards_Called", 1);
    XMETRICS_TIME_RECORD(XREWARD_CONTRACT "calc_nodes_rewards_ExecutionTime");

    auto add_table_node_reward = [&](common::xaccount_address_t const & account, top::xstake::uint128_t node_reward) {
        if (node_reward == 0)
            return;

        uint32_t table_id = 0;
        if (!EXTRACT_TABLE_ID(account, table_id)) {
            xwarn("[xzec_reward_contract::calc_nodes_rewards_v3][xzec_reward_contract::add_table_node_reward] node reward pid: %d, account: %s, node_reward: [%llu, %u]\n",
                getpid(), account.c_str(), static_cast<uint64_t>(node_reward / REWARD_PRECISION), static_cast<uint32_t>(node_reward % REWARD_PRECISION));
            return;
        }

        auto const & reward_contract = CALC_CONTRACT_ADDRESS(sys_contract_sharding_reward_claiming_addr, table_id);
        xdbg("[xzec_reward_contract::calc_nodes_rewards_v3][xzec_reward_contract::add_table_node_reward] node reward, pid:%d, reward_contract: %s, account: %s, reward: [%llu, %u]\n",
             getpid(),
             reward_contract.c_str(),
             account.c_str(),
             static_cast<uint64_t>(node_reward / REWARD_PRECISION), static_cast<uint32_t>(node_reward % REWARD_PRECISION));

        contract_rewards[reward_contract] += node_reward;
        table_nodes_rewards[reward_contract][account.value()] = node_reward;
    };

    auto get_adv_total_votes = [&](std::map<std::string, std::map<std::string, std::string>> const & contract_auditor_votes, common::xaccount_address_t const & account) {
        uint64_t adv_total_votes = 0;

        for (auto const & contract_auditor_vote : contract_auditor_votes) {
            auto const & contract = contract_auditor_vote.first;
            auto const & auditor_votes = contract_auditor_vote.second;

            auto iter = auditor_votes.find(account.value());
            if (iter != auditor_votes.end()) {
                adv_total_votes += base::xstring_utl::touint64(iter->second);
            }
        }

        return adv_total_votes;
    };

    auto add_table_vote_reward = [&](common::xaccount_address_t const & account,
                                   uint64_t adv_total_votes,
                                   top::xstake::uint128_t const & adv_reward_to_voters,
                                   std::map<std::string, std::map<std::string, std::string>> const & contract_auditor_votes) {
        if (adv_total_votes == 0)
            return;

        for (auto & contract_auditor_vote : contract_auditor_votes) {
            auto const & contract = contract_auditor_vote.first;
            auto const & auditor_votes = contract_auditor_vote.second;

            uint32_t table_id = 0;
            if (!xdatautil::extract_table_id_from_address(contract, table_id)) {
                xwarn("[xzec_reward_contract::calc_nodes_rewards_v3][xzec_reward_contract::add_table_vote_reward] extract_table_id_from_address %s  failed!\n", contract.c_str());
                continue;
            }
            auto const & reward_contract = CALC_CONTRACT_ADDRESS(sys_contract_sharding_reward_claiming_addr, table_id);
            auto iter = auditor_votes.find(account.value());
            if (iter != auditor_votes.end()) {
                auto adv_reward_to_contract = adv_reward_to_voters * base::xstring_utl::touint64(iter->second) / adv_total_votes;
                xdbg("[xzec_reward_contract::calc_nodes_rewards_v3][add_table_vote_reward] account: %s, contract: %s, table votes: %llu, adv_total_votes: %llu, adv_reward_to_voters: [%llu, %u], adv_reward_to_contract: [%llu, %u]\n",
                    account.c_str(),
                    contract.c_str(),
                    base::xstring_utl::touint64(iter->second),
                    adv_total_votes,
                    static_cast<uint64_t>(adv_reward_to_voters / REWARD_PRECISION),
                    static_cast<uint32_t>(adv_reward_to_voters % REWARD_PRECISION),
                    static_cast<uint64_t>(adv_reward_to_contract / REWARD_PRECISION),
                    static_cast<uint32_t>(adv_reward_to_contract % REWARD_PRECISION));
                if (adv_reward_to_contract > 0) {
                    contract_rewards[reward_contract] += adv_reward_to_contract;
                    contract_auditor_vote_rewards[reward_contract][account.value()] += adv_reward_to_contract;
                }
            }
        }
    };

    auto add_workload_reward = [&](bool is_auditor, common::xaccount_address_t const & account, top::xstake::uint128_t const & cluster_total_rewards, std::map<std::string, std::string> const & clusters_workloads, top::xstake::uint128_t & seed_node_rewards, top::xstake::uint128_t & node_reward) {
        uint32_t zero_workload_val = 0;
        if (is_auditor) {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(cluster_zero_workload);
        } else {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(shard_zero_workload);
        }
        xdbg("[xzec_reward_contract::calc_nodes_rewards_v3][add_workload_reward] account: %s, is_auditor: %d, %d clusters report workloads, cluster_total_rewards: [%llu, %u], zero_workload_val: %u\n",
             account.c_str(),
             is_auditor,
             clusters_workloads.size(),
             static_cast<uint64_t>(cluster_total_rewards / REWARD_PRECISION),
             static_cast<uint32_t>(cluster_total_rewards % REWARD_PRECISION),
             zero_workload_val);

        for (auto & cluster_workloads : clusters_workloads) {
            auto const & key_str = cluster_workloads.first;
            common::xcluster_address_t cluster;
            xstream_t key_stream(xcontext_t::instance(), (uint8_t *)key_str.data(), key_str.size());
            key_stream >> cluster;
            auto const & value_str = cluster_workloads.second;
            xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
            cluster_workload_t workload;
            workload.serialize_from(stream);
            xdbg("[xzec_reward_contract::calc_nodes_rewards_v3][add_workload_reward] account: %s, cluster id: %s, cluster size: %d, cluster_total_workload: %u\n",
                 account.c_str(),
                 cluster.to_string().c_str(),
                 workload.m_leader_count.size(),
                 workload.cluster_total_workload);
            if (workload.cluster_total_workload <= zero_workload_val)
                continue;
            auto it = workload.m_leader_count.find(account.value());
            if (it != workload.m_leader_count.end()) {
                auto const & work = it->second;
                auto workload_reward = cluster_total_rewards * work / workload.cluster_total_workload;
                node_reward += workload_reward;

                xdbg("[xzec_reward_contract::calc_nodes_rewards_v3][add_workload_reward] account: %s, cluster_id: %s, work: %d, total_workload: %d, cluster_total_rewards: [%llu, %u], reward: [%llu, %u]\n",
                     account.c_str(),
                     cluster.to_string().c_str(),
                     work,
                     workload.cluster_total_workload,
                     static_cast<uint64_t>(cluster_total_rewards / xstake::REWARD_PRECISION),
                     static_cast<uint32_t>(cluster_total_rewards % xstake::REWARD_PRECISION),
                     static_cast<uint64_t>(workload_reward / xstake::REWARD_PRECISION),
                     static_cast<uint32_t>(workload_reward % xstake::REWARD_PRECISION));
            }
        }
    };

    auto zero_workload_reward = [&](bool validator, top::xstake::uint128_t const & workload_total_reward, const std::map<std::string, std::string> & clusters_workloads, top::xstake::uint128_t & zero_workload_rewards) {
        std::size_t cluster_size;
        uint8_t group_id_begin;
        bool zero_workload = false;
        uint32_t zero_workload_val = 0;
        if (validator) {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(shard_zero_workload);
            cluster_size = XGET_ONCHAIN_GOVERNANCE_PARAMETER(validator_group_count);
            group_id_begin = common::xvalidator_group_id_begin.value();
        } else {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(cluster_zero_workload);
            cluster_size = XGET_ONCHAIN_GOVERNANCE_PARAMETER(auditor_group_count);
            group_id_begin = common::xauditor_group_id_begin.value();
        }
        if (cluster_size == 0) {
            xwarn("[xzec_reward_contract::calc_nodes_rewards_v3][zero_workload_reward] validator_workload: %d, cluster_size zero", validator);
            return;
        }
        top::xstake::uint128_t cluster_total_rewards = workload_total_reward / cluster_size;  // averaged by all clusters
        for (auto group_id = group_id_begin; group_id < group_id_begin + cluster_size; group_id++) {
            zero_workload = true;
            for (auto & cluster_workloads : clusters_workloads) {
                auto const & key_str = cluster_workloads.first;
                xstream_t stream(xcontext_t::instance(), (uint8_t *)key_str.data(), key_str.size());
                common::xcluster_address_t cluster;
                stream >> cluster;
                if (group_id == cluster.group_id().value()) {
                    auto const & value_str = cluster_workloads.second;
                    xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
                    cluster_workload_t workload;
                    workload.serialize_from(stream);
                    if (workload.cluster_total_workload > zero_workload_val) {
                        zero_workload = false;
                    }
                    xdbg("[xzec_reward_contract::calc_nodes_rewards_v3][zero_workload_reward] group %u has workload %u, zero_workload: %d", group_id, workload.cluster_total_workload, zero_workload);
                    break;
                }
            }
            if (zero_workload) {
                zero_workload_rewards += cluster_total_rewards;
            }
            xdbg("[xzec_reward_contract::calc_nodes_rewards_v3][zero_workload_reward] validator: %d, cluster_size: %u, group %u, zero_workload: %d, cluster_total_rewards: [%llu, %u], zero_workload_rewards: [%llu, %u]",
                 validator,
                 cluster_size,
                 group_id,
                 zero_workload,
                 static_cast<uint64_t>(cluster_total_rewards / xstake::REWARD_PRECISION),
                 static_cast<uint32_t>(cluster_total_rewards % xstake::REWARD_PRECISION),
                 static_cast<uint64_t>(zero_workload_rewards / xstake::REWARD_PRECISION),
                 static_cast<uint32_t>(zero_workload_rewards % xstake::REWARD_PRECISION));
        }
    };

    // preprocess workload
    auto preprocess_workload = [&](bool is_auditor, std::map<std::string, std::string> & clusters_workloads, std::map<std::string, xreg_node_info> const & map_nodes) {
        uint32_t zero_workload_val = 0;
        if (is_auditor) {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(cluster_zero_workload);
        } else {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(shard_zero_workload);
        }

        xdbg("[xzec_reward_contract::calc_nodes_rewards_v3][preprocess_workload] is_auditor: %u, total group num: %d\n",
            is_auditor,
            clusters_workloads.size());
        for (auto it = clusters_workloads.begin(); it != clusters_workloads.end(); ) {
            auto const & key_str = it->first;
            common::xcluster_address_t cluster;
            xstream_t key_stream(xcontext_t::instance(), (uint8_t *)key_str.data(), key_str.size());
            key_stream >> cluster;
            auto const & value_str = it->second;
            xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
            cluster_workload_t workload;
            bool workload_changed = false;
            workload.serialize_from(stream);
            xdbg("[xzec_reward_contract::calc_nodes_rewards_v3][preprocess_workload] is_auditor: %u, auditor cluster id: %s, cluster size: %d, cluster_total_workload: %u\n",
                 is_auditor,
                 cluster.to_string().c_str(),
                 workload.m_leader_count.size(),
                 workload.cluster_total_workload);
            if (workload.cluster_total_workload <= zero_workload_val) {
                xinfo("[xzec_reward_contract::calc_nodes_rewards_v3][preprocess_workload] is_auditor: %u, cluster id: %s, cluster size: %d, cluster_total_workload: %u, cluster workloads are <= zero_workload_val and will be ignored\n",
                    is_auditor,
                    cluster.to_string().c_str(),
                    workload.m_leader_count.size(),
                    workload.cluster_total_workload);
                clusters_workloads.erase(it++);
                continue;
            }

            for (auto it2 = workload.m_leader_count.begin(); it2 != workload.m_leader_count.end(); ) {
                xreg_node_info node;
                if (get_node_info(map_nodes, it2->first, node) != 0) {
                    xinfo("[xzec_reward_contract::calc_nodes_rewards_v3][preprocess_workload] account: %s not in map nodes", it2->first.c_str());
                    workload.cluster_total_workload -= it2->second;
                    workload.m_leader_count.erase(it2++);
                    workload_changed = true;
                    continue;
                }

                if (is_auditor) {
                    if (node.get_deposit() == 0 || !node.is_valid_auditor_node()) {
                        xinfo("[xzec_reward_contract::calc_nodes_rewards_v3][preprocess_workload] account: %s is not a valid auditor, deposit: %llu, votes: %llu",
                            it2->first.c_str(), node.get_deposit(), node.m_vote_amount);
                        workload.cluster_total_workload -= it2->second;
                        workload.m_leader_count.erase(it2++);
                        workload_changed = true;
                    } else {
                        it2++;
                    }
                } else {
                    if (node.get_deposit() == 0 || !node.is_validator_node()) {
                        xinfo("[xzec_reward_contract::calc_nodes_rewards_v3][preprocess_workload] account: %s is not a valid validator, deposit: %llu",
                            it2->first.c_str(), node.get_deposit());
                        workload.cluster_total_workload -= it2->second;
                        workload.m_leader_count.erase(it2++);
                        workload_changed = true;
                    } else {
                        it2++;
                    }
                }
            } // end of group
            if (workload.m_leader_count.size() == 0) {
                clusters_workloads.erase(it++);
            } else {
                if (workload_changed) {
                    xstream_t stream(xcontext_t::instance());
                    workload.serialize_to(stream);
                    it->second = std::string((const char*)stream.data(), stream.size());
                }
                it++;
            }
        }
    };

    std::map<std::string, std::string> auditor_clusters_workloads;
    std::map<std::string, std::string> validator_clusters_workloads;
    //base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)workload_str.data(), workload_str.size());
    //MAP_DESERIALIZE_SIMPLE(stream, auditor_clusters_workloads);
    //MAP_DESERIALIZE_SIMPLE(stream, validator_clusters_workloads);

    // auditor workload, property not created in setup
    try {
        MAP_COPY_GET(XPORPERTY_CONTRACT_WORKLOAD_KEY, auditor_clusters_workloads);
    } catch (std::runtime_error & e) {
        xdbg("[xzec_reward_contract::calc_nodes_rewards_v3] MAP COPY GET XPORPERTY_CONTRACT_WORKLOAD_KEY error: %s", e.what());
    }

    // validator workload, property not created in setup
    try {
        MAP_COPY_GET(XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY, validator_clusters_workloads);
    } catch (std::runtime_error & e) {
        xdbg("[xzec_reward_contract::calc_nodes_rewards_v3] MAP COPY GET XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY error: %s", e.what());
    }

    try {
        clear_workload();
    } catch (std::runtime_error & e) {
        xdbg("[xzec_reward_contract::calc_nodes_rewards_v3] clear_workload error: %s", e.what());
    }

    // contract auditor votes
    std::map<std::string, std::string> contract_auditor_votes2;
    MAP_COPY_GET(XPORPERTY_CONTRACT_TICKETS_KEY, contract_auditor_votes2, sys_contract_zec_vote_addr);

    xdbg("[xzec_reward_contract::calc_nodes_rewards_v3] contract_auditor_votes2 size: %d", contract_auditor_votes2.size());
    // transform to map of map struct
    std::map<std::string, std::map<std::string, std::string>> contract_auditor_votes;
    for (auto & contract_auditor_vote : contract_auditor_votes2) {
        auto const & contract = contract_auditor_vote.first;
        auto const & auditor_votes_str = contract_auditor_vote.second;

        std::map<std::string, std::string> auditor_votes;
        xstream_t stream(xcontext_t::instance(), (uint8_t *)auditor_votes_str.data(), auditor_votes_str.size());
        stream >> auditor_votes;
        contract_auditor_votes[contract] = auditor_votes;
    }

    uint64_t cur_time = onchain_timer_round;
    uint64_t activation_time = get_activated_time();
    int64_t total_height = cur_time - activation_time;
    uint32_t edge_num = 0;
    uint32_t archive_num = 0;
    uint32_t total_auditor_nodes = 0;
    auto issuance = calc_issuance(total_height);
    auto auditor_total_rewards      = get_reward(issuance, xreward_type::auditor_reward);
    auto validator_total_rewards    = get_reward(issuance, xreward_type::validator_reward);
    auto edge_total_rewards         = get_reward(issuance, xreward_type::edge_reward);
    auto archive_total_rewards      = get_reward(issuance, xreward_type::archive_reward);
    auto total_vote_rewards         = get_reward(issuance, xreward_type::vote_reward);
    auto governance_rewards         = get_reward(issuance, xreward_type::governance_reward);
    std::size_t auditor_group_count = XGET_ONCHAIN_GOVERNANCE_PARAMETER(auditor_group_count);
    XCONTRACT_ENSURE(auditor_group_count > 0, "auditor group count equals zero");
    top::xstake::uint128_t auditor_group_rewards = auditor_total_rewards / auditor_group_count;
    std::size_t validator_group_count = XGET_ONCHAIN_GOVERNANCE_PARAMETER(validator_group_count);
    XCONTRACT_ENSURE(validator_group_count > 0, "validator group count equals zero");
    top::xstake::uint128_t validator_group_rewards = validator_total_rewards / validator_group_count;
    top::xstake::uint128_t zero_workload_rewards = 0;
    top::xstake::uint128_t seed_node_rewards = 0;

    // transform map_nodes
    std::map<std::string, xreg_node_info> map_nodes;
    {
        std::map<std::string, std::string> map_nodes2;
        auto const last_read_height = static_cast<std::uint64_t>(std::stoull(STRING_GET(XPROPERTY_LAST_READ_REC_REG_CONTRACT_BLOCK_HEIGHT)));
        GET_MAP_PROPERTY(XPORPERTY_CONTRACT_REG_KEY, map_nodes2, last_read_height, sys_contract_rec_registration_addr);
        //MAP_COPY_GET(XPORPERTY_CONTRACT_REG_KEY, map_nodes2, sys_contract_rec_registration_addr);
        xdbg("[xzec_reward_contract::calc_nodes_rewards_v3] last_read_height: %llu, map_nodes2 size: %d",
            last_read_height, map_nodes2.size());

        for (auto const & entity : map_nodes2) {
            auto const & account = entity.first;
            auto const & value_str = entity.second;
            xreg_node_info node;
            xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
            node.serialize_from(stream);
            node.m_vote_amount = get_adv_total_votes(contract_auditor_votes, node.m_account);
            map_nodes[account] = node;
            xdbg("[xzec_reward_contract::calc_nodes_rewards_v3] map_nodes: account: %s, deposit: %llu, node_type: %s, votes: %llu",
                node.m_account.c_str(),
                node.get_deposit(),
                node.m_genesis_node ? "advance,validator,edge" : common::to_string(node.m_registered_role).c_str(),
                node.m_vote_amount);
            if (node.get_deposit() > 0 && node.is_edge_node()) {
                edge_num++;
            }
            if (node.get_deposit() > 0 && node.is_valid_archive_node()) {
                archive_num++;
            }
            if (node.get_deposit() > 0 && node.is_valid_auditor_node()) {
                total_auditor_nodes++;
            }
        }
    }

    // preprocess workload
    preprocess_workload(false, validator_clusters_workloads, map_nodes);
    preprocess_workload(true, auditor_clusters_workloads, map_nodes);

    // count all votes
    uint64_t all_tickets = 0;
    for (auto const & entity : contract_auditor_votes) {
        auto const & auditor_votes = entity.second;

        for (auto const & entity2 : auditor_votes) {
            xreg_node_info node;
            if (get_node_info(map_nodes, entity2.first, node) != 0)  {
                xwarn("[xzec_reward_contract::calc_nodes_rewards_v3] account %s not in map_nodes", entity2.first.c_str());
                continue;
            }

            if (node.get_deposit() > 0 && node.is_valid_auditor_node()) {
                all_tickets += base::xstring_utl::touint64(entity2.second);
            }
        }
    }
    if (total_auditor_nodes > 0) {
        xassert(all_tickets > 0);
    }

    xinfo(
        "[xzec_reward_contract::calc_nodes_rewards_v3] cur_time: %llu, activation_time: %llu, "
        "issuance: [%llu, %u], "
        "edge total rewards: [%llu, %u], edge num: %d, "
        "archive_total_rewards: [%llu, %u], archive num: %d, "
        "auditor_total_rewards: [%llu, %u], validator_total_rewards: [%llu, %u], "
        "total_vote_rewards: [%llu, %u], governance_rewards: [%llu, %u], all_tickets: %llu, total_auditor_nodes: %u\n",
        cur_time,
        activation_time,
        static_cast<uint64_t>(issuance / REWARD_PRECISION),
        static_cast<uint32_t>(issuance % REWARD_PRECISION),
        static_cast<uint64_t>(edge_total_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(edge_total_rewards % REWARD_PRECISION),
        edge_num,
        static_cast<uint64_t>(archive_total_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(archive_total_rewards % REWARD_PRECISION),
        archive_num,
        static_cast<uint64_t>(auditor_total_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(auditor_total_rewards % REWARD_PRECISION),
        static_cast<uint64_t>(validator_total_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(validator_total_rewards % REWARD_PRECISION),
        static_cast<uint64_t>(total_vote_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(total_vote_rewards % REWARD_PRECISION),
        static_cast<uint64_t>(governance_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(governance_rewards % REWARD_PRECISION),
        all_tickets,
        total_auditor_nodes);
    for (auto const & entity : map_nodes) {
        auto const & account = entity.first;
        auto const & node = entity.second;
        top::xstake::uint128_t node_reward = 0;

        if (edge_num > 0 && node.is_edge_node() && node.get_deposit() > 0) {
            //add_node_reward(account, xreward_type::edge_reward, edge_total_rewards / edge_num);
            auto edge_reward = edge_total_rewards / edge_num;
            xdbg("[xzec_reward_contract::calc_nodes_rewards_v3] account: %s, edge reward: [%llu, %u]",
                account.c_str(),
                static_cast<uint64_t>(edge_reward / xstake::REWARD_PRECISION),
                static_cast<uint32_t>(edge_reward % xstake::REWARD_PRECISION));
            node_reward += edge_reward;
        }
        if (archive_num > 0 && node.is_valid_archive_node() && node.get_deposit() > 0) {
            //add_node_reward(account, xreward_type::archive_reward, archive_total_rewards / archive_num);
            auto archive_reward = archive_total_rewards / archive_num;
            xdbg("[xzec_reward_contract::calc_nodes_rewards_v3] account: %s, archive reward: [%llu, %u]",
                account.c_str(),
                static_cast<uint64_t>(archive_reward / xstake::REWARD_PRECISION),
                static_cast<uint32_t>(archive_reward % xstake::REWARD_PRECISION));
            node_reward += archive_reward;
        }
        if (node.is_validator_node() && node.get_deposit() > 0) {
            add_workload_reward(false, node.m_account, validator_group_rewards, validator_clusters_workloads, seed_node_rewards, node_reward);
        }
        auto adv_total_votes = node.m_vote_amount;
        if (node.is_valid_auditor_node() && node.get_deposit() > 0) {
            add_workload_reward(true, node.m_account, auditor_group_rewards, auditor_clusters_workloads, seed_node_rewards, node_reward);
            // vote reward
            xassert(all_tickets > 0);
            auto node_vote_reward = adv_total_votes * total_vote_rewards / all_tickets;
            xdbg("[xzec_reward_contract::calc_nodes_rewards_v3] account: %s, node_vote_reward: [%llu, %u], node deposit: %llu, all_tickets: %llu, adv_total_votes: %llu",
                    account.c_str(),
                    static_cast<uint64_t>(node_vote_reward / REWARD_PRECISION),
                    static_cast<uint32_t>(node_vote_reward % REWARD_PRECISION),
                    node.get_deposit(),
                    all_tickets,
                    adv_total_votes);
            node_reward += node_vote_reward;
        }
        // vote dividend
        if (adv_total_votes > 0 && node.m_support_ratio_numerator > 0) {
            auto adv_reward_to_self = node_reward * (node.m_support_ratio_denominator - node.m_support_ratio_numerator) / node.m_support_ratio_denominator;
            auto adv_reward_to_voters = node_reward - adv_reward_to_self;
            add_table_vote_reward(node.m_account, adv_total_votes, adv_reward_to_voters, contract_auditor_votes);
            node_reward = adv_reward_to_self;
        }
        add_table_node_reward(node.m_account, node_reward);
    }
    if (edge_num == 0) seed_node_rewards += edge_total_rewards;
    if (archive_num == 0) seed_node_rewards += archive_total_rewards;
    if (total_auditor_nodes == 0) seed_node_rewards += total_vote_rewards;
    zero_workload_reward(true, validator_total_rewards, validator_clusters_workloads, zero_workload_rewards);
    zero_workload_reward(false, auditor_total_rewards, auditor_clusters_workloads, zero_workload_rewards);

    // clear accumulated workloads
    // CLEAR(enum_type_t::map, XPORPERTY_CONTRACT_WORKLOAD_KEY);
    // CLEAR(enum_type_t::map, XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY);
    // CALL(common::xaccount_address_t{sys_contract_zec_workload_addr}, "clear_workload", std::string(""));
    // uint32_t task_id = get_task_id();
    // add_task(task_id, onchain_timer_round, sys_contract_zec_workload_addr, XZEC_WORKLOAD_CLEAR_WORKLOAD_ACTION, std::string(""));
    // task_id++;

    // governance rewards
    // request additional issuance
    uint64_t common_funds = static_cast<uint64_t>( (governance_rewards + zero_workload_rewards + seed_node_rewards) / REWARD_PRECISION );
    if ( common_funds > 0 ) {
        uint32_t task_id = get_task_id();
        std::map<std::string, uint64_t> issuances;
        issuances.emplace(sys_contract_rec_tcc_addr, common_funds);
        base::xstream_t seo_stream(base::xcontext_t::instance());
        seo_stream << issuances;
        add_task(task_id, onchain_timer_round, "", XTRANSFER_ACTION, std::string((char *)seo_stream.data(), seo_stream.size()));
        task_id++;
        update_accumulated_issuance(common_funds, onchain_timer_round);
        XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "calc_nodes_rewards_Executed", 1);
    }
}

void xzec_reward_contract::calc_nodes_rewards_v4(std::map<std::string, std::map<std::string, top::xstake::uint128_t >> & table_nodes_rewards,
                                              std::map<std::string, top::xstake::uint128_t> & contract_rewards,
                                              std::map<std::string, std::map<std::string, top::xstake::uint128_t >> & contract_auditor_vote_rewards,
                                              const uint64_t onchain_timer_round) {
    XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "calc_nodes_rewards_Called", 1);
    XMETRICS_TIME_RECORD(XREWARD_CONTRACT "calc_nodes_rewards_ExecutionTime");

    auto add_table_node_reward = [&](common::xaccount_address_t const & account, top::xstake::uint128_t node_reward) {
        if (node_reward == 0)
            return;

        uint32_t table_id = 0;
        if (!EXTRACT_TABLE_ID(account, table_id)) {
            xwarn("[xzec_reward_contract::calc_nodes_rewards_v4][xzec_reward_contract::add_table_node_reward] node reward pid: %d, account: %s, node_reward: [%llu, %u]\n",
                getpid(), account.c_str(), static_cast<uint64_t>(node_reward / REWARD_PRECISION), static_cast<uint32_t>(node_reward % REWARD_PRECISION));
            return;
        }

        auto const & reward_contract = CALC_CONTRACT_ADDRESS(sys_contract_sharding_reward_claiming_addr, table_id);
        xdbg("[xzec_reward_contract::calc_nodes_rewards_v4][xzec_reward_contract::add_table_node_reward] node reward, pid:%d, reward_contract: %s, account: %s, reward: [%llu, %u]\n",
             getpid(),
             reward_contract.c_str(),
             account.c_str(),
             static_cast<uint64_t>(node_reward / REWARD_PRECISION), static_cast<uint32_t>(node_reward % REWARD_PRECISION));

        contract_rewards[reward_contract] += node_reward;
        table_nodes_rewards[reward_contract][account.value()] = node_reward;
    };

    auto get_adv_total_votes = [&](std::map<std::string, std::map<std::string, std::string>> const & contract_auditor_votes, common::xaccount_address_t const & account) {
        uint64_t adv_total_votes = 0;

        for (auto const & contract_auditor_vote : contract_auditor_votes) {
            auto const & contract = contract_auditor_vote.first;
            auto const & auditor_votes = contract_auditor_vote.second;

            auto iter = auditor_votes.find(account.value());
            if (iter != auditor_votes.end()) {
                adv_total_votes += base::xstring_utl::touint64(iter->second);
            }
        }

        return adv_total_votes;
    };

    auto add_table_vote_reward = [&](common::xaccount_address_t const & account,
                                   uint64_t adv_total_votes,
                                   top::xstake::uint128_t const & adv_reward_to_voters,
                                   std::map<std::string, std::map<std::string, std::string>> const & contract_auditor_votes) {
        if (adv_total_votes == 0)
            return;

        for (auto & contract_auditor_vote : contract_auditor_votes) {
            auto const & contract = contract_auditor_vote.first;
            auto const & auditor_votes = contract_auditor_vote.second;

            uint32_t table_id = 0;
            if (!xdatautil::extract_table_id_from_address(contract, table_id)) {
                xwarn("[xzec_reward_contract::calc_nodes_rewards_v4][xzec_reward_contract::add_table_vote_reward] extract_table_id_from_address %s  failed!\n", contract.c_str());
                continue;
            }
            auto const & reward_contract = CALC_CONTRACT_ADDRESS(sys_contract_sharding_reward_claiming_addr, table_id);
            auto iter = auditor_votes.find(account.value());
            if (iter != auditor_votes.end()) {
                auto adv_reward_to_contract = adv_reward_to_voters * base::xstring_utl::touint64(iter->second) / adv_total_votes;
                xdbg("[xzec_reward_contract::calc_nodes_rewards_v4][add_table_vote_reward] account: %s, contract: %s, table votes: %llu, adv_total_votes: %llu, adv_reward_to_voters: [%llu, %u], adv_reward_to_contract: [%llu, %u]\n",
                    account.c_str(),
                    contract.c_str(),
                    base::xstring_utl::touint64(iter->second),
                    adv_total_votes,
                    static_cast<uint64_t>(adv_reward_to_voters / REWARD_PRECISION),
                    static_cast<uint32_t>(adv_reward_to_voters % REWARD_PRECISION),
                    static_cast<uint64_t>(adv_reward_to_contract / REWARD_PRECISION),
                    static_cast<uint32_t>(adv_reward_to_contract % REWARD_PRECISION));
                if (adv_reward_to_contract > 0) {
                    contract_rewards[reward_contract] += adv_reward_to_contract;
                    contract_auditor_vote_rewards[reward_contract][account.value()] += adv_reward_to_contract;
                }
            }
        }
    };

    auto add_workload_reward = [&](bool is_auditor, common::xaccount_address_t const & account, top::xstake::uint128_t const & cluster_total_rewards, std::map<std::string, std::string> const & clusters_workloads, top::xstake::uint128_t & seed_node_rewards, top::xstake::uint128_t & workload_reward) {
        uint32_t zero_workload_val = 0;
        if (is_auditor) {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(cluster_zero_workload);
        } else {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(shard_zero_workload);
        }
        xdbg("[xzec_reward_contract::calc_nodes_rewards_v4][add_workload_reward] account: %s, is_auditor: %d, %d clusters report workloads, cluster_total_rewards: [%llu, %u], zero_workload_val: %u\n",
             account.c_str(),
             is_auditor,
             clusters_workloads.size(),
             static_cast<uint64_t>(cluster_total_rewards / REWARD_PRECISION),
             static_cast<uint32_t>(cluster_total_rewards % REWARD_PRECISION),
             zero_workload_val);

        for (auto & cluster_workloads : clusters_workloads) {
            auto const & key_str = cluster_workloads.first;
            common::xcluster_address_t cluster;
            xstream_t key_stream(xcontext_t::instance(), (uint8_t *)key_str.data(), key_str.size());
            key_stream >> cluster;
            auto const & value_str = cluster_workloads.second;
            xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
            cluster_workload_t workload;
            workload.serialize_from(stream);
            xdbg("[xzec_reward_contract::calc_nodes_rewards_v4][add_workload_reward] account: %s, cluster id: %s, cluster size: %d, cluster_total_workload: %u\n",
                 account.c_str(),
                 cluster.to_string().c_str(),
                 workload.m_leader_count.size(),
                 workload.cluster_total_workload);
            if (workload.cluster_total_workload <= zero_workload_val)
                continue;
            auto it = workload.m_leader_count.find(account.value());
            if (it != workload.m_leader_count.end()) {
                auto const & work = it->second;
                workload_reward += cluster_total_rewards * work / workload.cluster_total_workload;

                xdbg("[xzec_reward_contract::calc_nodes_rewards_v4][add_workload_reward] account: %s, cluster_id: %s, work: %d, total_workload: %d, cluster_total_rewards: [%llu, %u], reward: [%llu, %u]\n",
                     account.c_str(),
                     cluster.to_string().c_str(),
                     work,
                     workload.cluster_total_workload,
                     static_cast<uint64_t>(cluster_total_rewards / xstake::REWARD_PRECISION),
                     static_cast<uint32_t>(cluster_total_rewards % xstake::REWARD_PRECISION),
                     static_cast<uint64_t>(workload_reward / xstake::REWARD_PRECISION),
                     static_cast<uint32_t>(workload_reward % xstake::REWARD_PRECISION));
            }
        }
    };

    auto zero_workload_reward = [&](bool validator, top::xstake::uint128_t const & workload_total_reward, const std::map<std::string, std::string> & clusters_workloads, top::xstake::uint128_t & zero_workload_rewards) {
        std::size_t cluster_size;
        uint8_t group_id_begin;
        bool zero_workload = false;
        uint32_t zero_workload_val = 0;
        if (validator) {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(shard_zero_workload);
            cluster_size = XGET_ONCHAIN_GOVERNANCE_PARAMETER(validator_group_count);
            group_id_begin = common::xvalidator_group_id_begin.value();
        } else {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(cluster_zero_workload);
            cluster_size = XGET_ONCHAIN_GOVERNANCE_PARAMETER(auditor_group_count);
            group_id_begin = common::xauditor_group_id_begin.value();
        }
        if (cluster_size == 0) {
            xwarn("[xzec_reward_contract::calc_nodes_rewards_v4][zero_workload_reward] validator_workload: %d, cluster_size zero", validator);
            return;
        }
        top::xstake::uint128_t cluster_total_rewards = workload_total_reward / cluster_size;  // averaged by all clusters
        for (auto group_id = group_id_begin; group_id < group_id_begin + cluster_size; group_id++) {
            zero_workload = true;
            for (auto & cluster_workloads : clusters_workloads) {
                auto const & key_str = cluster_workloads.first;
                xstream_t stream(xcontext_t::instance(), (uint8_t *)key_str.data(), key_str.size());
                common::xcluster_address_t cluster;
                stream >> cluster;
                if (group_id == cluster.group_id().value()) {
                    auto const & value_str = cluster_workloads.second;
                    xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
                    cluster_workload_t workload;
                    workload.serialize_from(stream);
                    if (workload.cluster_total_workload > zero_workload_val) {
                        zero_workload = false;
                    }
                    xdbg("[xzec_reward_contract::calc_nodes_rewards_v4][zero_workload_reward] group %u has workload %u, zero_workload: %d", group_id, workload.cluster_total_workload, zero_workload);
                    break;
                }
            }
            if (zero_workload) {
                zero_workload_rewards += cluster_total_rewards;
            }
            xdbg("[xzec_reward_contract::calc_nodes_rewards_v4][zero_workload_reward] validator: %d, cluster_size: %u, group %u, zero_workload: %d, cluster_total_rewards: [%llu, %u], zero_workload_rewards: [%llu, %u]",
                 validator,
                 cluster_size,
                 group_id,
                 zero_workload,
                 static_cast<uint64_t>(cluster_total_rewards / xstake::REWARD_PRECISION),
                 static_cast<uint32_t>(cluster_total_rewards % xstake::REWARD_PRECISION),
                 static_cast<uint64_t>(zero_workload_rewards / xstake::REWARD_PRECISION),
                 static_cast<uint32_t>(zero_workload_rewards % xstake::REWARD_PRECISION));
        }
    };

    // preprocess workload
    auto preprocess_workload = [&](bool is_auditor, std::map<std::string, std::string> & clusters_workloads, std::map<std::string, xreg_node_info> const & map_nodes) {
        uint32_t zero_workload_val = 0;
        if (is_auditor) {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(cluster_zero_workload);
        } else {
            zero_workload_val = XGET_ONCHAIN_GOVERNANCE_PARAMETER(shard_zero_workload);
        }

        xdbg("[xzec_reward_contract::calc_nodes_rewards_v4][preprocess_workload] is_auditor: %u, total group num: %d\n",
            is_auditor,
            clusters_workloads.size());
        for (auto it = clusters_workloads.begin(); it != clusters_workloads.end(); ) {
            auto const & key_str = it->first;
            common::xcluster_address_t cluster;
            xstream_t key_stream(xcontext_t::instance(), (uint8_t *)key_str.data(), key_str.size());
            key_stream >> cluster;
            auto const & value_str = it->second;
            xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
            cluster_workload_t workload;
            bool workload_changed = false;
            workload.serialize_from(stream);
            xdbg("[xzec_reward_contract::calc_nodes_rewards_v4][preprocess_workload] is_auditor: %u, auditor cluster id: %s, cluster size: %d, cluster_total_workload: %u\n",
                 is_auditor,
                 cluster.to_string().c_str(),
                 workload.m_leader_count.size(),
                 workload.cluster_total_workload);
            if (workload.cluster_total_workload <= zero_workload_val) {
                xinfo("[xzec_reward_contract::calc_nodes_rewards_v4][preprocess_workload] is_auditor: %u, cluster id: %s, cluster size: %d, cluster_total_workload: %u, cluster workloads are <= zero_workload_val and will be ignored\n",
                    is_auditor,
                    cluster.to_string().c_str(),
                    workload.m_leader_count.size(),
                    workload.cluster_total_workload);
                clusters_workloads.erase(it++);
                continue;
            }

            for (auto it2 = workload.m_leader_count.begin(); it2 != workload.m_leader_count.end(); ) {
                xreg_node_info node;
                if (get_node_info(map_nodes, it2->first, node) != 0) {
                    xinfo("[xzec_reward_contract::calc_nodes_rewards_v4][preprocess_workload] account: %s not in map nodes", it2->first.c_str());
                    workload.cluster_total_workload -= it2->second;
                    workload.m_leader_count.erase(it2++);
                    workload_changed = true;
                    continue;
                }

                if (is_auditor) {
                    if (node.get_deposit() == 0 || !node.is_valid_auditor_node()) {
                        xinfo("[xzec_reward_contract::calc_nodes_rewards_v4][preprocess_workload] account: %s is not a valid auditor, deposit: %llu, votes: %llu",
                            it2->first.c_str(), node.get_deposit(), node.m_vote_amount);
                        workload.cluster_total_workload -= it2->second;
                        workload.m_leader_count.erase(it2++);
                        workload_changed = true;
                    } else {
                        it2++;
                    }
                } else {
                    if (node.get_deposit() == 0 || !node.is_validator_node()) {
                        xinfo("[xzec_reward_contract::calc_nodes_rewards_v4][preprocess_workload] account: %s is not a valid validator, deposit: %llu",
                            it2->first.c_str(), node.get_deposit());
                        workload.cluster_total_workload -= it2->second;
                        workload.m_leader_count.erase(it2++);
                        workload_changed = true;
                    } else {
                        it2++;
                    }
                }
            } // end of group
            if (workload.m_leader_count.size() == 0) {
                clusters_workloads.erase(it++);
            } else {
                if (workload_changed) {
                    xstream_t stream(xcontext_t::instance());
                    workload.serialize_to(stream);
                    it->second = std::string((const char*)stream.data(), stream.size());
                }
                it++;
            }
        }
    };

    std::map<std::string, std::string> auditor_clusters_workloads;
    std::map<std::string, std::string> validator_clusters_workloads;
    //base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)workload_str.data(), workload_str.size());
    //MAP_DESERIALIZE_SIMPLE(stream, auditor_clusters_workloads);
    //MAP_DESERIALIZE_SIMPLE(stream, validator_clusters_workloads);

    // auditor workload, property not created in setup
    try {
        MAP_COPY_GET(XPORPERTY_CONTRACT_WORKLOAD_KEY, auditor_clusters_workloads);
    } catch (std::runtime_error & e) {
        xdbg("[xzec_reward_contract::calc_nodes_rewards_v4] MAP COPY GET XPORPERTY_CONTRACT_WORKLOAD_KEY error: %s", e.what());
    }

    // validator workload, property not created in setup
    try {
        MAP_COPY_GET(XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY, validator_clusters_workloads);
    } catch (std::runtime_error & e) {
        xdbg("[xzec_reward_contract::calc_nodes_rewards_v4] MAP COPY GET XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY error: %s", e.what());
    }

    try {
        clear_workload();
    } catch (std::runtime_error & e) {
        xdbg("[xzec_reward_contract::calc_nodes_rewards_v4] clear_workload error: %s", e.what());
    }

    // contract auditor votes
    std::map<std::string, std::string> contract_auditor_votes2;
    MAP_COPY_GET(XPORPERTY_CONTRACT_TICKETS_KEY, contract_auditor_votes2, sys_contract_zec_vote_addr);

    xdbg("[xzec_reward_contract::calc_nodes_rewards_v4] contract_auditor_votes2 size: %d", contract_auditor_votes2.size());
    // transform to map of map struct
    std::map<std::string, std::map<std::string, std::string>> contract_auditor_votes;
    for (auto & contract_auditor_vote : contract_auditor_votes2) {
        auto const & contract = contract_auditor_vote.first;
        auto const & auditor_votes_str = contract_auditor_vote.second;

        std::map<std::string, std::string> auditor_votes;
        xstream_t stream(xcontext_t::instance(), (uint8_t *)auditor_votes_str.data(), auditor_votes_str.size());
        stream >> auditor_votes;
        contract_auditor_votes[contract] = auditor_votes;
    }

    uint64_t cur_time = onchain_timer_round;
    uint64_t activation_time = get_activated_time();
    int64_t total_height = cur_time - activation_time;
    uint32_t edge_num = 0;
    uint32_t archive_num = 0;
    uint32_t total_auditor_nodes = 0;
    auto issuance = calc_issuance(total_height);
    auto auditor_total_rewards      = get_reward(issuance, xreward_type::auditor_reward);
    auto validator_total_rewards    = get_reward(issuance, xreward_type::validator_reward);
    auto edge_total_rewards         = get_reward(issuance, xreward_type::edge_reward);
    auto archive_total_rewards      = get_reward(issuance, xreward_type::archive_reward);
    auto total_vote_rewards         = get_reward(issuance, xreward_type::vote_reward);
    auto governance_rewards         = get_reward(issuance, xreward_type::governance_reward);
    std::size_t auditor_group_count = XGET_ONCHAIN_GOVERNANCE_PARAMETER(auditor_group_count);
    XCONTRACT_ENSURE(auditor_group_count > 0, "auditor group count equals zero");
    top::xstake::uint128_t auditor_group_rewards = auditor_total_rewards / auditor_group_count;
    std::size_t validator_group_count = XGET_ONCHAIN_GOVERNANCE_PARAMETER(validator_group_count);
    XCONTRACT_ENSURE(validator_group_count > 0, "validator group count equals zero");
    top::xstake::uint128_t validator_group_rewards = validator_total_rewards / validator_group_count;
    top::xstake::uint128_t zero_workload_rewards = 0;
    top::xstake::uint128_t seed_node_rewards = 0;

    // transform map_nodes
    std::map<std::string, xreg_node_info> map_nodes;
    {
        std::map<std::string, std::string> map_nodes2;
        auto const last_read_height = static_cast<std::uint64_t>(std::stoull(STRING_GET(XPROPERTY_LAST_READ_REC_REG_CONTRACT_BLOCK_HEIGHT)));
        GET_MAP_PROPERTY(XPORPERTY_CONTRACT_REG_KEY, map_nodes2, last_read_height, sys_contract_rec_registration_addr);
        //MAP_COPY_GET(XPORPERTY_CONTRACT_REG_KEY, map_nodes2, sys_contract_rec_registration_addr);
        xdbg("[xzec_reward_contract::calc_nodes_rewards_v4] last_read_height: %llu, map_nodes2 size: %d",
            last_read_height, map_nodes2.size());

        for (auto const & entity : map_nodes2) {
            auto const & account = entity.first;
            auto const & value_str = entity.second;
            xreg_node_info node;
            xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
            node.serialize_from(stream);
            node.m_vote_amount = get_adv_total_votes(contract_auditor_votes, node.m_account);
            map_nodes[account] = node;
            xdbg("[xzec_reward_contract::calc_nodes_rewards_v4] map_nodes: account: %s, deposit: %llu, node_type: %s, votes: %llu",
                node.m_account.c_str(),
                node.get_deposit(),
                node.m_genesis_node ? "advance,validator,edge" : common::to_string(node.m_registered_role).c_str(),
                node.m_vote_amount);
            if (node.get_deposit() > 0 && node.is_edge_node()) {
                edge_num++;
            }
            if (node.get_deposit() > 0 && node.is_valid_archive_node()) {
                archive_num++;
            }
            if (node.get_deposit() > 0 && node.is_valid_auditor_node()) {
                total_auditor_nodes++;
            }
        }
    }

    // preprocess workload
    preprocess_workload(false, validator_clusters_workloads, map_nodes);
    preprocess_workload(true, auditor_clusters_workloads, map_nodes);

    // count all votes
    uint64_t all_tickets = 0;
    for (auto const & entity : contract_auditor_votes) {
        auto const & auditor_votes = entity.second;

        for (auto const & entity2 : auditor_votes) {
            xreg_node_info node;
            if (get_node_info(map_nodes, entity2.first, node) != 0)  {
                xwarn("[xzec_reward_contract::calc_nodes_rewards_v4] account %s not in map_nodes", entity2.first.c_str());
                continue;
            }

            if (node.get_deposit() > 0 && node.is_valid_auditor_node()) {
                all_tickets += base::xstring_utl::touint64(entity2.second);
            }
        }
    }
    if (total_auditor_nodes > 0) {
        xassert(all_tickets > 0);
    }

    xinfo(
        "[xzec_reward_contract::calc_nodes_rewards_v4] cur_time: %llu, activation_time: %llu, "
        "issuance: [%llu, %u], "
        "edge total rewards: [%llu, %u], edge num: %d, "
        "archive_total_rewards: [%llu, %u], archive num: %d, "
        "auditor_total_rewards: [%llu, %u], validator_total_rewards: [%llu, %u], "
        "total_vote_rewards: [%llu, %u], governance_rewards: [%llu, %u], all_tickets: %llu, total_auditor_nodes: %u\n",
        cur_time,
        activation_time,
        static_cast<uint64_t>(issuance / REWARD_PRECISION),
        static_cast<uint32_t>(issuance % REWARD_PRECISION),
        static_cast<uint64_t>(edge_total_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(edge_total_rewards % REWARD_PRECISION),
        edge_num,
        static_cast<uint64_t>(archive_total_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(archive_total_rewards % REWARD_PRECISION),
        archive_num,
        static_cast<uint64_t>(auditor_total_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(auditor_total_rewards % REWARD_PRECISION),
        static_cast<uint64_t>(validator_total_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(validator_total_rewards % REWARD_PRECISION),
        static_cast<uint64_t>(total_vote_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(total_vote_rewards % REWARD_PRECISION),
        static_cast<uint64_t>(governance_rewards / REWARD_PRECISION),
        static_cast<uint32_t>(governance_rewards % REWARD_PRECISION),
        all_tickets,
        total_auditor_nodes);
    xissue_detail issue_detail;
    issue_detail.onchain_timer_round            = onchain_timer_round;
    issue_detail.m_zec_vote_contract_height     = get_blockchain_height(sys_contract_zec_vote_addr);
    issue_detail.m_zec_workload_contract_height = get_blockchain_height(sys_contract_zec_workload_addr);
    issue_detail.m_zec_reward_contract_height   = get_blockchain_height(sys_contract_zec_reward_addr);
    issue_detail.m_edge_reward_ratio            = XGET_ONCHAIN_GOVERNANCE_PARAMETER(edge_reward_ratio);
    issue_detail.m_archive_reward_ratio         = XGET_ONCHAIN_GOVERNANCE_PARAMETER(archive_reward_ratio);
    issue_detail.m_validator_reward_ratio       = XGET_ONCHAIN_GOVERNANCE_PARAMETER(validator_reward_ratio);
    issue_detail.m_auditor_reward_ratio         = XGET_ONCHAIN_GOVERNANCE_PARAMETER(auditor_reward_ratio);
    issue_detail.m_vote_reward_ratio            = XGET_ONCHAIN_GOVERNANCE_PARAMETER(vote_reward_ratio);
    issue_detail.m_governance_reward_ratio      = XGET_ONCHAIN_GOVERNANCE_PARAMETER(governance_reward_ratio);
    issue_detail.m_auditor_group_count          = auditor_group_count;
    issue_detail.m_validator_group_count        = validator_group_count;

    bool add_issue_self_reward = false;
    auto fork_config = chain_upgrade::xtop_chain_fork_config_center::chain_fork_config();
    if (chain_upgrade::xtop_chain_fork_config_center::is_forked(fork_config.slash_workload_contract_upgrade, onchain_timer_round)) {
        add_issue_self_reward = true;
    }

    for (auto const & entity : map_nodes) {
        auto const & account = entity.first;
        auto const & node = entity.second;
        top::xstake::uint128_t node_reward = 0;

        if (edge_num > 0 && node.is_edge_node() && node.get_deposit() > 0) {
            //add_node_reward(account, xreward_type::edge_reward, edge_total_rewards / edge_num);
            auto edge_reward = edge_total_rewards / edge_num;
            xdbg("[xzec_reward_contract::calc_nodes_rewards_v4] account: %s, edge reward: [%llu, %u]",
                account.c_str(),
                static_cast<uint64_t>(edge_reward / xstake::REWARD_PRECISION),
                static_cast<uint32_t>(edge_reward % xstake::REWARD_PRECISION));
            node_reward += edge_reward;
            issue_detail.m_node_rewards[account].m_edge_reward = edge_reward;
        }
        if (archive_num > 0 && node.is_valid_archive_node() && node.get_deposit() > 0) {
            //add_node_reward(account, xreward_type::archive_reward, archive_total_rewards / archive_num);
            auto archive_reward = archive_total_rewards / archive_num;
            xdbg("[xzec_reward_contract::calc_nodes_rewards_v4] account: %s, archive reward: [%llu, %u]",
                account.c_str(),
                static_cast<uint64_t>(archive_reward / xstake::REWARD_PRECISION),
                static_cast<uint32_t>(archive_reward % xstake::REWARD_PRECISION));
            node_reward += archive_reward;
            issue_detail.m_node_rewards[account].m_archive_reward = archive_reward;
        }
        if (node.is_validator_node() && node.get_deposit() > 0) {
            top::xstake::uint128_t workload_reward = 0;
            add_workload_reward(false, node.m_account, validator_group_rewards, validator_clusters_workloads, seed_node_rewards, workload_reward);
            node_reward += workload_reward;
            issue_detail.m_node_rewards[account].m_validator_reward = workload_reward;
        }
        auto adv_total_votes = node.m_vote_amount;
        if (node.is_valid_auditor_node() && node.get_deposit() > 0) {
            top::xstake::uint128_t workload_reward = 0;
            add_workload_reward(true, node.m_account, auditor_group_rewards, auditor_clusters_workloads, seed_node_rewards, workload_reward);
            node_reward += workload_reward;
            issue_detail.m_node_rewards[account].m_auditor_reward = workload_reward;
            // vote reward
            xassert(all_tickets > 0);
            auto node_vote_reward = adv_total_votes * total_vote_rewards / all_tickets;
            xdbg("[xzec_reward_contract::calc_nodes_rewards_v4] account: %s, node_vote_reward: [%llu, %u], node deposit: %llu, all_tickets: %llu, adv_total_votes: %llu",
                    account.c_str(),
                    static_cast<uint64_t>(node_vote_reward / REWARD_PRECISION),
                    static_cast<uint32_t>(node_vote_reward % REWARD_PRECISION),
                    node.get_deposit(),
                    all_tickets,
                    adv_total_votes);
            node_reward += node_vote_reward;
            issue_detail.m_node_rewards[account].m_vote_reward = node_vote_reward;
        }
        // vote dividend
        if (adv_total_votes > 0 && node.m_support_ratio_numerator > 0) {
            auto adv_reward_to_self = node_reward * (node.m_support_ratio_denominator - node.m_support_ratio_numerator) / node.m_support_ratio_denominator;
            auto adv_reward_to_voters = node_reward - adv_reward_to_self;
            add_table_vote_reward(node.m_account, adv_total_votes, adv_reward_to_voters, contract_auditor_votes);
            node_reward = adv_reward_to_self;
        }
        if(add_issue_self_reward) {
            issue_detail.m_node_rewards[account].m_self_reward = node_reward;
        }
        add_table_node_reward(node.m_account, node_reward);
    }
    update_issuance_detail(issue_detail);
    if (edge_num == 0) seed_node_rewards += edge_total_rewards;
    if (archive_num == 0) seed_node_rewards += archive_total_rewards;
    if (total_auditor_nodes == 0) seed_node_rewards += total_vote_rewards;
    zero_workload_reward(true, validator_total_rewards, validator_clusters_workloads, zero_workload_rewards);
    zero_workload_reward(false, auditor_total_rewards, auditor_clusters_workloads, zero_workload_rewards);

    // clear accumulated workloads
    // CLEAR(enum_type_t::map, XPORPERTY_CONTRACT_WORKLOAD_KEY);
    // CLEAR(enum_type_t::map, XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY);
    // CALL(common::xaccount_address_t{sys_contract_zec_workload_addr}, "clear_workload", std::string(""));
    // uint32_t task_id = get_task_id();
    // add_task(task_id, onchain_timer_round, sys_contract_zec_workload_addr, XZEC_WORKLOAD_CLEAR_WORKLOAD_ACTION, std::string(""));
    // task_id++;

    // governance rewards
    // request additional issuance
    uint64_t common_funds = static_cast<uint64_t>( (governance_rewards + zero_workload_rewards + seed_node_rewards) / REWARD_PRECISION );
    if ( common_funds > 0 ) {
        uint32_t task_id = get_task_id();
        std::map<std::string, uint64_t> issuances;
        issuances.emplace(sys_contract_rec_tcc_addr, common_funds);
        base::xstream_t seo_stream(base::xcontext_t::instance());
        seo_stream << issuances;
        add_task(task_id, onchain_timer_round, "", XTRANSFER_ACTION, std::string((char *)seo_stream.data(), seo_stream.size()));
        task_id++;
        update_accumulated_issuance(common_funds, onchain_timer_round);
        XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "calc_nodes_rewards_Executed", 1);
    }
}

void xzec_reward_contract::dispatch_all_reward(std::map<std::string, std::map<std::string, top::xstake::uint128_t> > const & table_nodes_rewards,
                                               std::map<std::string, top::xstake::uint128_t> const & contract_rewards,
                                               std::map<std::string, std::map<std::string, top::xstake::uint128_t> > const & contract_auditor_vote_rewards,
                                               uint64_t onchain_timer_round) {
    XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "dispatch_all_reward_Called", 1);
    XMETRICS_TIME_RECORD(XREWARD_CONTRACT "dispatch_all_reward_ExecutionTime");
    xdbg("[xzec_reward_contract::dispatch_all_reward] pid:%d\n", getpid());

    // request additional issuance

    /*{
        //test for transaction atomicity
        auto contract_total_rewards_size = contract_total_rewards.size();
        int iiiii = 0;
        for(auto const& entity : contract_total_rewards) {
            auto const& contract    = entity.first;
            auto  total_award = entity.second;

            iiiii++;

            if (iiiii == contract_total_rewards_size) {
                total_award = 0;
                xdbg("[xbeacon_audit_workload_contract11111] contract: %s, total_award: %llu\n", contract.c_str(), total_award);

            }

            issuances.emplace(contract, total_award);
        }
    }*/

    uint64_t issuance = 0;
    uint32_t task_id = get_task_id();
    for (auto const & entity : contract_rewards) {
        auto const & contract = entity.first;
        auto const & total_award = entity.second;

        uint64_t reward = static_cast<uint64_t>(total_award / xstake::REWARD_PRECISION);
        if (total_award % xstake::REWARD_PRECISION != 0) {
            reward += 1;
        }
        issuance += reward;
        std::map<std::string, uint64_t> issuances;
        issuances.emplace(contract, reward);
        base::xstream_t seo_stream(base::xcontext_t::instance());
        seo_stream << issuances;

        add_task(task_id, onchain_timer_round, "", XTRANSFER_ACTION, std::string((char *)seo_stream.data(), seo_stream.size()));
        task_id++;
    }
    update_accumulated_issuance(issuance, onchain_timer_round);
    // request_issuance(issuances);

    // generate tasks
    xdbg("[xzec_reward_contract::dispatch_all_reward] pid: %d, table_nodes_rewards size: %d\n", getpid(), table_nodes_rewards.size());

    for (auto & entity : table_nodes_rewards) {
        auto const & contract = entity.first;
        auto const & account_awards = entity.second;

        base::xstream_t reward_stream(base::xcontext_t::instance());
        reward_stream << onchain_timer_round;
        reward_stream << account_awards;
        add_task(task_id, onchain_timer_round, contract, XREWARD_CLAIMING_ADD_NODE_REWARD, std::string((char *)reward_stream.data(), reward_stream.size()));
        task_id++;
    }

    xdbg("[xzec_reward_contract::dispatch_all_reward] pid: %d, contract_auditor_vote_rewards size: %d\n", getpid(), contract_auditor_vote_rewards.size());
    for (auto const & entity : contract_auditor_vote_rewards) {
        auto const & contract = entity.first;
        auto const & auditor_vote_rewards = entity.second;

        xdbg("[xzec_reward_contract::dispatch_all_reward] pid: %d, contract: %s, auditor_vote_rewards size: %d\n", getpid(), contract.c_str(), auditor_vote_rewards.size());

        base::xstream_t reward_stream(base::xcontext_t::instance());
        reward_stream << onchain_timer_round;
        reward_stream << auditor_vote_rewards;
        add_task(task_id, onchain_timer_round, contract, XREWARD_CLAIMING_ADD_VOTER_DIVIDEND_REWARD, std::string((char *)reward_stream.data(), reward_stream.size()));
        task_id++;
    }

    print_tasks();

    XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "dispatch_all_reward_Executed", 1);

    return;
}

void xzec_reward_contract::dispatch_all_reward_v2(std::map<std::string, std::map<std::string, top::xstake::uint128_t> > const & table_nodes_rewards,
                                               std::map<std::string, top::xstake::uint128_t> const & contract_rewards,
                                               std::map<std::string, std::map<std::string, top::xstake::uint128_t> > const & contract_auditor_vote_rewards,
                                               uint64_t onchain_timer_round) {
    XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "dispatch_all_reward_v2_Called", 1);
    XMETRICS_TIME_RECORD(XREWARD_CONTRACT "dispatch_all_reward_v2");
    xdbg("[xzec_reward_contract::dispatch_all_reward_v2] pid:%d\n", getpid());

    // request additional issuance

    /*{
        //test for transaction atomicity
        auto contract_total_rewards_size = contract_total_rewards.size();
        int iiiii = 0;
        for(auto const& entity : contract_total_rewards) {
            auto const& contract    = entity.first;
            auto  total_award = entity.second;

            iiiii++;

            if (iiiii == contract_total_rewards_size) {
                total_award = 0;
                xdbg("[xbeacon_audit_workload_contract11111] contract: %s, total_award: %llu\n", contract.c_str(), total_award);

            }

            issuances.emplace(contract, total_award);
        }
    }*/

    uint64_t issuance = 0;
    uint32_t task_id = get_task_id();
    for (auto const & entity : contract_rewards) {
        auto const & contract = entity.first;
        auto const & total_award = entity.second;

        uint64_t reward = static_cast<uint64_t>(total_award / xstake::REWARD_PRECISION);
        if (total_award % xstake::REWARD_PRECISION != 0) {
            reward += 1;
        }
        issuance += reward;
        std::map<std::string, uint64_t> issuances;
        issuances.emplace(contract, reward);
        base::xstream_t seo_stream(base::xcontext_t::instance());
        seo_stream << issuances;

        add_task(task_id, onchain_timer_round, "", XTRANSFER_ACTION, std::string((char *)seo_stream.data(), seo_stream.size()));
        task_id++;
    }
    update_accumulated_issuance(issuance, onchain_timer_round);
    // request_issuance(issuances);

    // generate tasks
    xdbg("[xzec_reward_contract::dispatch_all_reward_v2] pid: %d, table_nodes_rewards size: %d\n", getpid(), table_nodes_rewards.size());

    for (auto & entity : table_nodes_rewards) {
        auto const & contract = entity.first;
        auto const & account_awards = entity.second;

        int count = 0;
        std::map<std::string, top::xstake::uint128_t> account_awards2;
          for (auto it = account_awards.begin(); it != account_awards.end(); it++) {
            account_awards2[it->first] = it->second;
            if (++count % 1000 == 0) {
                base::xstream_t reward_stream(base::xcontext_t::instance());
                reward_stream << onchain_timer_round;
                reward_stream << account_awards2;
                add_task(task_id, onchain_timer_round, contract, XREWARD_CLAIMING_ADD_NODE_REWARD, std::string((char *)reward_stream.data(), reward_stream.size()));
                task_id++;

                account_awards2.clear();
            }
        }
        if (account_awards2.size() > 0) {
            base::xstream_t reward_stream(base::xcontext_t::instance());
            reward_stream << onchain_timer_round;
            reward_stream << account_awards2;
            add_task(task_id, onchain_timer_round, contract, XREWARD_CLAIMING_ADD_NODE_REWARD, std::string((char *)reward_stream.data(), reward_stream.size()));
            task_id++;
        }
    }

    xdbg("[xzec_reward_contract::dispatch_all_reward_v2] pid: %d, contract_auditor_vote_rewards size: %d\n", getpid(), contract_auditor_vote_rewards.size());
    for (auto const & entity : contract_auditor_vote_rewards) {
        auto const & contract = entity.first;
        auto const & auditor_vote_rewards = entity.second;

        xdbg("[xzec_reward_contract::dispatch_all_reward_v2] pid: %d, contract: %s, auditor_vote_rewards size: %d\n", getpid(), contract.c_str(), auditor_vote_rewards.size());

        int count = 0;
        std::map<std::string, top::xstake::uint128_t> auditor_vote_rewards2;
        for (auto it = auditor_vote_rewards.begin(); it != auditor_vote_rewards.end(); it++) {
            auditor_vote_rewards2[it->first] = it->second;
            if (++count % 1000 == 0) {
                base::xstream_t reward_stream(base::xcontext_t::instance());
                reward_stream << onchain_timer_round;
                reward_stream << auditor_vote_rewards2;
                add_task(task_id, onchain_timer_round, contract, XREWARD_CLAIMING_ADD_VOTER_DIVIDEND_REWARD, std::string((char *)reward_stream.data(), reward_stream.size()));
                task_id++;

                auditor_vote_rewards2.clear();
            }
        }
        if (auditor_vote_rewards2.size() > 0) {
            base::xstream_t reward_stream(base::xcontext_t::instance());
            reward_stream << onchain_timer_round;
            reward_stream << auditor_vote_rewards2;
            add_task(task_id, onchain_timer_round, contract, XREWARD_CLAIMING_ADD_VOTER_DIVIDEND_REWARD, std::string((char *)reward_stream.data(), reward_stream.size()));
            task_id++;
        }
    }

    print_tasks();

    XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "dispatch_all_reward_v2_Executed", 1);

    return;
}

bool xzec_reward_contract::reward_is_expire(const uint64_t onchain_timer_round) {
    uint64_t new_time_height = onchain_timer_round;

    auto get_activation_record = [&](xactivation_record & record) {
        std::string value_str;

        value_str = STRING_GET2(xstake::XPORPERTY_CONTRACT_GENESIS_STAGE_KEY, sys_contract_rec_registration_addr);
        if (!value_str.empty()) {
            base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.c_str(), (uint32_t)value_str.size());
            record.serialize_from(stream);
        }
        xdbg("[get_activation_record] activated: %d, pid:%d\n", record.activated, getpid());
        return record.activated;
    };

    xactivation_record record;
    if (!get_activation_record(record)) {
        xinfo("[xzec_reward_contract::reward_is_expire] mainnet not activated, onchain_timer_round: %llu", onchain_timer_round);
        return false;
    }

    xaccumulated_reward_record rew_record;
    get_accumulated_record(rew_record);// no need to check return value, rew_record has default value
    uint64_t old_time_height = record.activation_time + rew_record.last_issuance_time;
    xdbg("[xzec_reward_contract::reward_is_expire]  new_time_height %llu, old_time_height %llu\n", new_time_height, old_time_height);
    if (new_time_height <= old_time_height) {
        return false;
    }

    return true;
}

bool xzec_reward_contract::reward_is_expire_v2(const uint64_t onchain_timer_round) {
    uint64_t new_time_height = onchain_timer_round;

    auto get_activation_record = [&](xactivation_record & record) {
        std::string value_str;

        value_str = STRING_GET2(xstake::XPORPERTY_CONTRACT_GENESIS_STAGE_KEY, sys_contract_rec_registration_addr);
        if (!value_str.empty()) {
            base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.c_str(), (uint32_t)value_str.size());
            record.serialize_from(stream);
        }
        xdbg("[get_activation_record] activated: %d, pid:%d\n", record.activated, getpid());
        return record.activated;
    };

    xactivation_record record;
    if (!get_activation_record(record)) {
        xinfo("[xzec_reward_contract::reward_is_expire] mainnet not activated, onchain_timer_round: %llu", onchain_timer_round);
        return false;
    }

    xaccumulated_reward_record rew_record;
    get_accumulated_record(rew_record);// no need to check return value, rew_record has default value
    uint64_t old_time_height = record.activation_time + rew_record.last_issuance_time;
    auto reward_issue_interval = XGET_ONCHAIN_GOVERNANCE_PARAMETER(reward_issue_interval);
    xdbg("[xzec_reward_contract::reward_is_expire]  new_time_height %llu, old_time_height %llu, reward_issue_interval: %u\n",
        new_time_height, old_time_height, reward_issue_interval);
    if (new_time_height <= old_time_height || new_time_height - old_time_height < reward_issue_interval) {
        return false;
    }

    xinfo("[xzec_reward_contract::reward_is_expire] will reward, new_time_height %llu, old_time_height %llu, reward_issue_interval: %u\n",
        new_time_height, old_time_height, reward_issue_interval);
    return true;
}

uint32_t xzec_reward_contract::get_task_id() {
    std::map<std::string, std::string> dispatch_tasks;

    {
        XMETRICS_TIME_RECORD(XREWARD_CONTRACT "XPORPERTY_CONTRACT_TASK_KEY_GetExecutionTime");
        MAP_COPY_GET(XPORPERTY_CONTRACT_TASK_KEY, dispatch_tasks);
    }

    uint32_t task_id = 0;
    if (dispatch_tasks.size() > 0) {
        auto it = dispatch_tasks.end();
        it--;
        task_id = base::xstring_utl::touint32(it->first);
        task_id++;
    }
    return task_id;
}

void xzec_reward_contract::add_task(const uint32_t task_id,
                                    const uint64_t onchain_timer_round,
                                    const std::string & contract,
                                    const std::string & action,
                                    const std::string & params) {
    xreward_dispatch_task task;

    task.onchain_timer_round = onchain_timer_round;
    task.contract = contract;
    task.action = action;
    task.params = params;

    base::xstream_t stream(base::xcontext_t::instance());
    task.serialize_to(stream);
    std::stringstream ss;
    ss << std::setw(10) << std::setfill('0') << task_id;
    auto key = ss.str();
    {
        XMETRICS_TIME_RECORD(XREWARD_CONTRACT "XPORPERTY_CONTRACT_TASK_KEY_SetExecutionTime");
        MAP_SET(XPORPERTY_CONTRACT_TASK_KEY, key, std::string((char *)stream.data(), stream.size()));
    }
}

void xzec_reward_contract::execute_task() {
    XMETRICS_TIME_RECORD(XREWARD_CONTRACT "execute_task_ExecutionTime");
    std::map<std::string, std::string> dispatch_tasks;
    xreward_dispatch_task task;

    {
        XMETRICS_TIME_RECORD(XREWARD_CONTRACT "XPORPERTY_CONTRACT_TASK_KEY_CopyGetExecutionTime");
        MAP_COPY_GET(XPORPERTY_CONTRACT_TASK_KEY, dispatch_tasks);
    }

    xdbg("[xzec_reward_contract::execute_task] map size: %d\n", dispatch_tasks.size());
    XMETRICS_COUNTER_SET(XREWARD_CONTRACT "currentTaskCnt", dispatch_tasks.size());

    auto task_num_per_round = XGET_ONCHAIN_GOVERNANCE_PARAMETER(task_num_per_round);
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
                xinfo("[xzec_reward_contract::execute_task] contract: %s, action: %s, account: %s, reward: [%llu, %u], onchain_timer_round: %llu\n",
                    task.contract.c_str(),
                    task.action.c_str(),
                    r.first.c_str(),
                    static_cast<uint64_t>(r.second / REWARD_PRECISION),
                    static_cast<uint32_t>(r.second % REWARD_PRECISION),
                    task.onchain_timer_round);
            }
        } else if (task.action == XTRANSFER_ACTION) {
            std::map<std::string, uint64_t> issuances;
            base::xstream_t seo_stream(base::xcontext_t::instance(), (uint8_t *)task.params.c_str(), (uint32_t)task.params.size());
            seo_stream >> issuances;
            for (auto const & issue : issuances) {
                xinfo("[xzec_reward_contract::execute_task] action: %s, contract account: %s, issuance: %llu, onchain_timer_round: %llu\n",
                    task.action.c_str(),
                    issue.first.c_str(),
                    issue.second,
                    task.onchain_timer_round);
                TRANSFER(issue.first, issue.second);
            }
        }

        if (task.action != XTRANSFER_ACTION) {
            CALL(common::xaccount_address_t{task.contract}, task.action, task.params);
        }

        {
            XMETRICS_TIME_RECORD(XREWARD_CONTRACT "XPORPERTY_CONTRACT_TASK_KEY_RemoveExecutionTime");
            MAP_REMOVE(XPORPERTY_CONTRACT_TASK_KEY, it->first);
        }

        dispatch_tasks.erase(it);
    }
}

void xzec_reward_contract::print_tasks() {
#if defined(DEBUG)
    std::map<std::string, std::string> dispatch_tasks;
    MAP_COPY_GET(XPORPERTY_CONTRACT_TASK_KEY, dispatch_tasks);

    xreward_dispatch_task task;
    for (auto const & p : dispatch_tasks) {
        xstream_t stream(xcontext_t::instance(), (uint8_t *)p.second.c_str(), (uint32_t)p.second.size());
        task.serialize_from(stream);

        xdbg("[xzec_reward_contract::print_tasks] task id: %s, onchain_timer_round: %llu, contract: %s, action: %s\n",
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
                xdbg("[xzec_reward_contract::print_tasks] account: %s, reward: [%llu, %u]\n",
                    r.first.c_str(),
                    static_cast<uint64_t>(r.second / REWARD_PRECISION),
                    static_cast<uint32_t>(r.second % REWARD_PRECISION));
            }
        } else if (task.action == XTRANSFER_ACTION) {
            std::map<std::string, uint64_t> issuances;
            base::xstream_t seo_stream(base::xcontext_t::instance(), (uint8_t *)task.params.c_str(), (uint32_t)task.params.size());
            seo_stream >> issuances;
            for (auto const & issue : issuances) {
                xdbg("[xzec_reward_contract::print_tasks] contract account: %s, issuance: %llu\n",
                    issue.first.c_str(),
                    issue.second);
            }
        }
    }
#endif
}

void xzec_reward_contract::update_accumulated_issuance(uint64_t const issuance, uint64_t const timer_round) {
    XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "update_accumulated_issuance_Called", 1);
    auto current_year = (timer_round - get_activated_time()) / TIMER_BLOCK_HEIGHT_PER_YEAR + 1  ;

    uint64_t cur_year_issuances{0}, total_issuances{0};
    std::string cur_year_issuances_str = "", total_issuances_str = "";
    try {
        XMETRICS_TIME_RECORD(XREWARD_CONTRACT "XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE_GetExecutionTime");
        if (MAP_FIELD_EXIST(XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE, base::xstring_utl::tostring(current_year))) {
            cur_year_issuances_str = MAP_GET(XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE, base::xstring_utl::tostring(current_year));
        }
        total_issuances_str = MAP_GET(XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE, "total");
    } catch (std::runtime_error & e) {
        xwarn("[xzec_reward_contract][update_accumulated_issuance] read accumulated issuances error:%s", e.what());
        throw;
    }

    if (!cur_year_issuances_str.empty()) {
        cur_year_issuances = base::xstring_utl::touint64(cur_year_issuances_str);
    }

    cur_year_issuances += issuance;
    total_issuances = base::xstring_utl::touint64(total_issuances_str) + issuance;

    {
        XMETRICS_TIME_RECORD(XREWARD_CONTRACT "XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE_SetExecutionTime");

        MAP_SET(XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE, base::xstring_utl::tostring(current_year), base::xstring_utl::tostring(cur_year_issuances));
        MAP_SET(XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE, "total", base::xstring_utl::tostring(total_issuances));

        XMETRICS_PACKET_INFO(XREWARD_CONTRACT "issuance", "year", current_year,
            "issued", cur_year_issuances,
            "totalIssued", total_issuances);
        XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "update_accumulated_issuance_Executed", 1);
    }
    xkinfo("[xzec_reward_contract][update_accumulated_issuance] get stored accumulated issuance, year: %d, issuance: [%" PRIu64 ", total issuance: [%" PRIu64 ", timer round : %" PRIu64 "\n",
          current_year,
          cur_year_issuances,
          total_issuances,
          timer_round);
}

uint64_t xzec_reward_contract::get_activated_time() const {
    xactivation_record record;
    std::string value_str = STRING_GET2(xstake::XPORPERTY_CONTRACT_GENESIS_STAGE_KEY, sys_contract_rec_registration_addr);
    if (!value_str.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.c_str(), (uint32_t)value_str.size());
        record.serialize_from(stream);
    }
    xdbg("[xzec_reward_contract::is_mainnet_activated] activated: %d, activation_time: %llu, pid:%d\n", record.activated, record.activation_time, getpid());
    return record.activation_time;

}

top::xstake::uint128_t xzec_reward_contract::calc_issuance(uint64_t total_height) {
    if (0 == total_height) {
        return 0;
    }

    auto get_reserve_reward = [&](top::xstake::uint128_t issued_until_last_year_end) {
        top::xstake::uint128_t reserve_reward = 0;
        auto min_ratio_annual_total_reward = XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_ratio_annual_total_reward);
        auto minimum_issuance = static_cast<top::xstake::uint128_t>(TOTAL_ISSUANCE) * min_ratio_annual_total_reward / 100 * REWARD_PRECISION;
        if (static_cast<top::xstake::uint128_t>(TOTAL_RESERVE) * REWARD_PRECISION > issued_until_last_year_end) {
            auto issuance_rate = XGET_ONCHAIN_GOVERNANCE_PARAMETER(additional_issue_year_ratio);
            reserve_reward = std::max(static_cast<top::xstake::uint128_t>(static_cast<top::xstake::uint128_t>(TOTAL_RESERVE) * REWARD_PRECISION - issued_until_last_year_end) * issuance_rate / 100, minimum_issuance);
        } else {
            reserve_reward = minimum_issuance;
        }
        return reserve_reward;
    };

    top::xstake::uint128_t    additional_issuance = 0;
    uint64_t        issued_clocks       = 0; // from last issuance to last year end
    xaccumulated_reward_record record;
    get_accumulated_record(record);
    uint64_t call_duration_height = total_height - record.last_issuance_time;
    uint32_t current_year = (total_height - 1) / TIMER_BLOCK_HEIGHT_PER_YEAR + 1;
    uint32_t last_issuance_year = record.last_issuance_time == 0 ? 1 : (record.last_issuance_time - 1) / TIMER_BLOCK_HEIGHT_PER_YEAR + 1;
    xdbg("[xzec_reward_contract::calc_issuance] last_issuance_time: %llu, current_year: %u, last_issuance_year:%u",
        record.last_issuance_time, current_year, last_issuance_year);
    while (last_issuance_year < current_year) {
        uint64_t remaining_clocks = TIMER_BLOCK_HEIGHT_PER_YEAR - record.last_issuance_time % TIMER_BLOCK_HEIGHT_PER_YEAR;
        if (remaining_clocks > 0) {
            auto reserve_reward = get_reserve_reward(record.issued_until_last_year_end);
            additional_issuance += reserve_reward * remaining_clocks / TIMER_BLOCK_HEIGHT_PER_YEAR;
            xinfo("[xzec_reward_contract::calc_issuance] cross year, last_issuance_year: %u, reserve_reward: [%llu, %u], remaining_clocks: %llu, issued_clocks: %u, additional_issuance: [%llu, %u], issued_until_last_year_end: [%llu, %u]",
                last_issuance_year,
                static_cast<uint64_t>(reserve_reward / REWARD_PRECISION),
                static_cast<uint32_t>(reserve_reward % REWARD_PRECISION),
                remaining_clocks,
                issued_clocks,
                static_cast<uint64_t>(additional_issuance / REWARD_PRECISION),
                static_cast<uint32_t>(additional_issuance % REWARD_PRECISION),
                static_cast<uint64_t>(record.issued_until_last_year_end / REWARD_PRECISION),
                static_cast<uint32_t>(record.issued_until_last_year_end % REWARD_PRECISION));
            issued_clocks += remaining_clocks;
            record.last_issuance_time += remaining_clocks;
            record.issued_until_last_year_end += reserve_reward;
        }
        last_issuance_year++;
    }

    auto reserve_reward = get_reserve_reward(record.issued_until_last_year_end);
    additional_issuance += reserve_reward * (call_duration_height - issued_clocks) / TIMER_BLOCK_HEIGHT_PER_YEAR;

    xinfo("[xzec_reward_contract::calc_issuance] additional_issuance: [%" PRIu64 ", %u], call_duration_height: %" PRId64
          ", total_height: %" PRId64 ", current_year: %" PRIu32 ", last_issuance_year: %" PRIu32
          ", pid: %d"
          ", reserve_reward: [%llu, %u], last_issuance_time: %llu, issued_until_last_year_end: [%llu, %u], TIMER_BLOCK_HEIGHT_PER_YEAR: %llu",
         static_cast<uint64_t>(additional_issuance / REWARD_PRECISION),
         static_cast<uint32_t>(additional_issuance % REWARD_PRECISION),
         call_duration_height,
         total_height,
         current_year,
         last_issuance_year,
         getpid(),
         static_cast<uint64_t>(reserve_reward / REWARD_PRECISION),
         static_cast<uint32_t>(reserve_reward % REWARD_PRECISION),
         record.last_issuance_time,
         static_cast<uint64_t>(record.issued_until_last_year_end / REWARD_PRECISION),
         static_cast<uint32_t>(record.issued_until_last_year_end % REWARD_PRECISION),
         TIMER_BLOCK_HEIGHT_PER_YEAR);

    record.last_issuance_time = total_height;
    update_accumulated_record(record);

    return additional_issuance;
}

top::xstake::uint128_t xzec_reward_contract::get_reward(top::xstake::uint128_t issuance, xreward_type reward_type) {
    uint64_t reward_numerator = 0;
    if (reward_type == xreward_type::edge_reward) {
        reward_numerator = XGET_ONCHAIN_GOVERNANCE_PARAMETER(edge_reward_ratio);
    } else if (reward_type == xreward_type::archive_reward) {
        reward_numerator = XGET_ONCHAIN_GOVERNANCE_PARAMETER(archive_reward_ratio);
    } else if (reward_type == xreward_type::validator_reward) {
        reward_numerator = XGET_ONCHAIN_GOVERNANCE_PARAMETER(validator_reward_ratio);
    } else if (reward_type == xreward_type::auditor_reward) {
        reward_numerator = XGET_ONCHAIN_GOVERNANCE_PARAMETER(auditor_reward_ratio);
    } else if (reward_type == xreward_type::vote_reward) {
        reward_numerator = XGET_ONCHAIN_GOVERNANCE_PARAMETER(vote_reward_ratio);
    } else if (reward_type == xreward_type::governance_reward) {
        reward_numerator = XGET_ONCHAIN_GOVERNANCE_PARAMETER(governance_reward_ratio);
    }
    return issuance * reward_numerator / 100;
}

int xzec_reward_contract::get_accumulated_record(xaccumulated_reward_record & record) {
    std::string value_str = STRING_GET(XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE_YEARLY);
    xstream_t stream(xcontext_t::instance(), (uint8_t *)value_str.c_str(), (uint32_t)value_str.size());
    record.serialize_from(stream);

    return 0;
}

void xzec_reward_contract::update_accumulated_record(const xaccumulated_reward_record & record) {
    base::xstream_t stream(base::xcontext_t::instance());
    record.serialize_to(stream);
    auto value_str = std::string((char *)stream.data(), stream.size());
    STRING_SET(XPROPERTY_CONTRACT_ACCUMULATED_ISSUANCE_YEARLY, value_str);

    return;
}

int xzec_reward_contract::get_node_info(const std::map<std::string, xreg_node_info> & map_nodes, const std::string & account, xreg_node_info & node) {
    auto it = map_nodes.find(account);
    if (it == map_nodes.end()) {
        return -1;
    }
    node = it->second;
    return 0;
}

void xzec_reward_contract::on_receive_workload(std::string const& workload_str) {
    XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "on_receive_workload_Called", 1);
    XMETRICS_TIME_RECORD(XREWARD_CONTRACT "on_receive_workload_ExecutionTime");
    auto const& source_address = SOURCE_ADDRESS();

    xstream_t stream(xcontext_t::instance(), (uint8_t*)workload_str.data(), workload_str.size());
    std::map<common::xcluster_address_t, xauditor_workload_info_t> auditor_workload_info;
    std::map<common::xcluster_address_t, xvalidator_workload_info_t> validator_workload_info;

    MAP_OBJECT_DESERIALZE2(stream, auditor_workload_info);
    MAP_OBJECT_DESERIALZE2(stream, validator_workload_info);
    xdbg("[xzec_reward_contract::on_receive_workload] pid:%d, SOURCE_ADDRESS: %s, auditor_workload_info size: %zu, validator_workload_info size: %zu\n",
        getpid(), source_address.c_str(), auditor_workload_info.size(), validator_workload_info.size());

    //add_batch_workload2(auditor_workload_info, validator_workload_info);
    for (auto const& workload : auditor_workload_info) {
        xstream_t stream(xcontext_t::instance());
        stream << workload.first;
        auto const& cluster_id      = std::string((const char*)stream.data(), stream.size());
        auto const& workload_info   = workload.second;
        add_cluster_workload(true, cluster_id, workload_info.m_leader_count);
    }

    for (auto const& workload : validator_workload_info) {
        xstream_t stream(xcontext_t::instance());
        stream << workload.first;
        auto const& cluster_id      = std::string((const char*)stream.data(), stream.size());
        auto const& workload_info   = workload.second;
        add_cluster_workload(false, cluster_id, workload_info.m_leader_count);
    }

    XMETRICS_COUNTER_INCREMENT(XREWARD_CONTRACT "on_receive_workload_Executed", 1);
}

void xzec_reward_contract::add_cluster_workload(bool auditor, std::string const& cluster_id, std::map<std::string, uint32_t> const& leader_count) {
    const char* property;
    if (auditor) {
        property = XPORPERTY_CONTRACT_WORKLOAD_KEY;
    } else {
        property = XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY;
    }
    if (!MAP_PROPERTY_EXIST(property)) {
        MAP_CREATE(property);
    }
    common::xcluster_address_t cluster_id2;
    {

        xstream_t stream(xcontext_t::instance(), (uint8_t*)cluster_id.data(), cluster_id.size());
        stream >> cluster_id2;
        xdbg("[xzec_reward_contract::add_cluster_workload] auditor: %d, cluster_id: %s, group size: %d",
            auditor, cluster_id2.to_string().c_str(), leader_count.size());
    }

    cluster_workload_t workload;
    std::string value_str;
    int32_t ret;
    if (auditor) {
        XMETRICS_TIME_RECORD(XREWARD_CONTRACT "XPORPERTY_CONTRACT_WORKLOAD_KEY_GetExecutionTime");
        ret = MAP_GET2(property, cluster_id, value_str);
    } else {
        XMETRICS_TIME_RECORD(XREWARD_CONTRACT "XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY_GetExecutionTime");
        ret = MAP_GET2(property, cluster_id, value_str);
    }

    if (ret) {
        xdbg("[xzec_reward_contract::add_cluster_workload] cluster_id not exist, auditor: %d\n", auditor);
        workload.cluster_id = cluster_id;
    } else {
        xstream_t stream(xcontext_t::instance(), (uint8_t*)value_str.data(), value_str.size());
        workload.serialize_from(stream);
    }

    for (auto const& leader_count_info : leader_count) {
        auto const& leader  = leader_count_info.first;
        auto const& work   = leader_count_info.second;

        workload.m_leader_count[leader] += work;
        workload.cluster_total_workload += work;
        xdbg("[xzec_reward_contract::add_cluster_workload] auditor: %d, cluster_id: %s, leader: %s, work: %u, total_workload: %d\n",
            auditor, cluster_id2.to_string().c_str(), leader_count_info.first.c_str(), workload.m_leader_count[leader], workload.cluster_total_workload);
    }

    xstream_t stream(xcontext_t::instance());
    workload.serialize_to(stream);
    std::string value = std::string((const char*)stream.data(), stream.size());
    if (auditor) {
        XMETRICS_TIME_RECORD(XREWARD_CONTRACT "XPORPERTY_CONTRACT_WORKLOAD_KEY_SetExecutionTime");
        MAP_SET(property, cluster_id, value);
    } else {
        XMETRICS_TIME_RECORD(XREWARD_CONTRACT "XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY_SetExecutionTime");
        MAP_SET(property, cluster_id, value);
    }
}

void xzec_reward_contract::clear_workload() {
    XMETRICS_TIME_RECORD("zec_reward_clear_workload_all_time");

    {
        XMETRICS_TIME_RECORD(XREWARD_CONTRACT "XPORPERTY_CONTRACT_WORKLOAD_KEY_SetExecutionTime");
        CLEAR(enum_type_t::map, XPORPERTY_CONTRACT_WORKLOAD_KEY);
    }
    {
        XMETRICS_TIME_RECORD(XREWARD_CONTRACT "XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY_SetExecutionTime");
        CLEAR(enum_type_t::map, XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY);
    }
}

void xzec_reward_contract::update_issuance_detail(xissue_detail const & issue_detail) {
    xdbg("[xzec_reward_contract::update_issuance_detail] onchain_timer_round: %llu, m_zec_vote_contract_height: %llu, "
        "m_zec_workload_contract_height: %llu, m_zec_reward_contract_height: %llu, "
        "m_edge_reward_ratio: %u, m_archive_reward_ratio: %u "
        "m_validator_reward_ratio: %u, m_auditor_reward_ratio: %u, m_vote_reward_ratio: %u, m_governance_reward_ratio: %u",
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
    auto issue_detail_str = issue_detail.to_string();
    if (!STRING_EXIST(XPROPERTY_REWARD_DETAIL)) {
        STRING_CREATE(XPROPERTY_REWARD_DETAIL);
    }
    try {
        STRING_SET(XPROPERTY_REWARD_DETAIL, issue_detail_str);
    } catch (std::runtime_error & e) {
        xdbg("[xzec_reward_contract::update_issuance_detail] STRING_SET XPROPERTY_REWARD_DETAIL error:%s", e.what());
    }
}

NS_END2

#undef XREWARD_CONTRACT
#undef XCONTRACT_PREFIX
#undef XZEC_MODULE

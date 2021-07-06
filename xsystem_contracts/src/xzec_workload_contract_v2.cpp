// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xchain_upgrade/xchain_upgrade_center.h"
#include "xconfig/xpredefined_configurations.h"
#include "xdata/xblock_statistics_data.h"
#include "xdata/xnative_contract_address.h"
#include "xdata/xworkload_info.h"
#include "xdata/xfull_tableblock.h"
#include "xstake/xstake_algorithm.h"
#include "xvm/manager/xcontract_manager.h"
#include "xvm/xsystem_contracts/xworkload/xzec_workload_contract_v2.h"

using top::base::xstream_t;
using top::base::xcontext_t;
using top::base::xstring_utl;
using top::base::xtime_utl;
using namespace top::xstake;
using namespace top::config;

#if !defined (XZEC_MODULE)
#define XZEC_MODULE "sysContract_"
#endif

#define XCONTRACT_PREFIX "workload_v2_"

#define XWORKLOAD_CONTRACT XZEC_MODULE XCONTRACT_PREFIX

NS_BEG3(top, xvm, system_contracts)

xzec_workload_contract_v2::xzec_workload_contract_v2(common::xnetwork_id_t const & network_id)
    : xbase_t{ network_id } {
}

void xzec_workload_contract_v2::setup() {
    // key: common::xaccount_address_t(table), value: uint64(height)
    MAP_CREATE(XPORPERTY_CONTRACT_TABLEBLOCK_HEIGHT_KEY);
    MAP_CREATE(XPORPERTY_CONTRACT_WORKLOAD_KEY);
    MAP_CREATE(XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY);
    STRING_CREATE(XPORPERTY_CONTRACT_TGAS_KEY);
}

bool xzec_workload_contract_v2::is_mainnet_activated() const {
    xactivation_record record;

    std::string value_str = STRING_GET2(xstake::XPORPERTY_CONTRACT_GENESIS_STAGE_KEY, sys_contract_rec_registration_addr);
    if (!value_str.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(),
                    (uint8_t*)value_str.c_str(), (uint32_t)value_str.size());
        record.serialize_from(stream);
    }
    xdbg("[xzec_workload_contract_v2::is_mainnet_activated] activated: %d\n", record.activated);
    return static_cast<bool>(record.activated);
};

std::vector<xobject_ptr_t<data::xblock_t>> xzec_workload_contract_v2::get_fullblock(const uint32_t table_id, common::xlogic_time_t const timestamp) {
    uint64_t last_read_height = get_table_height(table_id);
    uint64_t cur_read_height = last_read_height;
    // calc table address
    auto const & table_owner = common::xaccount_address_t{xdatautil::serialize_owner_str(sys_contract_sharding_table_block_addr, table_id)};
    // get block
    std::vector<xobject_ptr_t<data::xblock_t>> res;
    uint64_t time_interval = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::minutes{10}).count() / XGLOBAL_TIMER_INTERVAL_IN_SECONDS;
    while (true) {
        base::xauto_ptr<data::xblock_t> next_block = get_next_fullblock(table_owner.value(), cur_read_height);
        if (nullptr == next_block) {
            xdbg("[xzec_workload_contract_v2::get_fullblock] table %s next full block is empty, last read height: %lu", table_owner.value().c_str(), last_read_height);
            break;
        }
        auto block_height = next_block->get_height();
        XCONTRACT_ENSURE(next_block->is_fulltable(), "[xzec_workload_contract_v2::get_fullblock] next full tableblock is not full tableblock");
        // XCONTRACT_ENSURE(timestamp > next_block->get_clock(), "[xzec_workload_contract_v2::get_fullblock] time order error");
        if (timestamp < next_block->get_clock() || timestamp < (next_block->get_clock() + time_interval)) {
            xdbg("[xzec_workload_contract_v2::get_fullblock] clock interval not statisfy, timestamp: %lu, table: %s, full block height: %" PRIu64 ", block clock: %" PRIu64,
                 timestamp,
                 table_owner.value().c_str(),
                 block_height,
                 next_block->get_clock());
            break;
        } else {
            xdbg("[xzec_workload_contract_v2::get_fullblock] table: %s, block height: %" PRIu64 " push in", table_owner.value().c_str(), block_height);
            cur_read_height = block_height;
            res.push_back(std::move(next_block));
        }
    }
    // update table address height
    if (cur_read_height > last_read_height) {
        update_table_height(table_id, cur_read_height);
    }
    return res;
}

uint64_t xzec_workload_contract_v2::get_table_height(const uint32_t table_id) const {
    uint64_t last_read_height = 0;
    std::string value_str;
    XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "get_property_fulltableblock_height");

    std::string key = std::to_string(table_id);
    if (MAP_FIELD_EXIST(XPORPERTY_CONTRACT_TABLEBLOCK_HEIGHT_KEY, key)) {
        value_str = MAP_GET(XPORPERTY_CONTRACT_TABLEBLOCK_HEIGHT_KEY, key);
        XCONTRACT_ENSURE(!value_str.empty(), "read full tableblock height empty");
    }
    if (!value_str.empty()) {
        last_read_height = xstring_utl::touint64(value_str);
    }

    return last_read_height;
}

void xzec_workload_contract_v2::update_table_height(const uint32_t table_id, const uint64_t cur_read_height) {
    XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "set_property_contract_fulltableblock_height");
    MAP_SET(XPORPERTY_CONTRACT_TABLEBLOCK_HEIGHT_KEY, std::to_string(table_id), xstring_utl::tostring(cur_read_height));
}

void xzec_workload_contract_v2::update_tgas(int64_t table_pledge_balance_change_tgas) {
    std::string pledge_tgas_str;
    {
        XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "XPORPERTY_CONTRACT_TGAS_KEY_GetExecutionTime");
        pledge_tgas_str = STRING_GET2(XPORPERTY_CONTRACT_TGAS_KEY);
    }
    int64_t tgas = 0;
    if (!pledge_tgas_str.empty()) {
        tgas = xstring_utl::toint64(pledge_tgas_str);
    }
    tgas += table_pledge_balance_change_tgas;

    {
        XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "XPORPERTY_CONTRACT_TGAS_KEY_SetExecutionTime");
        STRING_SET(XPORPERTY_CONTRACT_TGAS_KEY, xstring_utl::tostring(tgas));
    }
}

void xzec_workload_contract_v2::accumulate_auditor_workload(common::xgroup_address_t const & group_addr,
                                                            std::string const & account_str,
                                                            const uint32_t slotid,
                                                            xgroup_related_statistics_data_t const & group_account_data,
                                                            const uint32_t workload_per_tableblock,
                                                            const uint32_t workload_per_tx,
                                                            std::map<common::xgroup_address_t, xauditor_workload_info_t> & auditor_group_workload) {
    uint32_t block_count = group_account_data.account_statistics_data[slotid].block_data.block_count;
    uint32_t tx_count = group_account_data.account_statistics_data[slotid].block_data.transaction_count;
    uint32_t workload = block_count * workload_per_tableblock + tx_count * workload_per_tx;
    if (workload > 0) {
        auto it2 = auditor_group_workload.find(group_addr);
        if (it2 == auditor_group_workload.end()) {
            xauditor_workload_info_t auditor_workload_info;
            auto ret = auditor_group_workload.insert(std::make_pair(group_addr, auditor_workload_info));
            XCONTRACT_ENSURE(ret.second, "insert auditor workload failed");
            it2 = ret.first;
        }
        it2->second.m_leader_count[account_str] += workload;
    }

    xdbg(
        "[xzec_workload_contract_v2::accumulate_auditor_workload] group_addr: [%s, network_id: %u, zone_id: %u, cluster_id: %u, group_id: %u], leader: %s, "
        "workload: %u, block_count: %u, tx_count: %u, workload_per_tableblock: %u, workload_per_tx: %u",
        group_addr.to_string().c_str(),
        group_addr.network_id().value(),
        group_addr.zone_id().value(),
        group_addr.cluster_id().value(),
        group_addr.group_id().value(),
        account_str.c_str(),
        workload,
        block_count,
        tx_count,
        workload_per_tableblock,
        workload_per_tx);
}

void xzec_workload_contract_v2::accumulate_validator_workload(common::xgroup_address_t const & group_addr,
                                                              std::string const & account_str,
                                                              const uint32_t slotid,
                                                              xgroup_related_statistics_data_t const & group_account_data,
                                                              const uint32_t workload_per_tableblock,
                                                              const uint32_t workload_per_tx,
                                                              std::map<common::xgroup_address_t, xvalidator_workload_info_t> & validator_group_workload) {
    uint32_t block_count = group_account_data.account_statistics_data[slotid].block_data.block_count;
    uint32_t tx_count = group_account_data.account_statistics_data[slotid].block_data.transaction_count;
    uint32_t workload = block_count * workload_per_tableblock + tx_count * workload_per_tx;
    if (workload > 0) {
        auto it2 = validator_group_workload.find(group_addr);
        if (it2 == validator_group_workload.end()) {
            xvalidator_workload_info_t validator_workload_info;
            auto ret = validator_group_workload.insert(std::make_pair(group_addr, validator_workload_info));
            XCONTRACT_ENSURE(ret.second, "insert auditor workload failed");
            it2 = ret.first;
        }

        it2->second.m_leader_count[account_str] += workload;
    }

    xdbg(
        "[xzec_workload_contract_v2::accumulate_validator_workload] group_addr: [%s, network_id: %u, zone_id: %u, cluster_id: %u, group_id: %u], leader: %s, "
        "workload: %u, block_count: %u, tx_count: %u, workload_per_tableblock: %u, workload_per_tx: %u",
        group_addr.to_string().c_str(),
        group_addr.network_id().value(),
        group_addr.zone_id().value(),
        group_addr.cluster_id().value(),
        group_addr.group_id().value(),
        account_str.c_str(),
        workload,
        block_count,
        tx_count,
        workload_per_tableblock,
        workload_per_tx);
}

void xzec_workload_contract_v2::accumulate_workload(xstatistics_data_t const & stat_data,
                                                    std::map<common::xgroup_address_t, xauditor_workload_info_t> & auditor_group_workload,
                                                    std::map<common::xgroup_address_t, xvalidator_workload_info_t> & validator_group_workload) {
    auto node_service = contract::xcontract_manager_t::instance().get_node_service();
    auto workload_per_tableblock = XGET_ONCHAIN_GOVERNANCE_PARAMETER(workload_per_tableblock);
    auto workload_per_tx = XGET_ONCHAIN_GOVERNANCE_PARAMETER(workload_per_tx);
    for (auto const & static_item: stat_data.detail) {
        auto elect_statistic = static_item.second;
        for (auto const & group_item: elect_statistic.group_statistics_data) {
            common::xgroup_address_t const & group_addr = group_item.first;
            xgroup_related_statistics_data_t const & group_account_data = group_item.second;
            xvip2_t const &group_xvip2 = top::common::xip2_t{
                group_addr.network_id(),
                group_addr.zone_id(),
                group_addr.cluster_id(),
                group_addr.group_id(),
                (uint16_t)group_account_data.account_statistics_data.size(),
                static_item.first};
            xdbg("[xzec_workload_contract_v2::accumulate_workload] group xvip2: %llu, %llu",
                group_xvip2.high_addr,
                group_xvip2.low_addr);
            bool is_auditor = false;
            if (common::has<common::xnode_type_t::auditor>(group_addr.type())) {
                is_auditor = true;
            } else if (common::has<common::xnode_type_t::validator>(group_addr.type())) {
                is_auditor = false;
            } else {
                // invalid group
                xwarn("[xzec_workload_contract_v2::accumulate_workload] invalid group id: %d", group_addr.group_id().value());
                continue;
            }
            for (size_t slotid = 0; slotid < group_account_data.account_statistics_data.size(); ++slotid) {
                auto account_str = node_service->get_group(group_xvip2)->get_node(slotid)->get_account();
                if (is_auditor) {
                    accumulate_auditor_workload(group_addr, account_str, slotid, group_account_data, workload_per_tableblock, workload_per_tx, auditor_group_workload);
                } else {
                    accumulate_validator_workload(group_addr, account_str, slotid, group_account_data, workload_per_tableblock, workload_per_tx, validator_group_workload);
                }
            }
        }
    }
}

void xzec_workload_contract_v2::accumulate_workload_with_fullblock(common::xlogic_time_t const timestamp,
                                                                   std::map<common::xgroup_address_t, xauditor_workload_info_t> & auditor_group_workload,
                                                                   std::map<common::xgroup_address_t, xvalidator_workload_info_t> & validator_group_workload) {
    XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "accumulate_total_time");
    xinfo("[xzec_workload_contract_v2::accumulate_workload_with_fullblock] enum_vbucket_has_tables_count %d, timestamp: %llu", enum_vledger_const::enum_vbucket_has_tables_count, timestamp);
    int64_t table_pledge_balance_change_tgas = 0;
    int64_t total_table_fullblock_num = 0;
    int64_t total_get_fullblock_time = 0;
    int64_t total_accumulate_workload_time = 0;
    // split tables
    xinterval_t workload_collection_interval = XGET_ONCHAIN_GOVERNANCE_PARAMETER(workload_collection_interval);
#if 0
    xinterval_t one_hour = 360;
    uint32_t split_num = one_hour / workload_collection_interval;
    if (split_num == 0) {
        split_num = 1;
    }
    xinterval_t split_interval = one_hour / split_num;
    uint32_t total_tables = enum_vledger_const::enum_vbucket_has_tables_count;
    uint32_t table_per_split = total_tables / split_num;
    uint32_t round = timestamp % one_hour / split_interval;
    uint32_t strat_table = round * table_per_split;
    uint32_t end_table = (round + 1 == split_num) ? (total_tables - 1) : ((round + 1) * table_per_split - 1);
#else
    uint32_t split_num = 4;
    xinterval_t split_interval = workload_collection_interval * split_num;
    uint32_t total_tables = enum_vledger_const::enum_vbucket_has_tables_count;
    uint32_t table_per_split = total_tables / split_num;
    uint32_t round = timestamp % split_interval / workload_collection_interval;
    uint32_t strat_table = round * table_per_split;
    uint32_t end_table = (round + 1 == split_num) ? (total_tables - 1) : ((round + 1) * table_per_split - 1);
#endif
    xinfo(
        "[xzec_workload_contract_v2::accumulate_workload_with_fullblock] timstamp: %lu, workload_collection_interval: %u, split_num: %u, split_interval: %u, table_per_split: %u, "
        "round: %u, strat_table: %u, end_table: %u",
        timestamp,
        workload_collection_interval,
        split_num,
        split_interval,
        table_per_split,
        round,
        strat_table,
        end_table);

    for (auto i = strat_table; i <= end_table; i++) {
        int64_t t1 = 0;
        int64_t t2 = 0;
        int64_t t3 = 0;
        t1 = xtime_utl::time_now_ms();
        // get table block
        auto const & full_blocks = get_fullblock(i, timestamp);
        t2 = xtime_utl::time_now_ms();
        // accumulate workload
        for (std::size_t block_index = 0; block_index < full_blocks.size(); block_index++) {
            xdbg("[xzec_workload_contract_v2::accumulate_workload_with_fullblock] block_index: %u", block_index);
            xfull_tableblock_t *full_tableblock = dynamic_cast<xfull_tableblock_t *>(full_blocks[block_index].get());
            assert(full_tableblock != nullptr);
            auto const & stat_data = full_tableblock->get_table_statistics();
            accumulate_workload(stat_data, auditor_group_workload, validator_group_workload);
            // m_table_pledge_balance_change_tgas
            table_pledge_balance_change_tgas += full_tableblock->get_pledge_balance_change_tgas();
        }
        t3 = xtime_utl::time_now_ms();
        total_get_fullblock_time += t2 - t1;
        total_accumulate_workload_time += t3 - t2;
        total_table_fullblock_num += full_blocks.size();
        if (full_blocks.size() >  0) {
            xinfo("[xzec_workload_contract_v2::accumulate_workload_with_fullblock] bucket index: %d, timer round: %" PRIu64 ", pid: %d, total_table_block_count: %" PRIu32 ", table_pledge_balance_change_tgas: %lld, this: %p\n",
                i,
                timestamp,
                getpid(),
                full_blocks.size(),
                table_pledge_balance_change_tgas,
                this);
        }
    }
    xinfo("[xzec_workload_contract_v2::accumulate_workload_with_fullblock] total_block_nums: %u, total_time: %ums, get_fullblock_time: %ums, accumulate_workload_time: %ums",
          total_table_fullblock_num,
          total_get_fullblock_time + total_accumulate_workload_time,
          total_get_fullblock_time,
          total_accumulate_workload_time);
#if defined DEBUG
    for (auto & entity : auditor_group_workload) {
        auto const & group = entity.first;
        for (auto const & wl : entity.second.m_leader_count) {
            xdbg("[xzec_workload_contract_v2::accumulate_workload_with_fullblock] workload auditor group: %s, leader: %s, workload: %u", group.to_string().c_str(), wl.first.c_str(), wl.second);
        }
        xdbg("[xzec_workload_contract_v2::accumulate_workload_with_fullblock] workload auditor group: %s, group id: %u, ends", group.to_string().c_str(), group.group_id().value());
    }

    for (auto & entity : validator_group_workload) {
        auto const & group = entity.first;
        for (auto const & wl : entity.second.m_leader_count) {
            xdbg("[xzec_workload_contract_v2::accumulate_workload_with_fullblock] workload validator group: %s, leader: %s, workload: %u", group.to_string().c_str(), wl.first.c_str(), wl.second);
        }
        xdbg("[xzec_workload_contract_v2::accumulate_workload_with_fullblock] workload validator group: %s, group id: %u, ends", group.to_string().c_str(), group.group_id().value());
    }
#endif
    update_tgas(table_pledge_balance_change_tgas);
}

void xzec_workload_contract_v2::on_timer(common::xlogic_time_t const timestamp) {
    XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "on_timer");
    // check address
    auto const & self_account = SELF_ADDRESS();
    auto const & source_address = SOURCE_ADDRESS();
    if (self_account.value() != source_address) {
        xwarn("[xzec_workload_contract_v2::on_timer] invalid call from %s", source_address.c_str());
        return;
    }
    xinfo("[xzec_workload_contract_v2::on_timer] timestamp: %lu, self: %s, src: %s", timestamp, self_account.value().c_str(), source_address.c_str());

    std::map<common::xgroup_address_t, xauditor_workload_info_t> auditor_clusters_workloads;
    std::map<common::xgroup_address_t, xvalidator_workload_info_t> validator_clusters_workloads;
    accumulate_workload_with_fullblock(timestamp, auditor_clusters_workloads, validator_clusters_workloads);

    // check mainnet
    if (!is_mainnet_activated()) {
        return;
    }

    XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "call_reward_contract");
    std::map<common::xcluster_address_t, xauditor_workload_info_t> auditor_clusters_workloads2;
    std::map<common::xcluster_address_t, xvalidator_workload_info_t> validator_clusters_workloads2;
    int count = 0;
    const int upload_cnt = 1000;
    for (auto it = auditor_clusters_workloads.begin(); it != auditor_clusters_workloads.end(); it++) {
        auto const & cluster_id2 = it->first;
        auto const & workload = it->second;
        for (auto const& leader_count_info : workload.m_leader_count) {
            auto const& leader  = leader_count_info.first;
            auto const& work   = leader_count_info.second;

            auto it3 = auditor_clusters_workloads2.find(cluster_id2);
            if (it3 == auditor_clusters_workloads2.end()) {
                xauditor_workload_info_t auditor_workload_info;
                auto ret = auditor_clusters_workloads2.insert(std::make_pair(cluster_id2, auditor_workload_info));
                it3 = ret.first;
            }
            it3->second.m_leader_count[leader] += work;

            if (++count % upload_cnt == 0) {
                std::string workload_info;
                {
                    xstream_t stream(xcontext_t::instance());
                    MAP_OBJECT_SERIALIZE2(stream, auditor_clusters_workloads2);
                    MAP_OBJECT_SERIALIZE2(stream, validator_clusters_workloads2);

                    xinfo("[xzec_workload_contract_v2::on_timer] report workload to zec reward, auditor_clusters_workloads size: %d, validator_clusters_workloads size: %d, timer round: %" PRIu64 ", count: %d, round: %d",
                        auditor_clusters_workloads2.size(),
                        validator_clusters_workloads2.size(),
                        timestamp,
                        count,
                        (count + upload_cnt - 1) / upload_cnt);
                    workload_info = std::string((char *)stream.data(), stream.size());
                }

                {
                    xstream_t stream(xcontext_t::instance());
                    stream << timestamp;
                    stream << workload_info;
                    CALL(common::xaccount_address_t{sys_contract_zec_reward_addr}, "calculate_reward", std::string((char *)stream.data(), stream.size()));
                    auditor_clusters_workloads2.clear();
                }
            }
        }
    }

    for (auto it2 = validator_clusters_workloads.begin(); it2 != validator_clusters_workloads.end(); it2++) {
        auto const & cluster_id2 = it2->first;
        auto const & workload = it2->second;
        for (auto const& leader_count_info : workload.m_leader_count) {
            auto const& leader  = leader_count_info.first;
            auto const& work   = leader_count_info.second;

            auto it3 = validator_clusters_workloads2.find(cluster_id2);
            if (it3 == validator_clusters_workloads2.end()) {
                xvalidator_workload_info_t validator_workload_info;
                auto ret = validator_clusters_workloads2.insert(std::make_pair(cluster_id2, validator_workload_info));
                it3 = ret.first;
            }
            it3->second.m_leader_count[leader] += work;

            if (++count % upload_cnt == 0) {
                std::string workload_info;
                {
                    xstream_t stream(xcontext_t::instance());
                    MAP_OBJECT_SERIALIZE2(stream, auditor_clusters_workloads2);
                    MAP_OBJECT_SERIALIZE2(stream, validator_clusters_workloads2);

                    xinfo("[xzec_workload_contract_v2::on_timer] report workload to zec reward, auditor_clusters_workloads size: %d, validator_clusters_workloads size: %d, timer round: %" PRIu64 ", count: %d, round: %d",
                        auditor_clusters_workloads2.size(),
                        validator_clusters_workloads2.size(),
                        timestamp,
                        count,
                        (count + upload_cnt - 1) / upload_cnt);
                    workload_info = std::string((char *)stream.data(), stream.size());
                }
                {
                    xstream_t stream(xcontext_t::instance());
                    stream << timestamp;
                    stream << workload_info;
                    CALL(common::xaccount_address_t{sys_contract_zec_reward_addr}, "calculate_reward", std::string((char *)stream.data(), stream.size()));
                    auditor_clusters_workloads2.clear();
                    validator_clusters_workloads2.clear();
                }
            }
        }
    }
    if (auditor_clusters_workloads2.size() > 0 || validator_clusters_workloads2.size() > 0) {
        xinfo("[xzec_workload_contract_v2::on_timer] report workload to zec reward, auditor_clusters_workloads size: %d, validator_clusters_workloads size: %d, timer round: %" PRIu64 ", count: %d, round: %d",
            auditor_clusters_workloads.size(),
            validator_clusters_workloads.size(),
            timestamp,
            count,
            (count + upload_cnt - 1) / upload_cnt);
        std::string workload_info;
        {
            xstream_t stream(xcontext_t::instance());
            MAP_OBJECT_SERIALIZE2(stream, auditor_clusters_workloads2);
            MAP_OBJECT_SERIALIZE2(stream, validator_clusters_workloads2);
            workload_info = std::string((char *)stream.data(), stream.size());
        }

        xstream_t stream(xcontext_t::instance());
        stream << timestamp;
        stream << workload_info;
        CALL(common::xaccount_address_t{sys_contract_zec_reward_addr}, "calculate_reward", std::string((char *)stream.data(), stream.size()));
    }
}

NS_END3

#undef XWORKLOAD_CONTRACT
#undef XCONTRACT_PREFIX
#undef XZEC_MODULE

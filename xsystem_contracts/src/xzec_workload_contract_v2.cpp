// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xworkload/xzec_workload_contract_v2.h"

// #include "xstore/xstore_error.h"
// #include "xbasic/xutility.h"
#include "xchain_upgrade/xchain_upgrade_center.h"
// #include "xdata/xgenesis_data.h"
#include "xdata/xblock_statistics_data.h"
#include "xdata/xnative_contract_address.h"
#include "xdata/xworkload_info.h"
// #include "xbase/xutl.h"
// #include "xstake/xstake_algorithm.h"
#include "xvm/manager/xcontract_manager.h"
// #include <iomanip>

using top::base::xstream_t;
using top::base::xcontext_t;
using top::base::xstring_utl;
// using namespace top::data;

#if !defined (XZEC_MODULE)
#define XZEC_MODULE "sysContract_"
#endif

#define XCONTRACT_PREFIX "workload_v2_"

#define XWORKLOAD_CONTRACT XZEC_MODULE XCONTRACT_PREFIX

NS_BEG2(top, xstake)

xzec_workload_contract_v2::xzec_workload_contract_v2(common::xnetwork_id_t const & network_id)
    : xbase_t{ network_id } {
}

void xzec_workload_contract_v2::setup() {
    MAP_CREATE(XPORPERTY_CONTRACT_WORKLOAD_KEY); // accumulate auditor cluster workload
    MAP_CREATE(XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY); // accumulate validator cluster workload
    MAP_CREATE(XPROPERTY_CONTRACT_LAST_READ_TABLE_BLOCK_HEIGHT); 
    MAP_CREATE(XPROPERTY_CONTRACT_LAST_READ_TABLE_BLOCK_TIME);
    STRING_CREATE(XPROPERTY_CONTRACT_WORKLOAD_DATA_MIGRATION_FLAG); 
    STRING_SET(XPROPERTY_CONTRACT_WORKLOAD_DATA_MIGRATION_FLAG, xstring_utl::tostring(0));
}

int xzec_workload_contract_v2::is_mainnet_activated() {
    xactivation_record record;

    std::string value_str = STRING_GET2(xstake::XPORPERTY_CONTRACT_GENESIS_STAGE_KEY, sys_contract_rec_registration_addr);
    if (!value_str.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(),
                    (uint8_t*)value_str.c_str(), (uint32_t)value_str.size());
        record.serialize_from(stream);
    }
    xdbg("[xzec_workload_contract::is_mainnet_activated] activated: %d, pid:%d\n", record.activated, getpid());
    return record.activated;
};

std::vector<base::xauto_ptr<data::xblock_t>> xzec_workload_contract_v2::get_fullblock(common::xaccount_address_t const& owner, uint64_t last_read_height, uint64_t & cur_read_height) {
    std::vector<base::xauto_ptr<data::xblock_t>> res;
    auto blockchain_height = get_blockchain_height(owner.value());
    for (auto i = last_read_height + 1; i <= blockchain_height; ++i) {
        base::xauto_ptr<data::xblock_t> tableblock = get_block_by_height(owner.value(), i);
        if (tableblock->is_fulltable()) {
            xdbg("[xtable_workload_contract::get_next_fulltableblock] tableblock owner: %s, height: %" PRIu64, owner.value().c_str(), i);
            res.push_back(std::move(tableblock));
            cur_read_height = i;
        }
    }

    return res;
}

void xzec_workload_contract_v2::migrate_data() {
    std::map<std::string, std::string> auditor_clusters_workloads;
    std::map<std::string, std::string> validator_clusters_workloads;
    MAP_COPY_GET(XPORPERTY_CONTRACT_WORKLOAD_KEY, auditor_clusters_workloads, sys_contract_zec_workload_addr);   
    MAP_COPY_GET(XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY, validator_clusters_workloads, sys_contract_zec_workload_addr);  
    for (auto const & it : auditor_clusters_workloads) {
        MAP_SET(XPORPERTY_CONTRACT_WORKLOAD_KEY, it.first, it.second);
    }
    for (auto const & it : validator_clusters_workloads) {
        MAP_SET(XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY, it.first, it.second);
    }
}

uint64_t xzec_workload_contract_v2::get_table_height(common::xaccount_address_t const & table) {
    uint64_t last_read_height = 0;
    std::string value_str;
    
    XMETRICS_TIME_RECORD("sysContract_zecWorkload_get_property_contract_fulltableblock_num_key");

    if (MAP_FIELD_EXIST(XPROPERTY_CONTRACT_LAST_READ_TABLE_BLOCK_HEIGHT, table.value())) {
        value_str = MAP_GET(XPROPERTY_CONTRACT_LAST_READ_TABLE_BLOCK_HEIGHT, table.value());
        XCONTRACT_ENSURE(!value_str.empty(), "read full tableblock height empty");
    }
 
    if (!value_str.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
        stream >> last_read_height;
    }    
    
    return last_read_height;
}

void xzec_workload_contract_v2::update_table_height(common::xaccount_address_t const & table, const uint64_t cur_read_height) {
    XMETRICS_TIME_RECORD("sysContract_zecWorkload_set_property_contract_fulltableblock_num_key");
    base::xstream_t stream{base::xcontext_t::instance()};
    stream << cur_read_height;
    MAP_SET(XPROPERTY_CONTRACT_LAST_READ_TABLE_BLOCK_HEIGHT, table.value(), std::string((char *)stream.data(), stream.size()));
}

void xzec_workload_contract_v2::on_receive_workload2(std::string const& workload_str) {
    XMETRICS_COUNTER_INCREMENT(XWORKLOAD_CONTRACT "on_receive_workload2_Called", 1);
    XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "on_receive_workload2_ExecutionTime");
    auto const& source_address = SOURCE_ADDRESS();

    std::string base_addr;
    uint32_t    table_id;
    if (!data::xdatautil::extract_parts(source_address, base_addr, table_id) || sys_contract_sharding_workload_addr != base_addr) {
        xwarn("[xzec_workload_contract::on_receive_workload] invalid call from %s", source_address.c_str());
        return;
    }

    xstream_t stream(xcontext_t::instance(), (uint8_t*)workload_str.data(), workload_str.size());
    std::map<common::xcluster_address_t, xauditor_workload_info_t> auditor_workload_info;
    std::map<common::xcluster_address_t, xvalidator_workload_info_t> validator_workload_info;
    int64_t table_pledge_balance_change_tgas = 0;

    MAP_OBJECT_DESERIALZE2(stream, auditor_workload_info);
    MAP_OBJECT_DESERIALZE2(stream, validator_workload_info);
    stream >> table_pledge_balance_change_tgas;
    xdbg("[xzec_workload_contract::on_receive_workload] pid:%d, SOURCE_ADDRESS: %s, auditor_workload_info size: %zu, validator_workload_info size: %zu, table_pledge_balance_change_tgas: %lld\n", getpid(), source_address.c_str(), auditor_workload_info.size(), validator_workload_info.size(), table_pledge_balance_change_tgas);

    // update system total tgas
    update_tgas(table_pledge_balance_change_tgas);

    if ( !is_mainnet_activated() ) return;

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

    XMETRICS_COUNTER_INCREMENT(XWORKLOAD_CONTRACT "on_receive_workload2_Executed", 1);
}

void xzec_workload_contract_v2::add_cluster_workload(bool auditor, std::string const& cluster_id, std::map<std::string, uint32_t> const& leader_count) {
    const char* property;
    if (auditor) {
        property = XPORPERTY_CONTRACT_WORKLOAD_KEY;
    } else {
        property = XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY;
    }
    cluster_workload_t workload;
    std::string value_str;
    int32_t ret;
    if (auditor) {
        XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "XPORPERTY_CONTRACT_WORKLOAD_KEY_GetExecutionTime");
        ret = MAP_GET2(property, cluster_id, value_str);
    } else {
        XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY_GetExecutionTime");
        ret = MAP_GET2(property, cluster_id, value_str);
    }

    if (ret) {
        xdbg("[xzec_workload_contract::add_cluster_workload] cluster_id not exist, auditor: %d\n", auditor);
        workload.cluster_id = cluster_id;
    } else {
        xstream_t stream(xcontext_t::instance(), (uint8_t*)value_str.data(), value_str.size());
        workload.serialize_from(stream);
    }

    common::xcluster_address_t cluster;
    xstream_t key_stream(xcontext_t::instance(), (uint8_t*)cluster_id.data(), cluster_id.size());
    key_stream >> cluster;
    for (auto const& leader_count_info : leader_count) {
        auto const& leader  = leader_count_info.first;
        auto const& count   = leader_count_info.second;

        workload.m_leader_count[leader] += count;
        workload.cluster_total_workload += count;
        xdbg("[xzec_workload_contract::add_cluster_workload] auditor: %d, cluster_id: %u, leader: %s, count: %d, total_workload: %d\n", auditor, cluster.group_id().value(), leader_count_info.first.c_str(), workload.m_leader_count[leader], workload.cluster_total_workload);
    }

    xstream_t stream(xcontext_t::instance());
    workload.serialize_to(stream);
    std::string value = std::string((const char*)stream.data(), stream.size());
    if (auditor) {
        XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "XPORPERTY_CONTRACT_WORKLOAD_KEY_SetExecutionTime");
        MAP_SET(property, cluster_id, value);
    } else {
        XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY_SetExecutionTime");
        MAP_SET(property, cluster_id, value);
    }
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

bool xzec_workload_contract_v2::calc_table_block_status(const uint64_t cur_time,
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

bool xzec_workload_contract_v2::read_table_block_status(common::xaccount_address_t const & table, const uint64_t cur_time, const uint64_t cur_height) {
    bool status{false};

    auto const last_read_height = static_cast<std::uint64_t>(std::stoull(MAP_GET(XPROPERTY_CONTRACT_LAST_READ_TABLE_BLOCK_HEIGHT, table.value())));
    auto const last_read_time = static_cast<std::uint64_t>(std::stoull(MAP_GET(XPROPERTY_CONTRACT_LAST_READ_TABLE_BLOCK_TIME, table .value())));

    xdbg("[xzec_workload_contract_v2::update_table_block_read_status] cur_time: %llu, last_read_time: %llu, latest_height: %llu, last_read_height: %llu",
         cur_time,
         last_read_time,
         cur_height,
         last_read_height);
    XCONTRACT_ENSURE(cur_height >= last_read_height, u8"xzec_workload_contract_v2::update_table_block_read_status table %s, cur_height < last_read_height");
    if (cur_height == last_read_height) {
        XMETRICS_PACKET_INFO(XWORKLOAD_CONTRACT "update_status", "next_read_height", last_read_height, "current_time", cur_time)
        MAP_SET(XPROPERTY_CONTRACT_LAST_READ_TABLE_BLOCK_TIME, table.value(), std::to_string(cur_time));
        return false;
    }
    auto const height_step_limitation = XGET_ONCHAIN_GOVERNANCE_PARAMETER(cross_reading_rec_reg_contract_height_step_limitation);
    auto const timeout_limitation = XGET_ONCHAIN_GOVERNANCE_PARAMETER(cross_reading_rec_reg_contract_logic_timeout_limitation);
    uint64_t next_read_height = last_read_height;
    status =
        calc_table_block_status(cur_time, last_read_time, last_read_height, cur_height, height_step_limitation, timeout_limitation, next_read_height);
    xinfo("[xzec_reward_contract::update_reg_contract_read_status] next_read_height: %" PRIu64 ", latest_height: %llu, update_rec_reg_contract_read_status: %d",
          next_read_height,
          cur_height,
          status);

    if (status) {
        XMETRICS_PACKET_INFO(XWORKLOAD_CONTRACT "update_status", "next_read_height", next_read_height, "current_time", cur_time)
        MAP_SET(XPROPERTY_CONTRACT_LAST_READ_TABLE_BLOCK_HEIGHT, table.value(), std::to_string(next_read_height));
        MAP_SET(XPROPERTY_CONTRACT_LAST_READ_TABLE_BLOCK_TIME, table.value(), std::to_string(cur_time));
    }
    return status;
}

void xzec_workload_contract_v2::add_workload_with_fullblock() {
    xdbg("[xzec_workload_contract_v2::calc_table_workload] enum_vbucket_has_tables_count %d", enum_vledger_const::enum_vbucket_has_tables_count);
    for (auto i = 0; i < enum_vledger_const::enum_vbucket_has_tables_count; i++) {
        std::map<common::xcluster_address_t, xauditor_workload_info_t> bookload_auditor_group_workload_info;
        std::map<common::xcluster_address_t, xvalidator_workload_info_t> bookload_validator_group_workload_info;
        int64_t table_pledge_balance_change_tgas = 0;
        int total_table_block_count = 0;
        // calc table address
        auto table_owner = common::xaccount_address_t{xdatautil::serialize_owner_str(sys_contract_sharding_table_block_addr, i)};
        // calc table address height
        uint64_t cur_read_height = 0;
        uint64_t old_version_height = 0;
        uint64_t last_read_height = get_table_height(table_owner);
        // get table block
        auto full_blocks = get_fullblock(table_owner, last_read_height, cur_read_height);
        // update table address height
        if(cur_read_height > last_read_height) {
            update_table_height(table_owner, cur_read_height);
        }
        xdbg("[xzec_workload_contract_v2::add_workload_with_fullblock] table owner address: %s, last height: %lu, cur height: %lu\n", table_owner.c_str(), last_read_height, cur_read_height);
        // accumulate workload
        for (std::size_t block_index = 0; block_index < full_blocks.size(); block_index++) {
            xfull_tableblock_t* full_tableblock = dynamic_cast<xfull_tableblock_t*>(full_blocks[block_index].get());
            assert(full_tableblock != nullptr);
            auto stat_data = full_tableblock->get_table_statistics();

            auto node_service = contract::xcontract_manager_t::instance().get_node_service();
            for (auto const static_item: stat_data.detail) {
                auto elect_statistic = static_item.second;
                for (auto const group_item: elect_statistic.group_statistics_data) {
                    common::xgroup_address_t const & group_addr = group_item.first;
                    xvip2_t group_xvip2 = top::common::xip2_t{group_addr.network_id(), group_addr.zone_id(), group_addr.cluster_id(), group_addr.group_id()};
                    common::xcluster_address_t cluster{common::xip_t{group_xvip2.low_addr}};
                    xdbg("[xzec_workload_contract_v2::add_workload_with_fullblock] group xvip2: %llu, %llu",
                        group_xvip2.high_addr,
                        group_xvip2.low_addr);
                    xgroup_related_statistics_data_t group_account_data = group_item.second;
                    auto id = group_addr.group_id();
                    if (id.value() >= common::xauditor_group_id_value_begin && id.value() < common::xauditor_group_id_value_end) {
                        auto it2 = bookload_auditor_group_workload_info.find(cluster);
                        if (it2 == bookload_auditor_group_workload_info.end()) {
                            xauditor_workload_info_t auditor_workload_info;
                            auto ret = bookload_auditor_group_workload_info.insert(std::make_pair(cluster, auditor_workload_info));
                            XCONTRACT_ENSURE(ret.second, "insert auditor workload failed");
                            it2 = ret.first;
                        }
                        for (size_t slotid = 0; slotid < group_account_data.account_statistics_data.size(); ++slotid) {
                            uint32_t workload_per_tableblock = group_account_data.account_statistics_data[slotid].block_data.block_count;
                            uint32_t workload_per_tx = group_account_data.account_statistics_data[slotid].block_data.transaction_count;
                            uint32_t workload = workload_per_tableblock + full_tableblock->get_txs_count() * workload_per_tx;
                            auto account_addr = node_service->get_group(group_xvip2)->get_node(slotid)->get_account();
                            it2->second.m_leader_count[account_addr] += workload;
                            xdbg(
                                "[xzec_workload_contract_v2::add_workload_with_fullblock] cluster: [%s, network_id: %u, zone_id: %u, cluster_id: %u, group_id: %u], leader: %s, workload: %u, tx_count: "
                                "%u, , workload_per_tableblock: %u, workload_per_tx: %u",
                                cluster.to_string().c_str(),
                                cluster.network_id().value(),
                                cluster.zone_id().value(),
                                cluster.cluster_id().value(),
                                cluster.group_id().value(),
                                account_addr.c_str(),
                                workload,
                                full_tableblock->get_txs_count(),
                                workload_per_tableblock,
                                workload_per_tx);
                        }
                    } else if (id.value() >= common::xvalidator_group_id_value_begin && id.value() < common::xvalidator_group_id_value_end) {
                        auto it2 = bookload_validator_group_workload_info.find(cluster);
                        if (it2 == bookload_validator_group_workload_info.end()) {
                            xvalidator_workload_info_t validator_workload_info;
                            auto ret = bookload_validator_group_workload_info.insert(std::make_pair(cluster, validator_workload_info));
                            XCONTRACT_ENSURE(ret.second, "insert auditor workload failed");
                            it2 = ret.first;
                        }
                        for (size_t slotid = 0; slotid < group_account_data.account_statistics_data.size(); ++slotid) {
                            uint32_t workload_per_tableblock = group_account_data.account_statistics_data[slotid].block_data.block_count;
                            uint32_t workload_per_tx = group_account_data.account_statistics_data[slotid].block_data.transaction_count;
                            uint32_t workload = workload_per_tableblock + full_tableblock->get_txs_count() * workload_per_tx;
                            auto account_addr = node_service->get_group(group_xvip2)->get_node(slotid)->get_account();
                            it2->second.m_leader_count[account_addr] += workload;
                            xdbg(
                                "[xzec_workload_contract_v2::add_workload_with_fullblock] cluster: [%s, network_id: %u, zone_id: %u, cluster_id: %u, group_id: %u], leader: %s, workload: %u, tx_count: "
                                "%u, , workload_per_tableblock: %u, workload_per_tx: %u",
                                cluster.to_string().c_str(),
                                cluster.network_id().value(),
                                cluster.zone_id().value(),
                                cluster.cluster_id().value(),
                                cluster.group_id().value(),
                                account_addr.c_str(),
                                workload,
                                full_tableblock->get_txs_count(),
                                workload_per_tableblock,
                                workload_per_tx);
                        }
                    } else { 
                        // invalid group
                        xwarn("[xzec_workload_contract_v2::add_workload_with_fullblock] invalid group id: %d", group_addr.group_id().value());
                        continue;
                    }
                }
            }
            // m_table_pledge_balance_change_tgas
            table_pledge_balance_change_tgas += full_tableblock->get_pledge_balance_change_tgas();
            total_table_block_count++;
            xdbg("[xzec_workload_contract_v2::add_workload_with_fullblock] total_table_block_count: %u", total_table_block_count);
        }

        if (full_blocks.size() >  0) {
            xinfo(
                "[xzec_workload_contract_v2::add_workload_with_fullblock] timer round: %" PRIu64 ", pid: %d, total_table_block_count: %d, table_pledge_balance_change_tgas: %lld, "
                "this: %p\n",
                TIME(),
                getpid(),
                total_table_block_count,
                table_pledge_balance_change_tgas,
                this);
            XMETRICS_PACKET_INFO("sysContract_zecWorkload2", "effective report timer round", std::to_string(TIME()));

            for (auto & entity : bookload_auditor_group_workload_info) {
                auto const & group = entity.first;
                for (auto const & wl : entity.second.m_leader_count) {
                    xdbg("[xzec_workload_contract_v2::add_workload_with_fullblock] workload auditor group: %s, leader: %s, workload: %u", group.to_string().c_str(), wl.first.c_str(), wl.second);
                }
                xdbg("[xzec_workload_contract_v2::add_workload_with_fullblock] workload auditor group: %s, group id: %u, ends", group.to_string().c_str(), group.group_id().value());
            }

            for (auto & entity : bookload_validator_group_workload_info) {
                auto const & group = entity.first;
                for (auto const & wl : entity.second.m_leader_count) {
                    xdbg("[xzec_workload_contract_v2::add_workload_with_fullblock] workload validator group: %s, leader: %s, workload: %u", group.to_string().c_str(), wl.first.c_str(), wl.second);
                }
                xdbg("[xzec_workload_contract_v2::add_workload_with_fullblock] workload validator group: %s, group id: %u, ends", group.to_string().c_str(), group.group_id().value());
            }

            xstream_t stream(xcontext_t::instance());
            MAP_OBJECT_SERIALIZE2(stream, bookload_auditor_group_workload_info);
            MAP_OBJECT_SERIALIZE2(stream, bookload_validator_group_workload_info);
            stream << table_pledge_balance_change_tgas;

            std::string workload_str = std::string((char *)stream.data(), stream.size());
            on_receive_workload2(workload_str);
        }
    }
}

void xzec_workload_contract_v2::on_timer(common::xlogic_time_t const timestamp) {
    XMETRICS_TIME_RECORD("sysContract_zecWorkload2_on_timer");
    // check address
    auto const & self_account = SELF_ADDRESS();
    auto const & source_address = SOURCE_ADDRESS();
    if (self_account.value() != source_address) {
        xwarn("[xzec_workload_contract_v2::on_timer] invalid call from %s", source_address.c_str());
        return;
    }
    // check mainnet
    if (!is_mainnet_activated()) {
        return;
    }
    // check fork
    auto const & fork_config = chain_upgrade::xchain_fork_config_center_t::chain_fork_config();
    if (!chain_upgrade::xchain_fork_config_center_t::is_forked(fork_config.slash_workload_contract_upgrade, timestamp)) {
        return;
    }
    // check data migration
    if (xstring_utl::toint32(STRING_GET(XPROPERTY_CONTRACT_WORKLOAD_DATA_MIGRATION_FLAG)) != 1) {
        migrate_data();
        STRING_SET(XPROPERTY_CONTRACT_WORKLOAD_DATA_MIGRATION_FLAG, xstring_utl::tostring(1));
    }

    add_workload_with_fullblock();

    std::map<std::string, std::string> auditor_clusters_workloads;
    std::map<std::string, std::string> validator_clusters_workloads;

    // auditor workload
    MAP_COPY_GET(XPORPERTY_CONTRACT_WORKLOAD_KEY, auditor_clusters_workloads);
    // validator workload
    MAP_COPY_GET(XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY, validator_clusters_workloads);

    //std::map<std::string, std::string> auditor_clusters_workloads2;
    //std::map<std::string, std::string> validator_clusters_workloads2;
    std::map<common::xcluster_address_t, xauditor_workload_info_t> auditor_clusters_workloads2;
    std::map<common::xcluster_address_t, xvalidator_workload_info_t> validator_clusters_workloads2;
    int count = 0;
    auto cur_time = TIME();

    const int upload_cnt = 1000;
    for (auto it = auditor_clusters_workloads.begin(); it != auditor_clusters_workloads.end(); it++) {
        xstream_t stream(xcontext_t::instance(), (uint8_t*)it->second.data(), it->second.size());
        cluster_workload_t workload;
        workload.serialize_from(stream);

        common::xcluster_address_t cluster_id2;
        {

            xstream_t stream(xcontext_t::instance(), (uint8_t*)it->first.data(), it->first.size());
            stream >> cluster_id2;
            xdbg("[xzec_workload_contract_v2::on_timer] auditor, cluster_id: %s, group size: %d",
                cluster_id2.to_string().c_str(), workload.m_leader_count.size());
        }
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

                    xinfo("[xzec_workload_contract_v2::on_timer] report workload to zec reward, auditor_clusters_workloads size: %d, validator_clusters_workloads size: %d, timer round: %" PRIu64 ", cur_time: %llu, count: %d, round: %d",
                        auditor_clusters_workloads2.size(),
                        validator_clusters_workloads2.size(),
                        timestamp,
                        cur_time,
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

        for (auto it2 = validator_clusters_workloads.begin(); it2 != validator_clusters_workloads.end(); it2++) {
            //validator_clusters_workloads[it2->first] = it2->second;
            xstream_t stream(xcontext_t::instance(), (uint8_t*)it2->second.data(), it2->second.size());
            cluster_workload_t workload;
            workload.serialize_from(stream);
            common::xcluster_address_t cluster_id2;
            {
                xstream_t stream(xcontext_t::instance(), (uint8_t*)it2->first.data(), it2->first.size());
                stream >> cluster_id2;
                xdbg("[xzec_workload_contract_v2::on_timer] validator, cluster_id: %s, group size: %d",
                    cluster_id2.to_string().c_str(), workload.m_leader_count.size());
            }
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

                        xinfo("[xzec_workload_contract_v2::on_timer] report workload to zec reward, auditor_clusters_workloads size: %d, validator_clusters_workloads size: %d, timer round: %" PRIu64 ", cur_time: %llu, count: %d, round: %d",
                            auditor_clusters_workloads2.size(),
                            validator_clusters_workloads2.size(),
                            timestamp,
                            cur_time,
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
        if (auditor_clusters_workloads2.size() > 0 || validator_clusters_workloads.size() > 0) {
            xinfo("[xzec_workload_contract_v2::on_timer] report workload to zec reward, auditor_clusters_workloads size: %d, validator_clusters_workloads size: %d, timer round: %" PRIu64 ", cur_time: %llu, count: %d, round: %d",
                auditor_clusters_workloads.size(),
                validator_clusters_workloads.size(),
                timestamp,
                cur_time,
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

    clear_workload();
}

void xzec_workload_contract_v2::clear_workload() {
    XMETRICS_TIME_RECORD("zec_workload_clear_workload_all_time");

    {
        XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "XPORPERTY_CONTRACT_WORKLOAD_KEY_SetExecutionTime");
        CLEAR(enum_type_t::map, XPORPERTY_CONTRACT_WORKLOAD_KEY);
    }
    {
        XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY_SetExecutionTime");
        CLEAR(enum_type_t::map, XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY);
    }
}

NS_END2

#undef XWORKLOAD_CONTRACT
#undef XCONTRACT_PREFIX
#undef XZEC_MODULE
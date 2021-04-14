// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xreward/xzec_workload_contract.h"

#include "xstore/xstore_error.h"
#include "xbasic/xutility.h"
#include "xchain_upgrade/xchain_upgrade_center.h"
#include "xvm/manager/xcontract_manager.h"
#include "xdata/xblock_statistics_data.h"
#include "xcommon/xip.h"
#include "xdata/xgenesis_data.h"
#include "xdata/xworkload_info.h"
#include "xbase/xutl.h"
#include "xstake/xstake_algorithm.h"
#include <iomanip>

using top::base::xstream_t;
using top::base::xcontext_t;
using top::base::xstring_utl;
using namespace top::data;

#if !defined (XZEC_MODULE)
#define XZEC_MODULE "sysContract_"
#endif

#define XCONTRACT_PREFIX "workload_"

#define XWORKLOAD_CONTRACT XZEC_MODULE XCONTRACT_PREFIX

NS_BEG2(top, xstake)

xzec_workload_contract::xzec_workload_contract(common::xnetwork_id_t const & network_id)
    : xbase_t{ network_id } {
}

void xzec_workload_contract::setup() {
    MAP_CREATE(XPORPERTY_CONTRACT_WORKLOAD_KEY); // accumulate auditor cluster workload
    MAP_CREATE(XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY); // accumulate validator cluster workload

    // save system total tgas
    // STRING_CREATE(XPORPERTY_CONTRACT_TGAS_KEY);
}

int xzec_workload_contract::is_mainnet_activated() {
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

void xzec_workload_contract::on_receive_workload2(std::string const& workload_str) {
    auto fork_config = chain_upgrade::xchain_fork_config_center_t::chain_fork_config();
	if (chain_upgrade::xchain_fork_config_center_t::is_forked(fork_config.slash_workload_contract_upgrade, TIME())) {
        return;
    }
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

void xzec_workload_contract::add_cluster_workload(bool auditor, std::string const& cluster_id, std::map<std::string, uint32_t> const& leader_count) {
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

void xzec_workload_contract::update_tgas(int64_t table_pledge_balance_change_tgas) {
    std::string pledge_tgas_str;
    {
        XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "XPORPERTY_CONTRACT_TGAS_KEY_GetExecutionTime");
        pledge_tgas_str = STRING_GET2(XPORPERTY_CONTRACT_TGAS_KEY);
    }
    int64_t tgas = 0;
    if (!pledge_tgas_str.empty()) {
        tgas = base::xstring_utl::toint64(pledge_tgas_str);
    }
    tgas += table_pledge_balance_change_tgas;

    {
        XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "XPORPERTY_CONTRACT_TGAS_KEY_SetExecutionTime");
        STRING_SET(XPORPERTY_CONTRACT_TGAS_KEY, xstring_utl::tostring(tgas));
    }
}

std::vector<base::xauto_ptr<data::xblock_t>> xzec_workload_contract::get_next_fulltableblock(common::xaccount_address_t const& owner, uint64_t last_read_height) {
    std::vector<base::xauto_ptr<data::xblock_t>> res;
    auto blockchain_height = get_blockchain_height(owner.value());
    for (auto i = last_read_height + 1; i <= blockchain_height; ++i) {
        base::xauto_ptr<data::xblock_t> tableblock = get_block_by_height(owner.value(), i);
        if (tableblock->is_fulltable()) {
            xdbg("[xtable_workload_contract::get_next_fulltableblock] tableblock owner: %s, height: %" PRIu64, owner.value().c_str(), i);
            res.push_back(std::move(tableblock));
        }
    }

    return res;
}

bool xzec_workload_contract::is_validtor_group(common::xgroup_id_t const& id) {
    return id.value() >= common::xvalidator_group_id_value_begin && id.value() < common::xvalidator_group_id_value_end;

}
bool xzec_workload_contract::is_auditor_group(common::xgroup_id_t const& id) {
    return id.value() >= common::xauditor_group_id_value_begin && id.value() < common::xauditor_group_id_value_end;
}

uint64_t xzec_workload_contract::get_table_height(common::xaccount_address_t const & account) {
    uint64_t last_read_height = 0;
    std::string value_str;
    
    XMETRICS_TIME_RECORD("sysContract_zecWorkload_get_property_contract_fulltableblock_num_key");

    if (!MAP_PROPERTY_EXIST(XPROPERTY_CONTRACT_TABLEBLOCK_NUM_KEY)) {
        MAP_CREATE(XPROPERTY_CONTRACT_TABLEBLOCK_NUM_KEY);
    }

    if (MAP_FIELD_EXIST(xstake::XPROPERTY_CONTRACT_TABLEBLOCK_NUM_KEY, account.value())) {
        value_str = MAP_GET(xstake::XPROPERTY_CONTRACT_TABLEBLOCK_NUM_KEY, account.value());
        XCONTRACT_ENSURE(!value_str.empty(), "read full tableblock height empty");
    }
 
    if (!value_str.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
        stream >> last_read_height;
    }    
    
    return last_read_height;
}

void xzec_workload_contract::update_table_height(common::xaccount_address_t const & account, uint64_t cur_read_height) {
    XMETRICS_TIME_RECORD("sysContract_zecWorkload_set_property_contract_fulltableblock_num_key");
    base::xstream_t stream{base::xcontext_t::instance()};
    stream << cur_read_height;
    MAP_SET(xstake::XPROPERTY_CONTRACT_TABLEBLOCK_NUM_KEY, account.to_string(), std::string((char *)stream.data(), stream.size()));
}

void xzec_workload_contract::calc_table_workload() {
    std::map<common::xcluster_address_t, xauditor_workload_info_t> bookload_auditor_group_workload_info;
    std::map<common::xcluster_address_t, xvalidator_workload_info_t> bookload_validator_group_workload_info;
    int64_t table_pledge_balance_change_tgas = 0;
    int total_table_block_count = 0;
    xdbg("[xzec_workload_contract::calc_table_workload] enum_vbucket_has_tables_count %d", enum_vledger_const::enum_vbucket_has_tables_count);
    for (auto i = 0; i < enum_vledger_const::enum_vbucket_has_tables_count; ++i) {
        auto contract_address = common::xaccount_address_t{CALC_CONTRACT_ADDRESS(sys_contract_sharding_table_block_addr, i)};
        auto last_read_height = get_table_height(contract_address);
        auto full_blocks = get_next_fulltableblock(contract_address, last_read_height);
        if(full_blocks.size() > 0) {
            update_table_height(contract_address, last_read_height + full_blocks.size());
        }
        for (std::size_t block_index = 0; block_index < full_blocks.size(); ++block_index) {
            xfull_tableblock_t* full_tableblock = dynamic_cast<xfull_tableblock_t*>(full_blocks[block_index].get());
            assert(full_tableblock != nullptr);
            auto fulltable_statisitc_resource = full_tableblock->get_fulltable_statistics_resource();
            if (fulltable_statisitc_resource == nullptr) {
                continue;
            }
            auto fulltable_statisitc_data = fulltable_statisitc_resource->get_statistics_data();
            data::xstatistics_data_t stat_data;
            data::xstream_t stream(base::xcontext_t::instance(), fulltable_statisitc_data.data(), fulltable_statisitc_data.size());
            stream >> stat_data;

            auto node_service = contract::xcontract_manager_t::instance().get_node_service();
            for (auto const static_item: stat_data.detail) {
                auto elect_statistic = static_item.second;
                for (auto const group_item: elect_statistic.group_statistics_data) {
                    common::xgroup_address_t const & group_addr = group_item.first;
                    xvip2_t group_xvip2 = top::common::xip2_t{group_addr.network_id(), group_addr.zone_id(), group_addr.cluster_id(), group_addr.group_id()};
                    xdbg("[xzec_workload_contract::calc_table_workload] xvip2: %llu, %llu, table_owner: %s, height: %llu",
                        group_xvip2.high_addr,
                        group_xvip2.low_addr,
                        last_read_height);
                    common::xcluster_address_t cluster{common::xip_t{group_xvip2.low_addr}};
                    xgroup_related_statistics_data_t group_account_data = group_item.second;
                    if (is_auditor_group(group_addr.group_id())) {
                        auto it2 = bookload_auditor_group_workload_info.find(cluster);
                        if (it2 == bookload_auditor_group_workload_info.end()) {
                            xauditor_workload_info_t auditor_workload_info;
                            std::pair<std::map<common::xcluster_address_t, xauditor_workload_info_t>::iterator, bool> ret =
                                bookload_auditor_group_workload_info.insert(std::make_pair(cluster, auditor_workload_info));
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
                                "[xzec_workload_contract::calc_table_workload] cluster: [%s, network_id: %u, zone_id: %u, cluster_id: %u, group_id: %u], leader: %s, workload: %u, tx_count: "
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
                    } else if (is_validtor_group(group_addr.group_id())) {
                        auto it2 = bookload_validator_group_workload_info.find(cluster);
                        if (it2 == bookload_validator_group_workload_info.end()) {
                            xvalidator_workload_info_t validator_workload_info;
                            std::pair<std::map<common::xcluster_address_t, xvalidator_workload_info_t>::iterator, bool> ret =
                                bookload_validator_group_workload_info.insert(std::make_pair(cluster, validator_workload_info));
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
                                "[xzec_workload_contract::calc_table_workload] cluster: [%s, network_id: %u, zone_id: %u, cluster_id: %u, group_id: %u], leader: %s, workload: %u, tx_count: "
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
                        xwarn("[xzec_workload_contract][calc_table_workload] invalid group id: %d", group_addr.group_id().value());
                        continue;
                    }
                }
            }
            // m_table_pledge_balance_change_tgas
            table_pledge_balance_change_tgas += full_tableblock->get_pledge_balance_change_tgas();
            total_table_block_count++;
            xdbg("[xzec_workload_contract::calc_table_workload] total_table_block_count: %u", total_table_block_count);
        }

        if (full_blocks.size() >  0) {
            xinfo(
                "[xzec_workload_contract::calc_table_workload] timer round: %" PRIu64 ", pid: %d, total_table_block_count: %d, table_pledge_balance_change_tgas: %lld, "
                "this: %p\n",
                TIME(),
                getpid(),
                total_table_block_count,
                table_pledge_balance_change_tgas,
                this);
            XMETRICS_PACKET_INFO("sysContract_zecWorkload", "effective report timer round", std::to_string(TIME()));

            for (auto & entity : bookload_auditor_group_workload_info) {
                auto const & group = entity.first;
                for (auto const & wl : entity.second.m_leader_count) {
                    xdbg("[xzec_workload_contract::calc_table_workload] workload auditor group: %s, leader: %s, workload: %u", group.to_string().c_str(), wl.first.c_str(), wl.second);
                }
                xdbg("[xzec_workload_contract::calc_table_workload] workload auditor group: %s, group id: %u, ends", group.to_string().c_str(), group.group_id().value());
            }

            for (auto & entity : bookload_validator_group_workload_info) {
                auto const & group = entity.first;
                for (auto const & wl : entity.second.m_leader_count) {
                    xdbg("[xzec_workload_contract::calc_table_workload] workload validator group: %s, leader: %s, workload: %u", group.to_string().c_str(), wl.first.c_str(), wl.second);
                }
                xdbg("[xzec_workload_contract::calc_table_workload] workload validator group: %s, group id: %u, ends", group.to_string().c_str(), group.group_id().value());
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

void xzec_workload_contract::on_timer(common::xlogic_time_t const timestamp) {
    if (!is_mainnet_activated()) return;

    /*{
        // test
        common::xnetwork_id_t   net_id{0};
        common::xzone_id_t      zone_id{0};
        common::xcluster_id_t   cluster_id{1};
        common::xgroup_id_t     group_id{0};

        group_id = common::xgroup_id_t{1};
        common::xcluster_address_t cluster{net_id, zone_id, cluster_id, group_id};

        std::map<common::xcluster_address_t, xauditor_workload_info_t> bookload_auditor_group_workload_info;
        std::map<common::xcluster_address_t, xvalidator_workload_info_t> bookload_validator_group_workload_info;
        int64_t table_pledge_balance_change_tgas = 0;
        std::string leader1 = "T00000LWUw2ioaCw3TYJ9Lsgu767bbNpmj75kv73";
        std::string leader2 = "T00000LTHfpc9otZwKmNcXA24qiA9A6SMHKkxwkg";

        auto it2 = bookload_auditor_group_workload_info.find(cluster);
        if (it2 == bookload_auditor_group_workload_info.end()) {
            xauditor_workload_info_t auditor_workload_info;
            std::pair<std::map<common::xcluster_address_t, xauditor_workload_info_t>::iterator,bool> ret =
                        bookload_auditor_group_workload_info.insert(std::make_pair(cluster, auditor_workload_info));
            it2 = ret.first;
        }
        uint32_t    txs_count = 149;
        it2->second.m_leader_count[leader1] += txs_count;
        it2->second.m_leader_count[leader2] += txs_count;
        for (auto i = 0; i < 700; i++) {
            std::stringstream ss;
            ss << std::setw(40) << std::setfill('0') << i;
            auto key = ss.str();
            it2->second.m_leader_count[key] += txs_count;
        }

        group_id = common::xgroup_id_t{64};
        cluster = common::xcluster_address_t{net_id, zone_id, cluster_id, group_id};
        auto it = bookload_validator_group_workload_info.find(cluster);
        if (it == bookload_validator_group_workload_info.end()) {
            xvalidator_workload_info_t validator_workload_info;
            std::pair<std::map<common::xcluster_address_t, xvalidator_workload_info_t>::iterator,bool> ret =
                        bookload_validator_group_workload_info.insert(std::make_pair(cluster, validator_workload_info));
            it = ret.first;
        }
        it->second.m_leader_count[leader1] += txs_count;
        it->second.m_leader_count[leader2] += txs_count;
        for (auto i = 0; i < 700; i++) {
            std::stringstream ss;
            ss << std::setw(40) << std::setfill('0') << i;
            auto key = ss.str();
            it->second.m_leader_count[key] += txs_count;
        }

        xstream_t stream(xcontext_t::instance());
        MAP_OBJECT_SERIALIZE2(stream, bookload_auditor_group_workload_info);
        MAP_OBJECT_SERIALIZE2(stream, bookload_validator_group_workload_info);
        stream << table_pledge_balance_change_tgas;

        std::string workload_str = std::string((char *)stream.data(), stream.size());
        on_receive_workload2(workload_str);
    }*/
    auto fork_config = chain_upgrade::xchain_fork_config_center_t::chain_fork_config();
    if (chain_upgrade::xchain_fork_config_center_t::is_forked(fork_config.slash_workload_contract_upgrade, timestamp)) {
        calc_table_workload();
    }

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
            xdbg("[xzec_workload_contract::on_timer] auditor, cluster_id: %s, group size: %d",
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

                    xinfo("[xzec_workload_contract::on_timer] report workload to zec reward, auditor_clusters_workloads size: %d, validator_clusters_workloads size: %d, timer round: %" PRIu64 ", cur_time: %llu, count: %d, round: %d",
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
                xdbg("[xzec_workload_contract::on_timer] validator, cluster_id: %s, group size: %d",
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

                        xinfo("[xzec_workload_contract::on_timer] report workload to zec reward, auditor_clusters_workloads size: %d, validator_clusters_workloads size: %d, timer round: %" PRIu64 ", cur_time: %llu, count: %d, round: %d",
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
            xinfo("[xzec_workload_contract::on_timer] report workload to zec reward, auditor_clusters_workloads size: %d, validator_clusters_workloads size: %d, timer round: %" PRIu64 ", cur_time: %llu, count: %d, round: %d",
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

void xzec_workload_contract::clear_workload() {
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

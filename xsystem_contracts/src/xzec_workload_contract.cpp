// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xreward/xzec_workload_contract.h"

#include "xstore/xstore_error.h"
#include "xbasic/xutility.h"
#include "xchain_upgrade/xchain_upgrade_center.h"
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

void xzec_workload_contract::on_timer(common::xlogic_time_t const timestamp) {
    auto const & fork_config = chain_upgrade::xchain_fork_config_center_t::chain_fork_config();
    if (chain_upgrade::xchain_fork_config_center_t::is_forked(fork_config.slash_workload_contract_upgrade, timestamp)) {
        return;
    }

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

    std::map<std::string, std::string> auditor_clusters_workloads;
    std::map<std::string, std::string> validator_clusters_workloads;

    // auditor workload
    MAP_COPY_GET(XPORPERTY_CONTRACT_WORKLOAD_KEY, auditor_clusters_workloads);
    // validator workload 
    MAP_COPY_GET(XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY, validator_clusters_workloads);
	if (chain_upgrade::xtop_chain_fork_config_center::is_forked(fork_config.reward_fork_spark, TIME())) {

        //std::map<std::string, std::string> auditor_clusters_workloads2;
        //std::map<std::string, std::string> validator_clusters_workloads2;
        std::map<common::xcluster_address_t, xauditor_workload_info_t> auditor_clusters_workloads2;
        std::map<common::xcluster_address_t, xvalidator_workload_info_t> validator_clusters_workloads2;
        int count = 0;
        auto cur_time = TIME();

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
                    std::pair<std::map<common::xcluster_address_t, xauditor_workload_info_t>::iterator, bool> ret =
                        auditor_clusters_workloads2.insert(std::make_pair(cluster_id2, auditor_workload_info));
                    it3 = ret.first;
                }
                it3->second.m_leader_count[leader] += work;

                if (++count % 1000 == 0) {
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
                            (count + 1000 - 1) / 1000);
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
                    std::pair<std::map<common::xcluster_address_t, xvalidator_workload_info_t>::iterator, bool> ret =
                        validator_clusters_workloads2.insert(std::make_pair(cluster_id2, validator_workload_info));
                    it3 = ret.first;
                }
                it3->second.m_leader_count[leader] += work;

                if (++count % 1000 == 0) {
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
                            (count + 1000 - 1) / 1000);
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
                (count + 1000 - 1) / 1000);
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
    } else if (chain_upgrade::xtop_chain_fork_config_center::is_forked(fork_config.reward_fork_point, TIME())) {
		xstream_t stream(xcontext_t::instance());
        MAP_SERIALIZE_SIMPLE(stream, auditor_clusters_workloads);
        MAP_SERIALIZE_SIMPLE(stream, validator_clusters_workloads);
		
		std::string workload_info = std::string((char *)stream.data(), stream.size());
        {
            xinfo("[xzec_workload_contract::on_timer] report workload to zec reward, auditor_clusters_workloads size: %d, validator_clusters_workloads size: %d, timer round: %" PRIu64,
                auditor_clusters_workloads.size(),
                validator_clusters_workloads.size(),
                timestamp);
            xstream_t stream(xcontext_t::instance());
            stream << timestamp;
            stream << workload_info;
            CALL(common::xaccount_address_t{sys_contract_zec_reward_addr}, "calculate_reward", std::string((char *)stream.data(), stream.size()));
        }
    } else {
        xstream_t stream(xcontext_t::instance());
        stream << auditor_clusters_workloads.empty();
        if(!auditor_clusters_workloads.empty()) MAP_SERIALIZE_SIMPLE(stream, auditor_clusters_workloads);
        stream << validator_clusters_workloads.empty();
        if(!validator_clusters_workloads.empty()) MAP_SERIALIZE_SIMPLE(stream, validator_clusters_workloads);
        std::string workload_info = std::string((char *)stream.data(), stream.size());
        {
            xinfo("[xzec_workload_contract::on_timer] report workload to zec reward, auditor_clusters_workloads size: %d, validator_clusters_workloads size: %d, timer round: %" PRIu64,
                auditor_clusters_workloads.size(),
                validator_clusters_workloads.size(),
                timestamp);
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

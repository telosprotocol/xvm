// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xslash/xtable_statistic_info_collection_contract.h"

#include "xbase/xmem.h"
#include "xcertauth/xcertauth_face.h"
#include "xcommon/xip.h"
#include "xdata/xdata_common.h"
#include "xdata/xnative_contract_address.h"
#include "xdata/xslash.h"
#include "xmetrics/xmetrics.h"
#include "xstake/xstake_algorithm.h"
#include "xvm/manager/xcontract_manager.h"
#include "xchain_upgrade/xchain_upgrade_center.h"

using namespace top::base;
using namespace top::data;
using namespace top::xstake;

NS_BEG3(top, xvm, xcontract)

#define FULLTABLE_NUM           "FULLTABLE_NUM"
#define FULLTABLE_HEIGHT        "FULLTABLE_HEIGHT"

xtable_statistic_info_collection_contract::xtable_statistic_info_collection_contract(common::xnetwork_id_t const & network_id) : xbase_t{network_id} {}

void xtable_statistic_info_collection_contract::setup() {
    // initialize map key
    MAP_CREATE(XPORPERTY_CONTRACT_WORKLOAD_KEY);
    MAP_CREATE(XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY);

    MAP_CREATE(XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY);
    MAP_SET(XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_NUM, "0");
    MAP_SET(XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_HEIGHT, "0");

    STRING_CREATE(XPORPERTY_CONTRACT_TGAS_KEY);
    STRING_SET(XPORPERTY_CONTRACT_TGAS_KEY, "0");

}

void xtable_statistic_info_collection_contract::on_collect_statistic_info(std::string const& statistic_info, uint64_t block_height, int64_t tgas) {
    XMETRICS_TIME_RECORD("sysContract_tableStatistic_on_collect_statistic_info");
    XMETRICS_GAUGE(metrics::xmetircs_tag_t::contract_table_statistic_exec_fullblock, 1);

    auto const & source_addr = SOURCE_ADDRESS();
    auto const & account = SELF_ADDRESS();

    std::string base_addr = "";
    uint32_t table_id = 0;

    XCONTRACT_ENSURE(data::xdatautil::extract_parts(source_addr, base_addr, table_id), "source address extract base_addr or table_id error!");
    xdbg("[xtable_statistic_info_collection_contract][on_collect_statistic_info] self_account %s, source_addr %s, base_addr %s\n", account.c_str(), source_addr.c_str(), base_addr.c_str());
    XCONTRACT_ENSURE(source_addr == account.value() && base_addr == top::sys_contract_sharding_statistic_info_addr, "invalid source addr's call!");

    auto fork_config = top::chain_upgrade::xtop_chain_fork_config_center::chain_fork_config();
    if (!chain_upgrade::xtop_chain_fork_config_center::is_forked(fork_config.table_statistic_info_fork_point, TIME())) {
        xdbg("[xtable_statistic_info_collection_contract][on_collect_statistic_info] not fork point time");
        return;
    }

    // check if the block processed
    uint64_t cur_statistic_height = 0;
    std::string value_str;
    try {
        XMETRICS_TIME_RECORD("sysContract_tableStatistic_get_property_contract_fulltable_height");
        if (MAP_FIELD_EXIST(xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_HEIGHT))
            value_str = MAP_GET(xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_HEIGHT);
    } catch (std::runtime_error const & e) {
        xwarn("[xtable_statistic_info_collection_contract][on_collect_statistic_info] read summarized slash info error:%s", e.what());
        throw;
    }

    if (!value_str.empty()) {
        cur_statistic_height = base::xstring_utl::touint64(value_str);
    }

    if (block_height <= cur_statistic_height) {
        xwarn("[xtable_statistic_info_collection_contract][on_collect_statistic_info] duplicated block, block height: %" PRIu64 ", current statistic block %" PRIu64,
        block_height,
        cur_statistic_height);

        return;
    }


    xinfo("[xtable_statistic_info_collection_contract][on_collect_statistic_info] enter collect statistic data, fullblock height: %" PRIu64 ", contract addr: %s, table_id: %u, pid: %d",
        block_height,
        source_addr.c_str(),
        table_id,
        getpid());



    xstatistics_data_t statistic_data;
    statistic_data.deserialize_based_on<base::xstream_t>({ std::begin(statistic_info), std::end(statistic_info) });
    auto node_service = contract::xcontract_manager_t::instance().get_node_service();
    std::string summarize_info_str;
    try {
        XMETRICS_TIME_RECORD("sysContract_tableStatistic_get_property_contract_unqualified_node_key");
        if (MAP_FIELD_EXIST(xstake::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY, "UNQUALIFIED_NODE"))
            summarize_info_str = MAP_GET(xstake::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY, "UNQUALIFIED_NODE");
    } catch (std::runtime_error const & e) {
        xwarn("[xtable_statistic_info_collection_contract][on_collect_statistic_info] read summarized slash info error:%s", e.what());
        throw;
    }

    std::string summarize_fulltableblock_num_str;
    try {
        XMETRICS_TIME_RECORD("sysContract_tableStatistic_get_property_contract_tableblock_num_key");
        if (MAP_FIELD_EXIST(xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_NUM)) {
            summarize_fulltableblock_num_str = MAP_GET(xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_NUM);
        }
    } catch (std::runtime_error & e) {
        xwarn("[xtable_statistic_info_collection_contract][on_collect_statistic_info] read summarized tableblock num error:%s", e.what());
        throw;
    }


    xunqualified_node_info_t summarize_info;
    uint32_t summarize_fulltableblock_num = 0;
    XCONTRACT_ENSURE(node_service != nullptr, "node service is empty!");
    collect_slash_statistic_info(statistic_data, node_service, summarize_info_str, summarize_fulltableblock_num_str,
                                    summarize_info, summarize_fulltableblock_num);
    update_slash_statistic_info(summarize_info, summarize_fulltableblock_num, block_height);


    process_workload_data(node_service, statistic_data, tgas);
}

void xtable_statistic_info_collection_contract::collect_slash_statistic_info(xstatistics_data_t const& statistic_data,  base::xvnodesrv_t * node_service, std::string const& summarize_info_str, std::string const& summarize_fulltableblock_num_str,
                                                                                xunqualified_node_info_t& summarize_info, uint32_t& summarize_fulltableblock_num) {
    XMETRICS_TIME_RECORD("sysContract_tableStatistic_collect_slash_statistic_info");

    // get the slash info
    auto const node_info = process_statistic_data(statistic_data, node_service);
    if (!summarize_info_str.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)summarize_info_str.data(), summarize_info_str.size());
        summarize_info.serialize_from(stream);
    }
    accumulate_node_info(node_info, summarize_info);

    #ifdef DEBUG
        print_summarize_info(summarize_info);
    #endif

    if (!summarize_fulltableblock_num_str.empty()) {
        summarize_fulltableblock_num = base::xstring_utl::touint32(summarize_fulltableblock_num_str);
    }

}

void  xtable_statistic_info_collection_contract::update_slash_statistic_info( xunqualified_node_info_t const& summarize_info, uint32_t summarize_fulltableblock_num, uint64_t block_height) {
    {
        XMETRICS_TIME_RECORD("sysContract_tableStatistic_set_property_contract_unqualified_node_key");
        base::xstream_t stream(base::xcontext_t::instance());
        summarize_info.serialize_to(stream);
        MAP_SET(xstake::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY, "UNQUALIFIED_NODE", std::string((char *)stream.data(), stream.size()));
    }


    {
        XMETRICS_TIME_RECORD("sysContract_tableStatistic_set_property_contract_extended_function_key");
        MAP_SET(xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_HEIGHT, base::xstring_utl::tostring(block_height));
        summarize_fulltableblock_num++;
        MAP_SET(xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_NUM, base::xstring_utl::tostring(summarize_fulltableblock_num));
    }


    xinfo("[xtable_statistic_info_collection_contract][on_collect_statistic_info] successfully summarize fulltableblock, current table num: %u, pid: %d",
        summarize_fulltableblock_num,
        getpid());

}

void  xtable_statistic_info_collection_contract::accumulate_node_info(xunqualified_node_info_t const&  node_info, xunqualified_node_info_t& summarize_info) {
    for (auto const & item : node_info.auditor_info) {
        summarize_info.auditor_info[item.first].block_count += item.second.block_count;
        summarize_info.auditor_info[item.first].subset_count += item.second.subset_count;
    }

    for (auto const & item : node_info.validator_info) {
        summarize_info.validator_info[item.first].block_count += item.second.block_count;
        summarize_info.validator_info[item.first].subset_count += item.second.subset_count;
    }
}

xunqualified_node_info_t  xtable_statistic_info_collection_contract::process_statistic_data(top::data::xstatistics_data_t const& block_statistic_data, base::xvnodesrv_t * node_service) {
    xunqualified_node_info_t res_node_info;

    // process one full tableblock statistic data
    for (auto const & static_item: block_statistic_data.detail) {
        auto elect_statistic = static_item.second;
        for (auto const & group_item: elect_statistic.group_statistics_data) {
            xgroup_related_statistics_data_t const& group_account_data = group_item.second;
            common::xgroup_address_t const& group_addr = group_item.first;
            xvip2_t const& group_xvip2 = top::common::xip2_t{
                group_addr.network_id(),
                group_addr.zone_id(),
                group_addr.cluster_id(),
                group_addr.group_id(),
                (uint16_t)group_account_data.account_statistics_data.size(),
                static_item.first
            };
            // process auditor group
            if (top::common::has<top::common::xnode_type_t::auditor>(group_addr.type())) {
                for (std::size_t slotid = 0; slotid < group_account_data.account_statistics_data.size(); ++slotid) {
                    auto account_addr = node_service->get_group(group_xvip2)->get_node(slotid)->get_account();
                    res_node_info.auditor_info[common::xnode_id_t{account_addr}].subset_count += group_account_data.account_statistics_data[slotid].vote_data.block_count;
                    res_node_info.auditor_info[common::xnode_id_t{account_addr}].block_count += group_account_data.account_statistics_data[slotid].vote_data.vote_count;
                    xdbg("[xtable_statistic_info_collection_contract][do_unqualified_node_slash] incremental auditor data: {gourp id: %d, account addr: %s, slot id: %u, subset count: %u, block_count: %u}", group_addr.group_id().value(), account_addr.c_str(),
                        slotid, group_account_data.account_statistics_data[slotid].vote_data.block_count, group_account_data.account_statistics_data[slotid].vote_data.vote_count);
                }
            } else if (top::common::has<top::common::xnode_type_t::validator>(group_addr.type())) {// process validator group
                for (std::size_t slotid = 0; slotid < group_account_data.account_statistics_data.size(); ++slotid) {
                    auto account_addr = node_service->get_group(group_xvip2)->get_node(slotid)->get_account();
                    res_node_info.validator_info[common::xnode_id_t{account_addr}].subset_count += group_account_data.account_statistics_data[slotid].vote_data.block_count;
                    res_node_info.validator_info[common::xnode_id_t{account_addr}].block_count += group_account_data.account_statistics_data[slotid].vote_data.vote_count;
                    xdbg("[xtable_statistic_info_collection_contract][do_unqualified_node_slash] incremental validator data: {gourp id: %d, account addr: %s, slot id: %u, subset count: %u, block_count: %u}", group_addr.group_id().value(), account_addr.c_str(),
                        slotid, group_account_data.account_statistics_data[slotid].vote_data.block_count, group_account_data.account_statistics_data[slotid].vote_data.vote_count);
                }

            } else { // invalid group
                xwarn("[xtable_statistic_info_collection_contract][do_unqualified_node_slash] invalid group id: %d", group_addr.group_id().value());
                std::error_code ec{ xvm::enum_xvm_error_code::enum_vm_exception };
                top::error::throw_error(ec, "[xtable_statistic_info_collection_contract][do_unqualified_node_slash] invalid group");
            }

        }

    }

    return res_node_info;
}

void xtable_statistic_info_collection_contract::report_summarized_statistic_info(common::xlogic_time_t timestamp) {
    XMETRICS_TIME_RECORD("sysContract_tableStatistic_report_summarized_statistic_info");

    auto const & source_addr = SOURCE_ADDRESS();
    auto const & account = SELF_ADDRESS();

    std::string base_addr = "";
    uint32_t table_id = 0;

    XCONTRACT_ENSURE(data::xdatautil::extract_parts(source_addr, base_addr, table_id), "source address extract base_addr or table_id error!");
    xdbg("[xtable_statistic_info_collection_contract][report_summarized_statistic_info] self_account %s, source_addr %s, base_addr %s\n", account.c_str(), source_addr.c_str(), base_addr.c_str());
    XCONTRACT_ENSURE(source_addr == account.value() && base_addr == top::sys_contract_sharding_statistic_info_addr, "invalid source addr's call!");

    auto fork_config = top::chain_upgrade::xtop_chain_fork_config_center::chain_fork_config();
    if (!chain_upgrade::xtop_chain_fork_config_center::is_forked(fork_config.table_statistic_info_fork_point, TIME())) {
        xdbg("[xtable_statistic_info_collection_contract][on_collect_statistic_info] not fork point time");
        return;
    }

    uint32_t summarize_fulltableblock_num = 0;
    std::string value_str;
    try {
        XMETRICS_TIME_RECORD("sysContract_tableStatistic_get_property_contract_tableblock_num_key");
        if (MAP_FIELD_EXIST(xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_NUM)) {
            value_str = MAP_GET(xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_NUM);
        }
    } catch (std::runtime_error & e) {
        xwarn("[xtable_statistic_info_collection_contract][report_summarized_statistic_info] read summarized tableblock num error:%s", e.what());
        throw;
    }

    if (!value_str.empty()) {
        summarize_fulltableblock_num = base::xstring_utl::touint32(value_str);
    }

    if (0 == summarize_fulltableblock_num) {
        xinfo("[xtable_statistic_info_collection_contract][report_summarized_statistic_info] no summarized fulltable info, timer round %" PRIu64 ", table_id: %d",
             timestamp,
             table_id);
        return;
    }


    xinfo("[xtable_statistic_info_collection_contract][report_summarized_statistic_info] enter report summarized info, timer round: %" PRIu64 ", table_id: %u, contract addr: %s, pid: %d",
         timestamp,
         table_id,
         source_addr.c_str(),
         getpid());

    {
        XMETRICS_TIME_RECORD("sysContractc_slash_report_statistic_info");
        xunqualified_node_info_t summarize_info;
        value_str.clear();
        try {
            XMETRICS_TIME_RECORD("sysContract_tableStatistic_get_property_contract_unqualified_node_key");
            if (MAP_FIELD_EXIST(xstake::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY, "UNQUALIFIED_NODE"))
                value_str = MAP_GET(xstake::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY, "UNQUALIFIED_NODE");
        } catch (std::runtime_error const & e) {
            xwarn("[xtable_statistic_info_collection_contract][report_summarized_statistic_info] read summarized slash info error:%s", e.what());
            throw;
        }

        if (!value_str.empty()) {
            base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
            summarize_info.serialize_from(stream);
        }


        value_str.clear();
        uint64_t cur_statistic_height = 0;
        try {
            XMETRICS_TIME_RECORD("sysContract_tableStatistic_get_property_contract_fulltable_height_key");
            if (MAP_FIELD_EXIST(xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_HEIGHT))
                value_str = MAP_GET(xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_HEIGHT);
        } catch (std::runtime_error const & e) {
            xwarn("[xtable_statistic_info_collection_contract][report_summarized_statistic_info] read fulltable num error:%s", e.what());
            throw;
        }

        if (!value_str.empty()) {
            cur_statistic_height = base::xstring_utl::touint64(value_str);
        }



        base::xstream_t stream(base::xcontext_t::instance());
        summarize_info.serialize_to(stream);
        stream << cur_statistic_height;

        xkinfo("[xtable_statistic_info_collection_contract][report_summarized_statistic_info] effective reprot summarized info, timer round %" PRIu64
                ", fulltableblock num: %u, cur_statistic_height: %" PRIu64 ", table_id: %u, contract addr: %s, pid: %d",
                timestamp,
                summarize_fulltableblock_num,
                cur_statistic_height,
                table_id,
                account.c_str(),
                getpid());

        {
            XMETRICS_TIME_RECORD("sysContract_tableStatistic_remove_property_contract_unqualified_node_key");
            MAP_REMOVE(xstake::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY, "UNQUALIFIED_NODE");
        }
        {
            XMETRICS_TIME_RECORD("sysContract_tableStatistic_remove_property_contract_tableblock_num_key");
            MAP_REMOVE(xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_NUM);
        }

        XMETRICS_GAUGE(metrics::xmetircs_tag_t::contract_table_statistic_report_fullblock, 1);
        std::string shard_slash_collect = std::string((char *)stream.data(), stream.size());
        {
            stream.reset();
            stream << shard_slash_collect;
            CALL(common::xaccount_address_t{sys_contract_zec_slash_info_addr}, "summarize_slash_info", std::string((char *)stream.data(), stream.size()));
        }

    }


    upload_workload();

}

void xtable_statistic_info_collection_contract::get_workload_from_data(const base::xvnodesrv_t * node_service,
                                                                       const xstatistics_data_t & statistic_data,
                                                                       const std::map<std::string, std::string> & workload_str,
                                                                       std::map<std::string, std::string> & workload_str_new) {
    std::map<common::xgroup_address_t, xgroup_workload_t> group_workload;
    // auto node_service = contract::xcontract_manager_t::instance().get_node_service();
    auto workload_per_tableblock = XGET_ONCHAIN_GOVERNANCE_PARAMETER(workload_per_tableblock);
    auto workload_per_tx = XGET_ONCHAIN_GOVERNANCE_PARAMETER(workload_per_tx);
    for (auto const & static_item : statistic_data.detail) {
        auto elect_statistic = static_item.second;
        for (auto const & group_item : elect_statistic.group_statistics_data) {
            common::xgroup_address_t const & group_addr = group_item.first;
            xgroup_related_statistics_data_t const & group_account_data = group_item.second;
            xvip2_t const & group_xvip2 = top::common::xip2_t{group_addr.network_id(),
                                                              group_addr.zone_id(),
                                                              group_addr.cluster_id(),
                                                              group_addr.group_id(),
                                                              (uint16_t)group_account_data.account_statistics_data.size(),
                                                              static_item.first};
            xdbg("[xtable_statistic_info_collection_contract::get_workload] group xvip2: %llu, %llu", group_xvip2.high_addr, group_xvip2.low_addr);
            for (size_t slotid = 0; slotid < group_account_data.account_statistics_data.size(); ++slotid) {
                auto account_str = node_service->get_group(group_xvip2)->get_node(slotid)->get_account();
                uint32_t block_count = group_account_data.account_statistics_data[slotid].block_data.block_count;
                uint32_t tx_count = group_account_data.account_statistics_data[slotid].block_data.transaction_count;
                uint32_t workload = block_count * workload_per_tableblock + tx_count * workload_per_tx;
                if (workload > 0) {
                    auto it2 = group_workload.find(group_addr);
                    if (it2 == group_workload.end()) {
                        xgroup_workload_t group_workload_info;
                        auto ret = group_workload.insert(std::make_pair(group_addr, group_workload_info));
                        XCONTRACT_ENSURE(ret.second, "insert workload failed");
                        it2 = ret.first;
                    }

                    it2->second.m_leader_count[account_str] += workload;
                }

                xdbg(
                    "[xtable_statistic_info_collection_contract::get_workload] group_addr: [%s, network_id: %u, zone_id: %u, cluster_id: %u, group_id: %u], leader: %s, workload: "
                    "%u, block_count: %u, tx_count: %u, workload_per_tableblock: %u, workload_per_tx: %u",
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
        }
    }

    for (auto const & one_group_workload : group_workload) {
        auto const & group_address = one_group_workload.first;
        auto const & add_workload = one_group_workload.second;
        // get
        // auto total_workload = get_workload(group_address);
        std::string group_address_str;
        {
            xstream_t stream(xcontext_t::instance());
            stream << group_address;
            group_address_str = std::string((const char *)stream.data(), stream.size());
        }
        xgroup_workload_t total_workload;
        {
            auto it = workload_str.find(group_address_str);
            if (it == workload_str.end()) {
                total_workload.cluster_id = group_address_str;
                xdbg("[xtable_statistic_info_collection_contract::update_workload] group not exist: %s", group_address.to_string().c_str());
            } else {
                xstream_t stream(xcontext_t::instance(), (uint8_t *)it->second.data(), it->second.size());
                total_workload.serialize_from(stream);
            }
        }
        // update
        for (auto const & leader_workload : add_workload.m_leader_count) {
            auto const & leader = leader_workload.first;
            auto const & count = leader_workload.second;
            total_workload.m_leader_count[leader] += count;
            total_workload.cluster_total_workload += count;
            xdbg("[xtable_statistic_info_collection_contract::update_workload] group: %u, leader: %s, count: %d, total_count: %d, total_workload: %d",
                 group_address.group_id().value(),
                 leader.c_str(),
                 count,
                 total_workload.m_leader_count[leader],
                 total_workload.cluster_total_workload);
        }
        // set
        // set_workload(group_address, total_workload);
        std::string str_new;
        {
            xstream_t stream(xcontext_t::instance());
            total_workload.serialize_to(stream);
            str_new = std::string((const char *)stream.data(), stream.size());
            workload_str_new[group_address_str] = str_new;
        }
    }
}

void xtable_statistic_info_collection_contract::upload_workload_internal(std::string & call_contract_str) {
    XMETRICS_TIME_RECORD("sysContractc_workload_report_statistic_info");
    std::map<std::string, std::string> group_workload_str;
    std::map<common::xgroup_address_t, xgroup_workload_t> group_workload_upload;

    MAP_COPY_GET(XPORPERTY_CONTRACT_WORKLOAD_KEY, group_workload_str);
    for (auto it = group_workload_str.begin(); it != group_workload_str.end(); it++) {
        xstream_t key_stream(xcontext_t::instance(), (uint8_t *)it->first.data(), it->first.size());
        common::xgroup_address_t group_address;
        key_stream >> group_address;
        xstream_t stream(xcontext_t::instance(), (uint8_t *)it->second.data(), it->second.size());
        xgroup_workload_t group_workload;
        group_workload.serialize_from(stream);

        for (auto const & leader_workload : group_workload.m_leader_count) {
            auto const & leader = leader_workload.first;
            auto const & workload = leader_workload.second;

            auto it2 = group_workload_upload.find(group_address);
            if (it2 == group_workload_upload.end()) {
                xgroup_workload_t empty_workload;
                auto ret = group_workload_upload.insert(std::make_pair(group_address, empty_workload));
                it2 = ret.first;
            }
            it2->second.m_leader_count[leader] += workload;
        }
    }
    if (group_workload_upload.size() > 0) {
        int64_t tgas = 0;
        {
            std::string pledge_tgas_str = STRING_GET2(XPORPERTY_CONTRACT_TGAS_KEY);
            if (!pledge_tgas_str.empty()) {
                tgas = xstring_utl::toint64(pledge_tgas_str);
            }
        }

        uint64_t height = 0;
        {
            std::string value_str;
            if (MAP_GET2(XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_HEIGHT, value_str)) {
                xwarn("[xtable_statistic_info_collection_contract::update_workload] table height not exist!");
            } else {
                if (!value_str.empty()) {
                    height = base::xstring_utl::touint64(value_str);
                }
            }
        }

        std::string group_workload_upload_str;
        {
            xstream_t stream(xcontext_t::instance());
            MAP_OBJECT_SERIALIZE2(stream, group_workload_upload);
            stream << tgas;
            stream << height;
            group_workload_upload_str = std::string((char *)stream.data(), stream.size());
            xinfo("[xtable_statistic_info_collection_contract::upload_workload] upload workload to zec reward, group_workload_upload size: %d", group_workload_upload.size());
        }
        {
            xstream_t stream(xcontext_t::instance());
            stream << group_workload_upload_str;
            call_contract_str = std::string((char *)stream.data(), stream.size());
            group_workload_upload.clear();
        }

        MAP_CLEAR(XPORPERTY_CONTRACT_WORKLOAD_KEY);
        STRING_SET(XPORPERTY_CONTRACT_TGAS_KEY, "0");
    }
}

void xtable_statistic_info_collection_contract::upload_workload() {
    XMETRICS_TIME_RECORD("sysContractc_workload_report_statistic_info");
    std::string call_contract_str{};
    upload_workload_internal(call_contract_str);
    if (!call_contract_str.empty()) {
        CALL(common::xaccount_address_t{sys_contract_zec_workload_addr}, "on_receive_workload", call_contract_str);
    }
}

void xtable_statistic_info_collection_contract::process_workload_data_internal(const base::xvnodesrv_t * node_service,
                                                                               const xstatistics_data_t & statistic_data,
                                                                               const int64_t tgas,
                                                                               const std::map<std::string, std::string> & workload_str,
                                                                               const std::string & tgas_str,
                                                                               std::map<std::string, std::string> & workload_str_new,
                                                                               std::string & tgas_str_new) {
    XMETRICS_TIME_RECORD("sysContract_tableStatistic_workload_statistic_info");
    if (statistic_data.detail.size() > 0) {
        get_workload_from_data(node_service, statistic_data, workload_str, workload_str_new);
    }
    if (tgas != 0) {
        int64_t total_tgas = 0;
        if (!tgas_str.empty()) {
            total_tgas = xstring_utl::toint64(tgas_str);
        }
        total_tgas += tgas;
        tgas_str_new = xstring_utl::tostring(total_tgas);
    }
}

void xtable_statistic_info_collection_contract::process_workload_data(const base::xvnodesrv_t * node_service, const xstatistics_data_t & statistic_data, const int64_t tgas) {
    std::map<std::string, std::string> workload_str_new;
    std::string tgas_str_new{};
    {
        std::map<std::string, std::string> workload_str;
        try {
            MAP_COPY_GET(XPORPERTY_CONTRACT_WORKLOAD_KEY, workload_str);
        } catch (std::runtime_error & e) {
            xwarn("[xtable_statistic_info_collection_contract::on_collect_statistic_info] MAP COPY GET error:%s", e.what());
        }
        std::string tgas_str{STRING_GET2(XPORPERTY_CONTRACT_TGAS_KEY)};
        process_workload_data_internal(node_service, statistic_data, tgas, workload_str, tgas_str, workload_str_new, tgas_str_new);
    }

    if (!tgas_str_new.empty()) {
        STRING_SET(XPORPERTY_CONTRACT_TGAS_KEY, tgas_str_new);
    }
    for (auto it = workload_str_new.cbegin(); it != workload_str_new.cend(); it++) {
        MAP_SET(XPORPERTY_CONTRACT_WORKLOAD_KEY, it->first, it->second);
    }
}

NS_END3

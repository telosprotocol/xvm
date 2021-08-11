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

using namespace top::data;  // NOLINE

NS_BEG3(top, xvm, xcontract)

#define FULLTABLE_NUM_PROPERTY  "FULLTABLE_NUM_PROPERTY"
#define FULLTABLE_HEIGHT        "FULLTABLE_HEIGHT"

xtable_statistic_info_collection_contract::xtable_statistic_info_collection_contract(common::xnetwork_id_t const & network_id) : xbase_t{network_id} {}

void xtable_statistic_info_collection_contract::setup() {
    // initialize map key
    MAP_CREATE(xstake::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY);

    MAP_CREATE(xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY);
    MAP_SET(xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_NUM_PROPERTY, "0");
    MAP_SET(xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_HEIGHT, "0");

}

void xtable_statistic_info_collection_contract::on_collect_statistic_info(std::string const& slash_info, uint64_t block_height) {
    XMETRICS_TIME_RECORD("sysContract_tableStatistic_on_collect_statistic_info");

    auto const & source_addr = SOURCE_ADDRESS();
    auto const & account = SELF_ADDRESS();

    std::string base_addr = "";
    uint32_t table_id = 0;

    XCONTRACT_ENSURE(data::xdatautil::extract_parts(source_addr, base_addr, table_id), "source address extract base_addr or table_id error!");
    xdbg("[xtable_statistic_info_collection_contract][on_collect_statistic_info] self_account %s, source_addr %s, base_addr %s\n", account.c_str(), source_addr.c_str(), base_addr.c_str());
    XCONTRACT_ENSURE(source_addr == account.value() && base_addr == top::sys_contract_sharding_statistic_info_addr, "invalid source addr's call!");
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


    // get the slash info from db event
    xstatistics_data_t statistic_data;
    statistic_data.deserialize_based_on<base::xstream_t>({ std::begin(slash_info), std::end(slash_info) });
    auto node_service = contract::xcontract_manager_t::instance().get_node_service();
    auto const node_info = process_statistic_data(statistic_data, node_service);

    xunqualified_node_info_t summarize_info;
    value_str.clear();

    try {
        XMETRICS_TIME_RECORD("sysContract_tableStatistic_get_property_contract_unqualified_node_key");
        if (MAP_FIELD_EXIST(xstake::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY, "UNQUALIFIED_NODE"))
            value_str = MAP_GET(xstake::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY, "UNQUALIFIED_NODE");
    } catch (std::runtime_error const & e) {
        xwarn("[xtable_statistic_info_collection_contract][on_collect_statistic_info] read summarized slash info error:%s", e.what());
        throw;
    }

    if (!value_str.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
        summarize_info.serialize_from(stream);
    }
    accumulate_node_info(node_info, summarize_info);

    #ifdef DEBUG
        print_summarize_info(summarize_info);
    #endif

    value_str.clear();
    uint32_t summarize_fulltableblock_num = 0;
    try {
        XMETRICS_TIME_RECORD("sysContract_tableStatistic_get_property_contract_tableblock_num_key");
        if (MAP_FIELD_EXIST(xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_NUM_PROPERTY)) {
            value_str = MAP_GET(xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_NUM_PROPERTY);
        }
    } catch (std::runtime_error & e) {
        xwarn("[xtable_statistic_info_collection_contract][summarize_slash_info] read summarized tableblock num error:%s", e.what());
        throw;
    }

    if (!value_str.empty()) {
        summarize_fulltableblock_num = base::xstring_utl::touint32(value_str);
    }

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
        MAP_SET(xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_NUM_PROPERTY, base::xstring_utl::tostring(summarize_fulltableblock_num));
    }


    xinfo("[xtable_statistic_info_collection_contract][on_collect_statistic_info] successfully summarize fulltableblock, current table num: %u, table_id: %u, pid: %d",
        summarize_fulltableblock_num,
        table_id,
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
    XMETRICS_TIME_RECORD("sysContract_tableStatistic_on_collect_statistic_info");

    auto const & source_addr = SOURCE_ADDRESS();
    auto const & account = SELF_ADDRESS();

    std::string base_addr = "";
    uint32_t table_id = 0;

    XCONTRACT_ENSURE(data::xdatautil::extract_parts(source_addr, base_addr, table_id), "source address extract base_addr or table_id error!");
    xdbg("[xtable_statistic_info_collection_contract][report_summarized_statistic_info] self_account %s, source_addr %s, base_addr %s\n", account.c_str(), source_addr.c_str(), base_addr.c_str());
    XCONTRACT_ENSURE(source_addr == account.value() && base_addr == top::sys_contract_sharding_statistic_info_addr, "invalid source addr's call!");

    uint32_t summarize_fulltableblock_num = 0;
    std::string value_str;
    try {
        XMETRICS_TIME_RECORD("sysContract_tableStatistic_get_property_contract_tableblock_num_key");
        if (MAP_FIELD_EXIST(xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_NUM_PROPERTY)) {
            value_str = MAP_GET(xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_NUM_PROPERTY);
        }
    } catch (std::runtime_error & e) {
        xwarn("[xtable_statistic_info_collection_contract][summarize_slash_info] read summarized tableblock num error:%s", e.what());
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
    stream << summarize_fulltableblock_num;
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
        MAP_REMOVE(xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, FULLTABLE_NUM_PROPERTY);
    }

    std::string shard_slash_collect = std::string((char *)stream.data(), stream.size());
    {
        stream.reset();
        stream << shard_slash_collect;
        CALL(common::xaccount_address_t{sys_contract_zec_slash_info_addr}, "summarize_slash_info", std::string((char *)stream.data(), stream.size()));
    }

}

NS_END3

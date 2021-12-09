// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xchain_upgrade/xchain_data_processor.h"
#include "xcommon/xip.h"
#include "xdata/xgenesis_data.h"
#include "xdata/xfull_tableblock.h"
#include "xstake/xstake_algorithm.h"
#include "xvm/xsystem_contracts/xslash/xzec_slash_info_contract_new.h"

using namespace top::data;

#ifndef XSYSCONTRACT_MODULE
#    define XSYSCONTRACT_MODULE "SysContract_"
#endif
#define XCONTRACT_PREFIX "ZecSlash_"
#define XZEC_SLASH_CONTRACT XSYSCONTRACT_MODULE XCONTRACT_PREFIX

NS_BEG2(top, system_contracts)

#define SLASH_DELETE_PROPERTY  "SLASH_DELETE_PROPERTY"
#define LAST_SLASH_TIME  "LAST_SLASH_TIME"
#define SLASH_TABLE_ROUND "SLASH_TABLE_ROUND"
#define UNQUALIFIED_PROP_KEY "UNQUALIFIED_NODE"
#define TABLEBLOCK_NUM_KEY "TABLEBLOCK_NUM"

void xzec_slash_info_contract_new::setup() {
    // initialize property
    std::vector<std::pair<std::string, std::string>> db_kv_131;
    chain_data::xchain_data_processor_t::get_stake_map_property(common::xlegacy_account_address_t{address()}, xstake::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY, db_kv_131);
    process_reset_data(db_kv_131);

    m_extend_func_prop.set(SLASH_DELETE_PROPERTY, "false");
    m_extend_func_prop.set(LAST_SLASH_TIME, "0");
    m_extend_func_prop.set(SLASH_TABLE_ROUND, "0");
}

void xzec_slash_info_contract_new::summarize_slash_info(std::string const & slash_info) {
    XMETRICS_TIME_RECORD(XZEC_SLASH_CONTRACT "summarize_slash_info");
    XMETRICS_GAUGE(metrics::xmetrics_tag_t::contract_zec_slash_summarize_fullblock, 1);

    auto const & self_account = address();
    auto const & source_account = sender();

    std::string base_addr = "";
    uint32_t table_id = 0;
    XCONTRACT_ENSURE(data::xdatautil::extract_parts(source_account.value(), base_addr, table_id), "source address extract base_addr or table_id error!");
    xdbg("[xzec_slash_info_contract_new_t][summarize_slash_info] self_account %s, source_addr %s, base_addr %s\n", self_account.c_str(), source_account.c_str(), base_addr.c_str());
    XCONTRACT_ENSURE(base_addr == top::sys_contract_sharding_statistic_info_addr, "invalid source addr's call!");

    xinfo("[xzec_slash_info_contract_new_t][summarize_slash_info] enter table contract report slash info, SOURCE_ADDRESS: %s, pid:%d, ", source_account.c_str(), getpid());

    auto summarized_height = read_fulltable_height_of_table(table_id);
    // get current summarized info string
    std::string summarize_info_str;
    {
        if (m_unqualified_node_prop.exist(UNQUALIFIED_PROP_KEY)) {
            summarize_info_str = m_unqualified_node_prop.get("UNQUALIFIED_NODE");
        }
    }

    std::string summarize_tableblock_count_str;
    {
        if (m_tableblock_num_prop.exist(TABLEBLOCK_NUM_KEY)) {
            summarize_tableblock_count_str = m_tableblock_num_prop.get(TABLEBLOCK_NUM_KEY);
        }
    }

    xunqualified_node_info_t summarize_info;
    uint32_t summarize_tableblock_count = 0;
    std::uint64_t cur_statistic_height = 0;

    if (summarize_slash_info_internal(
            slash_info, summarize_info_str, summarize_tableblock_count_str, summarized_height, summarize_info, summarize_tableblock_count, cur_statistic_height)) {
        // set summarize info
        base::xstream_t stream(base::xcontext_t::instance());
        summarize_info.serialize_to(stream);
        m_unqualified_node_prop.set(UNQUALIFIED_PROP_KEY, std::string((char *)stream.data(), stream.size()));

        stream.reset();
        stream << summarize_tableblock_count;
        m_tableblock_num_prop.set(TABLEBLOCK_NUM_KEY, std::string((char *)stream.data(), stream.size()));

        stream.reset();
        stream << cur_statistic_height;
        m_tableblock_num_prop.set(base::xstring_utl::tostring(table_id), std::string((char *)stream.data(), stream.size()));
    } else {
        xinfo("[xzec_slash_info_contract_new_t][summarize_slash_info] condition not statisfy!");
    }


    xkinfo(
        "[xzec_slash_info_contract_new_t][summarize_slash_info] effective table contract report slash info, auditor size: %zu, validator size: %zu, summarized tableblock num: %u, "
        "pid: %d",
        summarize_info.auditor_info.size(),
        summarize_info.validator_info.size(),
        summarize_tableblock_count,
        getpid());
}


bool xzec_slash_info_contract_new::summarize_slash_info_internal(std::string const & slash_info,
                                                                 std::string const & summarize_info_str,
                                                                 std::string const & summarize_tableblock_count_str,
                                                                 uint64_t const summarized_height,
                                                                 xunqualified_node_info_t & summarize_info,
                                                                 uint32_t & summarize_tableblock_count,
                                                                 std::uint64_t & cur_statistic_height) {
    // get report info
    xunqualified_node_info_t node_info;
    base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)slash_info.data(), slash_info.size());
    node_info.serialize_from(stream);
    stream >> cur_statistic_height;

    if (cur_statistic_height <= summarized_height) {
        xwarn("[xzec_slash_info_contract_new_t][summarize_slash_info] report older slash info, summarized height: %" PRIu64 ", report height: %" PRIu64,
              summarized_height,
              cur_statistic_height);
        return false;
    }

    // get serialized summarized info from str
    if (!summarize_info_str.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)summarize_info_str.data(), summarize_info_str.size());
        summarize_info.serialize_from(stream);
    }

    if (!summarize_tableblock_count_str.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)summarize_tableblock_count_str.data(), summarize_tableblock_count_str.size());
        stream >> summarize_tableblock_count;
    }

    // accoumulate node info
    accumulate_node_info(node_info, summarize_info);
    summarize_tableblock_count += cur_statistic_height - summarized_height;

    return true;
}

void xzec_slash_info_contract_new::do_unqualified_node_slash(common::xlogic_time_t const timestamp) {
    XMETRICS_TIME_RECORD(XZEC_SLASH_CONTRACT "sysContract_zecSlash_do_unqualified_node_slash");
    XMETRICS_CPU_TIME_RECORD(XZEC_SLASH_CONTRACT "sysContract_zecSlash_do_unqualified_node_slash_cpu");
    auto const & self_account = address();
    auto const & source_account = sender();

    std::string base_addr = "";
    uint32_t table_id = 0;
    XCONTRACT_ENSURE(data::xdatautil::extract_parts(source_account.value(), base_addr, table_id), "source address extract base_addr or table_id error!");
    xdbg("[xzec_slash_info_contract_new_t][do_unqualified_node_slash] self_account %s, source_addr %s, base_addr %s\n", self_account.c_str(), source_account.c_str(), base_addr.c_str());
    XCONTRACT_ENSURE(source_account == self_account && source_account.value() == top::sys_contract_zec_slash_info_addr, "invalid source addr's call!");

    xinfo("[xzec_slash_info_contract_new_t][do_unqualified_node_slash] do unqualified node slash info, time round: %" PRIu64 ": SOURCE_ADDRESS: %s, pid:%d",
         timestamp,
         source_account.c_str(),
         getpid());

    /**
     *
     * get stored processed slash info
     *
     */
    xunqualified_node_info_t present_summarize_info;
    uint32_t present_tableblock_count = 0;
    pre_condition_process(present_summarize_info, present_tableblock_count);

    #ifdef DEBUG
        print_table_height_info();
        print_summarize_info(present_summarize_info);
        xdbg_info("[xzec_slash_info_contract_new_t][do_unqualified_node_slash] present tableblock num is %u", present_tableblock_count);
    #endif


    xunqualified_node_info_t summarize_info = present_summarize_info;
    uint32_t summarize_tableblock_count = present_tableblock_count;

    // get check params
    std::string last_slash_time_str;
    if (m_extend_func_prop.exist(LAST_SLASH_TIME)) {
        last_slash_time_str = m_extend_func_prop.get(LAST_SLASH_TIME);
    }

    auto punish_interval_table_block_param = XGET_ONCHAIN_GOVERNANCE_PARAMETER(punish_interval_table_block);
    auto punish_interval_time_block_param = XGET_ONCHAIN_GOVERNANCE_PARAMETER(punish_interval_time_block);

    // get filter param
    auto slash_vote = XGET_ONCHAIN_GOVERNANCE_PARAMETER(sign_block_publishment_threshold_value);
    auto slash_persent = XGET_ONCHAIN_GOVERNANCE_PARAMETER(sign_block_ranking_publishment_threshold_value);
    auto award_vote = XGET_ONCHAIN_GOVERNANCE_PARAMETER(sign_block_ranking_reward_threshold_value);
    auto award_persent = XGET_ONCHAIN_GOVERNANCE_PARAMETER(sign_block_reward_threshold_value);

    // do slash
    std::vector<xaction_node_info_t> node_to_action;
    if(do_unqualified_node_slash_internal(last_slash_time_str, summarize_tableblock_count, punish_interval_table_block_param, punish_interval_time_block_param, timestamp,
                                           summarize_info, slash_vote, slash_persent, award_vote, award_persent, node_to_action)) {

        m_extend_func_prop.set(SLASH_DELETE_PROPERTY, "true");
        m_extend_func_prop.set(LAST_SLASH_TIME, std::to_string(timestamp));

        base::xstream_t stream{base::xcontext_t::instance()};
        VECTOR_OBJECT_SERIALIZE2(stream, node_to_action);
        std::string punish_node_str = std::string((char *)stream.data(), stream.size());
        {
            stream.reset();
            stream << punish_node_str;
            xinfo("[xzec_slash_info_contract_new_t][do_unqualified_node_slash] call register contract slash_unqualified_node, time round: %" PRIu64, timestamp);
            XMETRICS_PACKET_INFO(XZEC_SLASH_CONTRACT "sysContract_zecSlash", "effective slash timer round", std::to_string(timestamp));
            call(common::xaccount_address_t{sys_contract_rec_registration_addr}, "slash_unqualified_node", std::string((char *)stream.data(), stream.size()),
                    contract_common::xfollowup_transaction_schedule_type_t::immediately);
        }
    } else {
        xdbg("[xzec_slash_info_contract_new_t][do_unqualified_node_slash] condition not satisfied! time round %" PRIu64, timestamp);
    }
}

bool xzec_slash_info_contract_new::do_unqualified_node_slash_internal(std::string const & last_slash_time_str,
                                                                      uint32_t summarize_tableblock_count,
                                                                      uint32_t punish_interval_table_block_param,
                                                                      uint32_t punish_interval_time_block_param,
                                                                      common::xlogic_time_t const timestamp,
                                                                      xunqualified_node_info_t const & summarize_info,
                                                                      uint32_t slash_vote,
                                                                      uint32_t slash_persent,
                                                                      uint32_t award_vote,
                                                                      uint32_t award_persent,
                                                                      std::vector<xaction_node_info_t> & node_to_action) {
    // check slash interval time
    auto result = slash_condition_check(last_slash_time_str, summarize_tableblock_count, punish_interval_table_block_param, punish_interval_time_block_param, timestamp);
    if (!result) {
        xinfo("[xzec_slash_info_contract_new_t][do_unqualified_node_slash_internal] slash condition not statisfied, time round: %" PRIu64 ": pid:%d", timestamp, getpid());
        return false;
    }

    node_to_action = filter_nodes(summarize_info, slash_vote, slash_persent, award_vote, award_persent);
    if (node_to_action.empty()) {
        xinfo("[xzec_slash_info_contract_new_t][do_unqualified_node_slash_internal] filter nodes empty, time round: %" PRIu64 ": pid:%d", timestamp, getpid());
        return false;
    }

    return true;
}

void xzec_slash_info_contract_new::pre_condition_process(xunqualified_node_info_t & summarize_info, uint32_t & tableblock_count) {
    std::string delete_property = m_extend_func_prop.get(SLASH_DELETE_PROPERTY);
    xdbg("[xzec_slash_info_contract_new_t][do_unqualified_node_slash] extend key value: %s", delete_property.c_str());

    if (delete_property == "true") {
        m_unqualified_node_prop.remove("UNQUALIFIED_NODE");
        m_tableblock_num_prop.remove("TABLEBLOCK_NUM");
        m_extend_func_prop.set(SLASH_DELETE_PROPERTY, "false");
    } else {
        std::string value_str;

        if (m_tableblock_num_prop.exist("TABLEBLOCK_NUM")) {
            value_str = m_tableblock_num_prop.get("TABLEBLOCK_NUM");
        }

        // normally only first time will be empty(property not create yet), means tableblock count is zero, so no need else branch
        if (!value_str.empty()) {
            base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
            stream >> tableblock_count;
            xdbg("[xzec_slash_info_contract_new_t][do_unqualified_node_slash]  current summarized tableblock num is: %u", tableblock_count);
        }

        value_str.clear();
        if (m_unqualified_node_prop.exist("UNQUALIFIED_NODE")) {
            value_str = m_unqualified_node_prop.get("UNQUALIFIED_NODE");
        }

        // normally only first time will be empty(property not create yet), means height is zero, so no need else branch
        if (!value_str.empty()) {
            base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
            summarize_info.serialize_from(stream);
        }
    }
}

std::vector<xaction_node_info_t> xzec_slash_info_contract_new::filter_nodes(xunqualified_node_info_t const & summarize_info, uint32_t slash_vote, uint32_t slash_persent, uint32_t award_vote, uint32_t award_persent) {
    std::vector<xaction_node_info_t> nodes_to_slash{};

    if (summarize_info.auditor_info.empty() && summarize_info.validator_info.empty()) {
        xwarn("[xzec_slash_info_contract_new_t][filter_slashed_nodes] the summarized node info is empty!");
        return nodes_to_slash;
    } else {
        // summarized info
        nodes_to_slash = filter_helper(summarize_info, slash_vote, slash_persent, award_vote, award_persent);
    }

    xkinfo("[xzec_slash_info_contract_new_t][filter_slashed_nodes] the filter node to slash: size: %zu", nodes_to_slash.size());
    return nodes_to_slash;
}

std::vector<xaction_node_info_t> xzec_slash_info_contract_new::filter_helper(xunqualified_node_info_t const & node_map, uint32_t slash_vote, uint32_t slash_persent, uint32_t award_vote, uint32_t award_persent) {
    std::vector<xaction_node_info_t> res{};

    // do filter
    std::vector<xunqualified_filter_info_t> node_to_action{};
    for (auto const & node : node_map.auditor_info) {
        xunqualified_filter_info_t info;
        info.node_id = node.first;
        info.node_type = common::xnode_type_t::consensus_auditor;
        info.vote_percent = node.second.block_count * 100 / node.second.subset_count;
        node_to_action.emplace_back(info);
    }

    std::sort(node_to_action.begin(), node_to_action.end(), [](xunqualified_filter_info_t const & lhs, xunqualified_filter_info_t const & rhs) {
        return lhs.vote_percent < rhs.vote_percent;
    });
    // uint32_t slash_size = node_to_slash.size() * slash_persent / 100 ?  node_to_slash.size() * slash_persent / 100 : 1;
    uint32_t slash_size = node_to_action.size() * slash_persent / 100;
    for (size_t i = 0; i < slash_size; ++i) {
        if (node_to_action[i].vote_percent < slash_vote || 0 == node_to_action[i].vote_percent) {
            res.push_back(xaction_node_info_t{node_to_action[i].node_id, node_to_action[i].node_type});
        }
    }

    uint32_t award_size = node_to_action.size() * award_persent / 100;
    for (int i = (int)node_to_action.size() - 1; i >= (int)(node_to_action.size() - award_size); --i) {
        if (node_to_action[i].vote_percent > award_vote) {
            res.push_back(xaction_node_info_t{node_to_action[i].node_id, node_to_action[i].node_type, false});
        }
    }

    node_to_action.clear();
    for (auto const & node : node_map.validator_info) {
        xunqualified_filter_info_t info;
        info.node_id = node.first;
        info.node_type = common::xnode_type_t::consensus_validator;
        info.vote_percent = node.second.block_count * 100 / node.second.subset_count;
        node_to_action.emplace_back(info);
    }

    std::sort(node_to_action.begin(), node_to_action.end(), [](xunqualified_filter_info_t const & lhs, xunqualified_filter_info_t const & rhs) {
        return lhs.vote_percent < rhs.vote_percent;
    });

    // uint32_t slash_size = node_to_slash.size() * slash_persent / 100 ?  node_to_slash.size() * slash_persent / 100 : 1;
    slash_size = node_to_action.size() * slash_persent / 100;
    for (size_t i = 0; i < slash_size; ++i) {
        if (node_to_action[i].vote_percent < slash_vote || 0 == node_to_action[i].vote_percent) {
            res.push_back(xaction_node_info_t{node_to_action[i].node_id, node_to_action[i].node_type});
        }
    }

    award_size = node_to_action.size() * award_persent / 100;
    for (int i = (int)node_to_action.size() - 1; i >= (int)(node_to_action.size() - award_size); --i) {
        if (node_to_action[i].vote_percent > award_vote) {
            res.push_back(xaction_node_info_t{node_to_action[i].node_id, node_to_action[i].node_type, false});
        }
    }

    return res;
}

void xzec_slash_info_contract_new::print_summarize_info(data::xunqualified_node_info_t const & summarize_slash_info) {
    std::string out = "";
    for (auto const & item : summarize_slash_info.auditor_info) {
        out += item.first.value();
        out += "|" + std::to_string(item.second.block_count);
        out += "|" + std::to_string(item.second.subset_count) + "|";
    }

    for (auto const & item : summarize_slash_info.validator_info) {
        out += item.first.value();
        out += "|" + std::to_string(item.second.block_count);
        out += "|" + std::to_string(item.second.subset_count) + "|";
    }

    xdbg("[xzec_slash_info_contract_new_t][print_summarize_info] summarize info: %s", out.c_str());
}

void xzec_slash_info_contract_new::print_table_height_info() {
    std::string out = "|";

    for (auto i = 0; i < enum_vledger_const::enum_vbucket_has_tables_count; ++i) {
        std::string height_key = std::to_string(i);
        std::string value_str;
        if (m_tableblock_num_prop.exist(height_key)) {
            value_str = m_tableblock_num_prop.get(height_key);
        }

        if (!value_str.empty()) {
            uint64_t height;
            base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
            stream >> height;
            if (height != 0) {
                out += std::to_string(i) + "-" + std::to_string(height) + "|";
            }
        }

    }

    xdbg("[xzec_slash_info_contract_new_t][print_table_height_info] table height info: %s", out.c_str());
}

void  xzec_slash_info_contract_new::accumulate_node_info(xunqualified_node_info_t const&  node_info, xunqualified_node_info_t& summarize_info) {

    for (auto const & item : node_info.auditor_info) {
        summarize_info.auditor_info[item.first].block_count += item.second.block_count;
        summarize_info.auditor_info[item.first].subset_count += item.second.subset_count;
    }

    for (auto const & item : node_info.validator_info) {
        summarize_info.validator_info[item.first].block_count += item.second.block_count;
        summarize_info.validator_info[item.first].subset_count += item.second.subset_count;
    }
}

bool xzec_slash_info_contract_new::slash_condition_check(std::string const & last_slash_time_str,
                                                         uint32_t summarize_tableblock_count,
                                                         uint32_t punish_interval_table_block_param,
                                                         uint32_t punish_interval_time_block_param,
                                                         common::xlogic_time_t const timestamp) {
    if (summarize_tableblock_count < punish_interval_table_block_param) {
        xinfo("[xzec_slash_info_contract_new_t][do_unqualified_node_slash] summarize_tableblock_count not enought, time round: %" PRIu64 ", tableblock_count:%u",
              timestamp,
              summarize_tableblock_count);
        return false;
    }

    // check slash interval time
    XCONTRACT_ENSURE(!last_slash_time_str.empty(), "read last slash time error, it is empty");
    uint64_t last_slash_time = base::xstring_utl::toint64(last_slash_time_str);
    XCONTRACT_ENSURE(timestamp > last_slash_time, "current timestamp smaller than last slash time");
    if (timestamp - last_slash_time < punish_interval_time_block_param) {
        xinfo("[xzec_slash_info_contract_new_t][do_unqualified_node_slash] punish interval time not enought, time round: %" PRIu64 ", last slash time: %s",
              timestamp,
              last_slash_time_str.c_str());
        return false;
    } else {
        xinfo("[xzec_slash_info_contract_new_t][do_unqualified_node_slash] statisfy punish interval condition, time round: %" PRIu64 ", last slash time: %s",
              timestamp,
              last_slash_time_str.c_str());
        XMETRICS_TIME_RECORD(XZEC_SLASH_CONTRACT "sysContract_zecSlash_set_property_contract_extend_key");
    }

    return true;
}

 uint64_t xzec_slash_info_contract_new::read_fulltable_height_of_table(uint32_t table_id) {
    std::string height_key = std::to_string(table_id);
    uint64_t read_height = 0;
    std::string value_str = m_tableblock_num_prop.get(height_key);

    // normally only first time will be empty(property not create yet), means height is zero, so no need else branch
    if (!value_str.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
        stream >> read_height;
        xdbg("[xzec_slash_info_contract_new_t][do_unqualified_node_slash]  last read full tableblock height is: %" PRIu64, read_height);
    }

    return read_height;
 }

 void  xzec_slash_info_contract_new::process_reset_data(std::vector<std::pair<std::string, std::string>> const& db_kv_131) {
    for (auto const & _p : db_kv_131) {
        if (_p.first == "UNQUALIFIED_NODE") {
            xunqualified_node_info_t node_info;
            base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)_p.second.data(), _p.second.size());
            base::xstream_t internal_stream{ base::xcontext_t::instance() };
            stream >> internal_stream;
            int32_t count;
            internal_stream >> count;
            // xdbg("LON cnt: %d", count);
            for (int32_t i = 0; i < count; i++) {
                std::string id;
                xnode_vote_percent_t value;
                base::xstream_t &internal_stream_temp = internal_stream;
                base::xstream_t internal_key_stream{ base::xcontext_t::instance() };
                internal_stream_temp >> internal_key_stream;
                internal_key_stream >> id;
                common::xnode_id_t node_id(id);
                value.serialize_from(internal_stream);
                node_info.auditor_info.emplace(std::make_pair(std::move(node_id), std::move(value)));
            }
            internal_stream >> count;
            for (int32_t i = 0; i < count; i++) {
                common::xnode_id_t node_id;
                xnode_vote_percent_t value;
                base::xstream_t &internal_stream_temp = internal_stream;
                base::xstream_t internal_key_stream{ base::xcontext_t::instance() };
                internal_stream_temp >> internal_key_stream;
                internal_key_stream >> node_id;
                value.serialize_from(internal_stream);
                node_info.validator_info.emplace(std::make_pair(std::move(node_id), std::move(value)));
            }
            stream.reset();
            node_info.serialize_to(stream);
            m_unqualified_node_prop.set(_p.first, std::string((char *)stream.data(), stream.size()));
        } else {
            m_unqualified_node_prop.set(_p.first, _p.second);
        }
    }
 }

NS_END2

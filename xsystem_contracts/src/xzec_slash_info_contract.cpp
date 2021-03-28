// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xslash/xzec_slash_info_contract.h"

#include "xdata/xgenesis_data.h"
#include "xstake/xstake_algorithm.h"

using namespace top::data;

NS_BEG3(top, xvm, xcontract)

xzec_slash_info_contract::xzec_slash_info_contract(common::xnetwork_id_t const & network_id) : xbase_t{network_id} {}

void xzec_slash_info_contract::setup() {
    // initialize map key
    MAP_CREATE(xstake::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY);
    MAP_CREATE(xstake::XPROPERTY_CONTRACT_TABLEBLOCK_NUM_KEY);
}

void xzec_slash_info_contract::summarize_slash_info(std::string const & slash_info) {
    XMETRICS_TIME_RECORD("sysContract_zecSlash_summarize_slash_info");
    auto const & account = SELF_ADDRESS();
    auto const & source_addr = SOURCE_ADDRESS();

    std::string base_addr = "";
    uint32_t table_id = 0;
    XCONTRACT_ENSURE(data::xdatautil::extract_parts(source_addr, base_addr, table_id), "source address extract base_addr or table_id error!");
    xdbg("[xzec_slash_info_contract][summarize_slash_info] self_account %s, source_addr %s, base_addr %s\n", account.c_str(), source_addr.c_str(), base_addr.c_str());
    XCONTRACT_ENSURE(base_addr == top::sys_contract_sharding_slash_info_addr, "invalid source addr's call!");

    xinfo("[xzec_slash_info_contract][summarize_slash_info] table contract report slash info, SOURCE_ADDRESS: %s, pid:%d, ", source_addr.c_str(), getpid());

    xunqualified_node_info_t summarize_info;
    std::string value_str;

    try {
        XMETRICS_TIME_RECORD("sysContract_zecSlash_get_property_contract_unqualified_node_key");
        if (MAP_FIELD_EXIST(xstake::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY, "UNQUALIFIED_NODE"))
            value_str = MAP_GET(xstake::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY, "UNQUALIFIED_NODE");
    } catch (std::runtime_error const & e) {
        xwarn("[xzec_slash_info_contract][summarize_slash_info] read summarized slash info error:%s", e.what());
        throw;
    }

    if (!value_str.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
        summarize_info.serialize_from(stream);
    }

    value_str.clear();
    uint32_t summarize_tableblock_count = 0;
    try {
        XMETRICS_TIME_RECORD("sysContract_zecSlash_get_property_contract_tableblock_num_key");
        if (MAP_FIELD_EXIST(xstake::XPROPERTY_CONTRACT_TABLEBLOCK_NUM_KEY, "TABLEBLOCK_NUM")) {
            value_str = MAP_GET(xstake::XPROPERTY_CONTRACT_TABLEBLOCK_NUM_KEY, "TABLEBLOCK_NUM");
        }
    } catch (std::runtime_error & e) {
        xwarn("[xzec_slash_info_contract][summarize_slash_info] read summarized tableblock num error:%s", e.what());
        throw;
    }

    if (!value_str.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
        stream >> summarize_tableblock_count;
    }

    xunqualified_node_info_t node_info;
    std::uint32_t tableblock_count;
    base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)slash_info.data(), slash_info.size());
    node_info.serialize_from(stream);
    stream >> tableblock_count;

    for (auto const & item : node_info.auditor_info) {
        summarize_info.auditor_info[item.first].block_count += item.second.block_count;
        summarize_info.auditor_info[item.first].subset_count += item.second.subset_count;
    }

    for (auto const & item : node_info.validator_info) {
        summarize_info.validator_info[item.first].block_count += item.second.block_count;
        summarize_info.validator_info[item.first].subset_count += item.second.subset_count;
    }

    summarize_tableblock_count += tableblock_count;

    stream.reset();
    summarize_info.serialize_to(stream);
    {
        XMETRICS_TIME_RECORD("sysContract_zecSlash_set_property_contract_unqualified_node_key");
        MAP_SET(xstake::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY, "UNQUALIFIED_NODE", std::string((char *)stream.data(), stream.size()));
    }
    stream.reset();
    stream << summarize_tableblock_count;
    {
        XMETRICS_TIME_RECORD("sysContract_zecSlash_set_property_contract_tableblock_num_key");
        MAP_SET(xstake::XPROPERTY_CONTRACT_TABLEBLOCK_NUM_KEY, "TABLEBLOCK_NUM", std::string((char *)stream.data(), stream.size()));
    }
    xkinfo("[xzec_slash_info_contract][summarize_slash_info]  table contract report slash info, auditor size: %zu, validator size: %zu, summarized tableblock num: %u, pid: %d",
         summarize_info.auditor_info.size(),
         summarize_info.validator_info.size(),
         summarize_tableblock_count,
         getpid());
}

void xzec_slash_info_contract::do_unqualified_node_slash(common::xlogic_time_t const timestamp) {
    XMETRICS_TIME_RECORD("sysContract_zecSlash_do_unqualified_node_slash");
    auto const & account = SELF_ADDRESS();
    auto const & source_addr = SOURCE_ADDRESS();

    std::string base_addr = "";
    uint32_t table_id = 0;
    XCONTRACT_ENSURE(data::xdatautil::extract_parts(source_addr, base_addr, table_id), "source address extract base_addr or table_id error!");
    xdbg("[xzec_slash_info_contract][do_unqualified_node_slash] self_account %s, source_addr %s, base_addr %s\n", account.c_str(), source_addr.c_str(), base_addr.c_str());

    XCONTRACT_ENSURE(source_addr == account.value() && source_addr == top::sys_contract_zec_slash_info_addr, "invalid source addr's call!");


    xinfo("[xzec_slash_info_contract][do_unqualified_node_slash] do unqualified node slash info, time round: %" PRIu64 ": SOURCE_ADDRESS: %s, pid:%d",
         timestamp,
         SOURCE_ADDRESS().c_str(),
         getpid());

    xunqualified_node_info_t summarize_info;
    std::string value_str;

    // add tableblock num filter
    uint32_t summarize_tableblock_count;
    try {
        XMETRICS_TIME_RECORD("sysContract_zecSlash_get_property_contract_tableblock_num_key");
        if (MAP_FIELD_EXIST(xstake::XPROPERTY_CONTRACT_TABLEBLOCK_NUM_KEY, "TABLEBLOCK_NUM")) {
            value_str = MAP_GET(xstake::XPROPERTY_CONTRACT_TABLEBLOCK_NUM_KEY, "TABLEBLOCK_NUM");
        }
    } catch (std::runtime_error const & e) {
        xwarn("[xzec_slash_info_contract][[do_unqualified_node_slash] read summarized tableblock num error:%s", e.what());
        throw;
    }
    if (!value_str.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
        stream >> summarize_tableblock_count;
        xdbg("[xzec_slash_info_contract][do_unqualified_node_slash]  current summarized tableblock num is: %u", summarize_tableblock_count);
    }
    if (summarize_tableblock_count < XGET_ONCHAIN_GOVERNANCE_PARAMETER(punish_interval_table_block)) {
        xdbg("[xzec_slash_info_contract][do_unqualified_node_slash] summarize_tableblock_count not enought, time round: %" PRIu64
             ": SOURCE_ADDRESS: %s, pid:%d, tableblock_count:%u",
             timestamp,
             SOURCE_ADDRESS().c_str(),
             getpid(),
             summarize_tableblock_count);
        return;
    }

    value_str.clear();
    try {
        XMETRICS_TIME_RECORD("sysContract_zecSlash_get_property_contract_unqualified_node_key");
        if (MAP_FIELD_EXIST(xstake::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY, "UNQUALIFIED_NODE")) {
            value_str = MAP_GET(xstake::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY, "UNQUALIFIED_NODE");
        }
    } catch (std::runtime_error const & e) {
        xwarn("[xzec_slash_info_contract][do_unqualified_node_slash] read summarized slash info error:%s", e.what());
        throw;
    }

    if (!value_str.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)value_str.data(), value_str.size());
        summarize_info.serialize_from(stream);
#ifdef DEBUG
        print_summarize_info(summarize_info);
#endif

        auto node_to_action = filter_nodes(summarize_info);
        xdbg("[xzec_slash_info_contract][filter_slashed_nodes] remove summarize info, time round: %" PRIu64, timestamp);
        // stream.reset();
        // std::vector<std::string> pass_node_to_slash;
        // for (auto const& nodeid: node_to_slash) {
        //     pass_node_to_slash.push_back(nodeid.to_string());
        // }
        if (!node_to_action.empty()) {
            VECTOR_OBJECT_SERIALIZE2(stream, node_to_action);
            std::string punish_node_str = std::string((char *)stream.data(), stream.size());
            {
                stream.reset();
                stream << punish_node_str;
                xinfo("[xzec_slash_info_contract][do_unqualified_node_slash] call register contract slash_unqualified_node, time round: %" PRIu64, timestamp);
                XMETRICS_PACKET_INFO("sysContract_zecSlash", "effective slash timer round", std::to_string(timestamp));
                CALL(common::xaccount_address_t{sys_contract_rec_registration_addr}, "slash_unqualified_node", std::string((char *)stream.data(), stream.size()));
            }
        } else {
            xdbg("[xzec_slash_info_contract][do_unqualified_node_slash] filter slash node empty!");
        }

    } else {
        xwarn("[xzec_slash_info_contract][do_unqualified_node_slash] no summarized slash info");
    }
}

std::vector<xaction_node_info_t> xzec_slash_info_contract::filter_nodes(xunqualified_node_info_t const & summarize_info) {
    std::vector<xaction_node_info_t> nodes_to_slash{};

    if (summarize_info.auditor_info.empty() && summarize_info.validator_info.empty()) {
        xwarn("[xzec_slash_info_contract][filter_slashed_nodes] the summarized node info is empty!");
        return nodes_to_slash;
    } else {
        // summarized info
        nodes_to_slash = filter_helper(summarize_info);
        {
            XMETRICS_TIME_RECORD("sysContract_zecSlash_remove_property_contract_unqualified_node_key");
            MAP_REMOVE(xstake::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY, "UNQUALIFIED_NODE");
        }
        {
            XMETRICS_TIME_RECORD("sysContract_zecSlash_remove_property_contract_tableblock_num_key");
            MAP_REMOVE(xstake::XPROPERTY_CONTRACT_TABLEBLOCK_NUM_KEY, "TABLEBLOCK_NUM");
        }
    }

    xkinfo("[xzec_slash_info_contract][filter_slashed_nodes] the filter node to slash: size: %zu", nodes_to_slash.size());
    return nodes_to_slash;
}

std::vector<xaction_node_info_t> xzec_slash_info_contract::filter_helper(xunqualified_node_info_t const & node_map) {
    std::vector<xaction_node_info_t> res{};

    // do punish filter
    auto node_block_vote = XGET_ONCHAIN_GOVERNANCE_PARAMETER(sign_block_publishment_threshold_value);
    auto node_persent = XGET_ONCHAIN_GOVERNANCE_PARAMETER(sign_block_ranking_publishment_threshold_value);
    auto award_node_block_vote = XGET_ONCHAIN_GOVERNANCE_PARAMETER(sign_block_ranking_reward_threshold_value);
    auto award_node_persent = XGET_ONCHAIN_GOVERNANCE_PARAMETER(sign_block_reward_threshold_value);

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
    // uint32_t slash_size = node_to_slash.size() * node_persent / 100 ?  node_to_slash.size() * node_persent / 100 : 1;
    uint32_t slash_size = node_to_action.size() * node_persent / 100;
    for (size_t i = 0; i < slash_size; ++i) {
        if (node_to_action[i].vote_percent < node_block_vote || 0 == node_to_action[i].vote_percent) {
            res.push_back(xaction_node_info_t{node_to_action[i].node_id, node_to_action[i].node_type});
        }
    }

    uint32_t award_size = node_to_action.size() * award_node_persent / 100;
    for (int i = (int)node_to_action.size() - 1; i >= (int)(node_to_action.size() - award_size); --i) {
        if (node_to_action[i].vote_percent > award_node_block_vote) {
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

    // uint32_t slash_size = node_to_slash.size() * node_persent / 100 ?  node_to_slash.size() * node_persent / 100 : 1;
    slash_size = node_to_action.size() * node_persent / 100;
    for (size_t i = 0; i < slash_size; ++i) {
        if (node_to_action[i].vote_percent < node_block_vote || 0 == node_to_action[i].vote_percent) {
            res.push_back(xaction_node_info_t{node_to_action[i].node_id, node_to_action[i].node_type});
        }
    }

    award_size = node_to_action.size() * award_node_persent / 100;
    for (int i = (int)node_to_action.size() - 1; i >= (int)(node_to_action.size() - award_size); --i) {
        if (node_to_action[i].vote_percent > award_node_block_vote) {
            res.push_back(xaction_node_info_t{node_to_action[i].node_id, node_to_action[i].node_type, false});
        }
    }

    return res;
}

void xzec_slash_info_contract::print_summarize_info(data::xunqualified_node_info_t const & summarize_slash_info) {
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

    xdbg("[xzec_slash_info_contract][print_summarize_info] summarize info: %s", out.c_str());
}

NS_END3

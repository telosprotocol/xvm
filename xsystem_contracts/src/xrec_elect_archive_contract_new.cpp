// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xelection/xrec/xrec_elect_archive_contract_new.h"

#include "xbasic/xutility.h"
#include "xcommon/xaccount_address.h"
#include "xconfig/xconfig_register.h"
#include "xconfig/xpredefined_configurations.h"
#include "xcontract_common/xserialization/xserialization.h"
#include "xdata/xcodec/xmsgpack/xelection_result_store_codec.hpp"
#include "xdata/xcodec/xmsgpack/xstandby_node_info_codec.hpp"
#include "xdata/xcodec/xmsgpack/xstandby_result_store_codec.hpp"
#include "xdata/xelection/xelection_result_property.h"
#include "xdata/xelection/xstandby_node_info.h"
#include "xdata/xnative_contract_address.h"
#include "xdata/xproperty.h"

#include <cinttypes>

#ifdef STATIC_CONSENSUS
#    include "xvm/xsystem_contracts/xelection/xstatic_election_center.h"
#endif

#ifndef XSYSCONTRACT_MODULE
#    define XSYSCONTRACT_MODULE "sysContract_"
#endif
#define XCONTRACT_PREFIX "RecElectArchive_"
#define XARCHIVE_ELECT XSYSCONTRACT_MODULE XCONTRACT_PREFIX

NS_BEG2(top, system_contracts)

using common::xnode_id_t;
using data::election::xelection_result_store_t;
using data::election::xstandby_node_info_t;
using data::election::xstandby_result_store_t;

#ifdef STATIC_CONSENSUS
bool executed_archive{false};
// if enabled static_consensus
// make sure add config in config.xxxx.json
// like this :
//
// "archive_start_nodes":"T00000LNi53Ub726HcPXZfC4z6zLgTo5ks6GzTUp.0.pub_key,T00000LeXNqW7mCCoj23LEsxEmNcWKs8m6kJH446.0.pub_key,T00000LVpL9XRtVdU5RwfnmrCtJhvQFxJ8TB46gB.0.pub_key",
//
// it will elect the first and only round archive nodes as you want.

void xtop_rec_elect_archive_contract_new::elect_config_nodes(common::xlogic_time_t const current_time) {
    uint64_t latest_height = state_height(common::xaccount_address_t{sys_contract_rec_elect_archive_addr});
    xinfo("[archive_start_nodes] get_latest_height: %" PRIu64, latest_height);
    if (latest_height > 0) {
        executed_archive = true;
        return;
    }

    std::map<common::xgroup_id_t, contract_common::properties::xbytes_property_t *> properties{{common::xarchive_group_id, std::addressof(m_archive_result)},
                                                                                               {common::xfull_node_group_id, std::addressof(m_fullnode_result)}};

    using top::data::election::xelection_info_bundle_t;
    using top::data::election::xelection_info_t;
    using top::data::election::xelection_result_store_t;
    using top::data::election::xstandby_node_info_t;

    for (auto index = 0; index < XGET_CONFIG(archive_group_count); ++index) {
        top::common::xgroup_id_t archive_gid{static_cast<top::common::xgroup_id_t::value_type>(common::xarchive_group_id_value_begin + index)};

        auto election_result_store = contract_common::serialization::xmsgpack_t<xelection_result_store_t>::deserialize_from_bytes(properties.at(archive_gid)->value());
        auto & election_network_result = election_result_store.result_of(network_id());
        auto node_type = common::xnode_type_t::archive;
        std::string static_node_str = "";
        if (archive_gid.value() == common::xarchive_group_id_value) {
            node_type = common::xnode_type_t::storage_archive;
            static_node_str = "archive_start_nodes";
        } else if (archive_gid.value() == common::xfull_node_group_id_value) {
            node_type = common::xnode_type_t::storage_full_node;
            static_node_str = "fullnode_start_nodes";
        } else {
            xassert(false);
        }
        auto nodes_info = xvm::system_contracts::xstatic_election_center::instance().get_static_election_nodes(static_node_str);
        if (nodes_info.empty()) {
            xinfo("[archive_start_nodes] get empty node_info: %s gid: %d", static_node_str.c_str(), archive_gid.value());
            continue;
        }
        auto & election_group_result = election_result_store.result_of(network_id()).result_of(node_type).result_of(common::xdefault_cluster_id).result_of(archive_gid);
        for (auto nodes : nodes_info) {
            xelection_info_t new_election_info{};
            new_election_info.consensus_public_key = nodes.pub_key;
            new_election_info.stake = nodes.stake;
            new_election_info.joined_version = common::xelection_round_t{0};

            xelection_info_bundle_t election_info_bundle{};
            election_info_bundle.node_id(nodes.node_id);
            election_info_bundle.election_info(std::move(new_election_info));

            election_group_result.insert(std::move(election_info_bundle));
        }
        election_group_result.election_committee_version(common::xelection_round_t{0});
        election_group_result.timestamp(current_time);
        election_group_result.start_time(current_time);
        if (election_group_result.group_version().empty()) {
            election_group_result.group_version(common::xelection_round_t::max());
        }
        properties.at(archive_gid)->set(contract_common::serialization::xmsgpack_t<xelection_result_store_t>::serialize_to_bytes(election_result_store));
    }
}
#endif

void xtop_rec_elect_archive_contract_new::setup() {
    xelection_result_store_t election_result_store;
    auto const & bytes = contract_common::serialization::xmsgpack_t<xelection_result_store_t>::serialize_to_bytes(election_result_store);

    m_archive_result.set(bytes);
    m_fullnode_result.set(bytes);
}

void xtop_rec_elect_archive_contract_new::on_timer(const uint64_t current_time) {
#ifdef STATIC_CONSENSUS
    if (xvm::system_contracts::xstatic_election_center::instance().if_allow_elect()) {
        if (!executed_archive) {
            elect_config_nodes(current_time);
            return;
        }
#ifndef ELECT_WHEREAFTER
    return;
#endif
    } else {
        return;
    }
#endif

    XMETRICS_TIME_RECORD(XARCHIVE_ELECT "on_timer_all_time");
    XMETRICS_CPU_TIME_RECORD(XARCHIVE_ELECT "on_timer_cpu_time");
    XCONTRACT_ENSURE(sender() == address(), "xrec_elect_archive_contract_t instance is triggled by others");
    XCONTRACT_ENSURE(address().value() == sys_contract_rec_elect_archive_addr,
                     "xrec_elect_archive_contract_t instance is not triggled by sys_contract_rec_elect_archive_addr");
    // XCONTRACT_ENSURE(current_time <= TIME(), "xrec_elect_archive_contract_t::on_timer current_time > consensus leader's time");
    XCONTRACT_ENSURE(current_time + XGET_ONCHAIN_GOVERNANCE_PARAMETER(archive_election_interval) / 2 > time(), "xrec_elect_archive_contract_t::on_timer retried too many times");
    xinfo("xrec_elect_archive_contract_t::archive_elect %" PRIu64, current_time);

    xrange_t<config::xgroup_size_t> archive_group_range{ 1, XGET_ONCHAIN_GOVERNANCE_PARAMETER(max_archive_group_size) };
    xrange_t<config::xgroup_size_t> full_node_group_range{ 0, XGET_ONCHAIN_GOVERNANCE_PARAMETER(max_archive_group_size) };

    auto const & rec_standby_bytes_property = get_property<contract_common::properties::xbytes_property_t>(
        state_accessor::properties::xtypeless_property_identifier_t{data::XPROPERTY_CONTRACT_STANDBYS_KEY}, common::xaccount_address_t{sys_contract_rec_standby_pool_addr});

    auto standby_result_store = contract_common::serialization::xmsgpack_t<xstandby_result_store_t>::deserialize_from_bytes(rec_standby_bytes_property.value());
    auto standby_network_result = standby_result_store.result_of(network_id()).network_result();

#if defined(DEBUG)
    for (auto const & r : standby_network_result) {
        auto const node_type = top::get<common::xnode_type_t const>(r);
        xdbg("xrec_elect_archive_contract_t::archive_elect seeing %s", common::to_string(node_type).c_str());
    }
#endif

    elect_archive(current_time, standby_network_result);
    elect_fullnode(current_time, standby_network_result);
}

common::xnode_type_t xtop_rec_elect_archive_contract_new::standby_type(common::xzone_id_t const & zid, common::xcluster_id_t const & cid, common::xgroup_id_t const & gid) const {
    assert(!broadcast(zid));
    assert(!broadcast(cid));
    assert(!broadcast(gid));

    assert(zid == common::xarchive_zone_id);
    assert(cid == common::xdefault_cluster_id);
    assert(gid == common::xarchive_group_id || gid == common::xfull_node_group_id);

    if (gid == common::xarchive_group_id) {
        return common::xnode_type_t::storage_archive;
    }

    if (gid == common::xfull_node_group_id) {
        return common::xnode_type_t::storage_full_node;
    }

    assert(false);
    return common::xnode_type_t::invalid;
}

void xtop_rec_elect_archive_contract_new::elect_archive(common::xlogic_time_t const current_time, data::election::xstandby_network_result_t const & standby_network_result) {
    xkinfo("[xrec_elect_archive_contract_t] archive_gid: %s, insert %s",
           common::xarchive_group_id.to_string().c_str(),
           data::election::get_property_by_group_id(common::xarchive_group_id).c_str());

    auto election_result_store = contract_common::serialization::xmsgpack_t<xelection_result_store_t>::deserialize_from_bytes(m_archive_result.value());
    auto & election_network_result = election_result_store.result_of(network_id());

    if (elect_group(common::xarchive_zone_id,
                    common::xdefault_cluster_id,
                    common::xarchive_group_id,
                    current_time,
                    current_time,
                    xrange_t<config::xgroup_size_t>{1, XGET_ONCHAIN_GOVERNANCE_PARAMETER(max_archive_group_size)},
                    standby_network_result,
                    election_network_result)) {
        m_archive_result.set(contract_common::serialization::xmsgpack_t<xelection_result_store_t>::serialize_to_bytes(election_result_store));
    }
}

void xtop_rec_elect_archive_contract_new::elect_fullnode(common::xlogic_time_t const current_time, data::election::xstandby_network_result_t const & standby_network_result) {
    xkinfo("[xrec_elect_archive_contract_t] archive_gid: %s, insert %s",
           common::xfull_node_group_id.to_string().c_str(),
           data::election::get_property_by_group_id(common::xfull_node_group_id).c_str());

    auto election_result_store = contract_common::serialization::xmsgpack_t<xelection_result_store_t>::deserialize_from_bytes(m_fullnode_result.value());
    auto & election_network_result = election_result_store.result_of(network_id());

    if (elect_group(common::xarchive_zone_id,
                    common::xdefault_cluster_id,
                    common::xfull_node_group_id,
                    current_time,
                    current_time,
                    xrange_t<config::xgroup_size_t>{0, XGET_ONCHAIN_GOVERNANCE_PARAMETER(max_archive_group_size)},
                    standby_network_result,
                    election_network_result)) {
        m_fullnode_result.set(contract_common::serialization::xmsgpack_t<xelection_result_store_t>::serialize_to_bytes(election_result_store));
    }
}

NS_END2

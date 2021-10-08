// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xelection/xrec/xrec_elect_ec_contract.h"

#include "xcodec/xmsgpack_codec.hpp"
#include "xconfig/xconfig_register.h"
#include "xconfig/xpredefined_configurations.h"
#include "xdata/xcodec/xmsgpack/xparachain_result_codec.hpp"
#include "xdata/xelection/xelection_data_struct.h"
#include "xdata/xelection/xelection_result_property.h"
#include "xdata/xgenesis_data.h"
#include "xdata/xparachain/xparachain_result.h"
#include "xdata/xstandby/xstandby_data_struct.h"
#include "xvm/xserialization/xserialization.h"

#include <cinttypes>

#ifdef STATIC_CONSENSUS
#    include "xvm/xsystem_contracts/xelection/xstatic_election_center.h"
#endif

#ifndef XSYSCONTRACT_MODULE
#    define XSYSCONTRACT_MODULE "sysContract_"
#endif
#define XCONTRACT_PREFIX "RecElectEc_"
#define XEC_ELECT XSYSCONTRACT_MODULE XCONTRACT_PREFIX

NS_BEG4(top, xvm, system_contracts, rec)

using data::election::xelection_result_store_t;
using data::parachain::xparachain_chain_info_t;
using data::parachain::xparachain_result_t;
using data::standby::xrec_standby_result_store_t;

xtop_rec_elect_ec_contract::xtop_rec_elect_ec_contract(common::xnetwork_id_t const & network_id) : xbase_t{network_id} {
}

void xtop_rec_elect_ec_contract::setup() {
    xelection_result_store_t election_result_store;
    STRING_CREATE(data::XPROPERTY_EC_ELECTION_KEY);
    serialization::xmsgpack_t<xelection_result_store_t>::serialize_to_string_prop(*this, data::XPROPERTY_EC_ELECTION_KEY, election_result_store);
}

void xtop_rec_elect_ec_contract::on_timer(common::xlogic_time_t const current_time) {
    XMETRICS_TIME_RECORD(XEC_ELECT "on_timer_all_time");
    XCONTRACT_ENSURE(SOURCE_ADDRESS() == SELF_ADDRESS().value(), "xrec_elect_ec_contract_t instance is triggled by others");
    XCONTRACT_ENSURE(SELF_ADDRESS().value() == sys_contract_rec_elect_ec_addr, "xrec_elect_ec_contract_t instance is not triggled by sys_contract_rec_elect_ec_addr");
    // XCONTRACT_ENSURE(current_time <= TIME(), "xrec_elect_ec_contract_t::on_timer current_time > consensus leader's time");
    XCONTRACT_ENSURE(current_time + XGET_ONCHAIN_GOVERNANCE_PARAMETER(zec_election_interval) / 2 > TIME(),
                     "xrec_elect_ec_contract_t::on_timer retried too many times. TX generated time " + std::to_string(current_time) + " TIME() " + std::to_string(TIME()));

    xparachain_result_t const parachain_result =
        serialization::xmsgpack_t<xparachain_result_t>::deserialize_from_string_prop(*this, sys_contract_rec_parachain_registration_addr, data::XPROPERTY_PARACHAIN_KEY);

    xelection_result_store_t election_result_store = serialization::xmsgpack_t<xelection_result_store_t>::deserialize_from_string_prop(*this, data::XPROPERTY_EC_ELECTION_KEY);

    xrec_standby_result_store_t const rec_standby_result_store =
        serialization::xmsgpack_t<xrec_standby_result_store_t>::deserialize_from_string_prop(*this, sys_contract_rec_standby_pool_addr2, data::XPROPERTY_BEACON_STANDBY_KEY);

    auto & zec_election_result = election_result_store.result_of(network_id());

    bool update = elect_mainnet_ec(current_time, rec_standby_result_store.result_of(network_id()), zec_election_result);

    for (auto const & _p : parachain_result) {
        auto const & chain_id = top::get<const uint32_t>(_p);
        auto const & parachain_info = top::get<data::parachain::xparachain_chain_info_t>(_p);
        xinfo("[xtop_rec_elect_ec_contract::on_timer] get chain_id: %d", chain_id);
        std::string parachain_property_name = std::string{data::XPROPERTY_EC_ELECTION_KEY} + "_" + std::to_string(chain_id);
        if (election_result_store.find(common::xnetwork_id_t{chain_id}) == election_result_store.end()) {
            auto & parachain_election_result = election_result_store.result_of(common::xnetwork_id_t{chain_id});
            // use mainnet ec node create node proxy.
            auto mainnetzec_standby_result = rec_standby_result_store.result_of(network_id());
            for (auto & _p : mainnetzec_standby_result) {
                top::get<standby::xsimple_standby_node_info_t>(_p).stake = 0;
            }
            update |= elect_parachain_genesis(current_time, chain_id, mainnetzec_standby_result, parachain_election_result);
        } else {
            // ? might be some with elect mainnet_ec
            auto & parachain_election_result = election_result_store.result_of(common::xnetwork_id_t{chain_id});
            update |= elect_parachain_ec(current_time, chain_id, rec_standby_result_store.result_of(common::xnetwork_id_t{chain_id}), parachain_election_result);
        }
    }

    if (update) {
        serialization::xmsgpack_t<xelection_result_store_t>::serialize_to_string_prop(*this, data::XPROPERTY_EC_ELECTION_KEY, election_result_store);
    }
    return;

}

bool xtop_rec_elect_ec_contract::elect_mainnet_ec(common::xlogic_time_t const current_time,
                                                  data::standby::xsimple_standby_result_t const & zec_standby_result,
                                                  data::election::xelection_network_result_t & zec_election_result) {
    std::uint64_t random_seed;
    try {
        auto seed = m_contract_helper->get_random_seed();
        random_seed = utl::xxh64_t::digest(seed);
    } catch (std::exception const & eh) {
        xwarn("[xtop_rec_elect_ec_contract::elect_mainnet_ec] get random seed failed: %s", eh.what());
        return false;
    }
    xinfo("[xtop_rec_elect_ec_contract::elect_mainnet_ec] random seed %" PRIu64, random_seed);

    auto const min_election_committee_size = XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_election_committee_size);
    auto const max_election_committee_size = XGET_ONCHAIN_GOVERNANCE_PARAMETER(max_election_committee_size);

    auto start_time = current_time;
    auto const & current_group_nodes = zec_election_result.result_of(common::xnode_type_t::zec).result_of(common::xcommittee_cluster_id).result_of(common::xcommittee_group_id);
    if (!current_group_nodes.empty()) {
        auto const zec_election_interval = XGET_ONCHAIN_GOVERNANCE_PARAMETER(zec_election_interval);
        start_time += zec_election_interval / 2;
    }
    return elect_group(common::xzec_zone_id,
                       common::xcommittee_cluster_id,
                       common::xcommittee_group_id,
                       current_time,
                       start_time,
                       random_seed,
                       xrange_t<config::xgroup_size_t>{min_election_committee_size, max_election_committee_size},
                       zec_standby_result,
                       zec_election_result);
}

bool xtop_rec_elect_ec_contract::elect_parachain_genesis(common::xlogic_time_t const current_time,
                                                         uint32_t chain_id,
                                                         data::standby::xsimple_standby_result_t const & zec_standby_result,
                                                         data::election::xelection_network_result_t & zec_election_result) {
    std::uint64_t random_seed;
    try {
        auto seed = m_contract_helper->get_random_seed();
        random_seed = utl::xxh64_t::digest(seed);
    } catch (std::exception const & eh) {
        xwarn("[xtop_rec_elect_ec_contract::elect_parachain_genesis] get random seed failed: %s", eh.what());
        return false;
    }
    xinfo("[xtop_rec_elect_ec_contract::elect_parachain_genesis] random seed %" PRIu64, random_seed);

    // ? parachain ec genesis round size might be some other tcc params. Use this anyway.
    auto const min_election_committee_size = XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_election_committee_size);
    auto const max_election_committee_size = XGET_ONCHAIN_GOVERNANCE_PARAMETER(max_election_committee_size);

    auto start_time = current_time;
    auto const & current_group_nodes = zec_election_result.result_of(common::xnode_type_t::zec).result_of(common::xcommittee_cluster_id).result_of(common::xcommittee_group_id);
    if (!current_group_nodes.empty()) {
        auto const zec_election_interval = XGET_ONCHAIN_GOVERNANCE_PARAMETER(zec_election_interval);
        start_time += zec_election_interval / 2;
    }
    return elect_group(common::xzec_zone_id,
                       common::xcommittee_cluster_id,
                       common::xcommittee_group_id,
                       current_time,
                       start_time,
                       random_seed,
                       xrange_t<config::xgroup_size_t>{min_election_committee_size, max_election_committee_size},
                       zec_standby_result,
                       zec_election_result);
}

bool xtop_rec_elect_ec_contract::elect_parachain_ec(common::xlogic_time_t const current_time,
                                                    uint32_t chain_id,
                                                    data::standby::xsimple_standby_result_t const & zec_standby_result,
                                                    data::election::xelection_network_result_t & zec_election_result) {
    std::uint64_t random_seed;
    try {
        auto seed = m_contract_helper->get_random_seed();
        random_seed = utl::xxh64_t::digest(seed);
    } catch (std::exception const & eh) {
        xwarn("[xtop_rec_elect_ec_contract::elect_mainnet_ec] get random seed failed: %s", eh.what());
        return false;
    }
    xinfo("[xtop_rec_elect_ec_contract::elect_mainnet_ec] random seed %" PRIu64, random_seed);

    // ? might need define new params
    auto const min_election_committee_size = XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_election_committee_size);
    auto const max_election_committee_size = XGET_ONCHAIN_GOVERNANCE_PARAMETER(max_election_committee_size);

    auto start_time = current_time;
    auto const & current_group_nodes = zec_election_result.result_of(common::xnode_type_t::zec).result_of(common::xcommittee_cluster_id).result_of(common::xcommittee_group_id);
    if (!current_group_nodes.empty()) {
        auto const zec_election_interval = XGET_ONCHAIN_GOVERNANCE_PARAMETER(zec_election_interval);
        start_time += zec_election_interval / 2;
    }
    return elect_group(common::xzec_zone_id,
                       common::xcommittee_cluster_id,
                       common::xcommittee_group_id,
                       current_time,
                       start_time,
                       random_seed,
                       xrange_t<config::xgroup_size_t>{min_election_committee_size, max_election_committee_size},
                       zec_standby_result,
                       zec_election_result);
}
NS_END4

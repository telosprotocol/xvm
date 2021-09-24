// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xelection/xrec/xrec_standby_pool_contract2.h"

#include "xbasic/xcrypto_key.h"
#include "xbasic/xutility.h"
#include "xcodec/xmsgpack_codec.hpp"
#include "xcommon/xregistration_type.h"
#include "xdata/xnative_contract_address.h"
#include "xdata/xregistration/xregistration_data_struct.h"
#include "xdata/xrootblock.h"
#include "xdata/xstandby/xstandby_data_struct.h"
#include "xvm/xserialization/xserialization.h"

#ifndef XSYSCONTRACT_MODULE
#    define XSYSCONTRACT_MODULE "sysContract_"
#endif
#define XCONTRACT_PREFIX "RecStandby_"
#define XREC_STANDBY XSYSCONTRACT_MODULE XCONTRACT_PREFIX

using namespace top::data;

NS_BEG4(top, xvm, system_contracts, rec)

using data::standby::xrec_standby_chain_result_t;
using data::standby::xrec_standby_node_info_t;
using data::standby::xrec_standby_result_store_t;

xtop_rec_standby_pool_contract2::xtop_rec_standby_pool_contract2(common::xnetwork_id_t const & network_id) : xbase_t{network_id} {
}

void xtop_rec_standby_pool_contract2::setup() {
    xrec_standby_result_store_t standby_result_store;
    const std::vector<node_info_t> & seed_nodes = data::xrootblock_t::get_seed_nodes();
    for (size_t i = 0u; i < seed_nodes.size(); i++) {
        auto const & node_data = seed_nodes[i];

        common::xnode_id_t node_id{node_data.m_account};

        xrec_standby_node_info_t seed_node_info;
        seed_node_info.public_key = xpublic_key_t{node_data.m_publickey};
        seed_node_info.ec_stake = 0;
        seed_node_info.program_version = "1.1.0";
        seed_node_info.is_genesis_node = true;

        standby_result_store.result_of(network_id()).insert({node_id, seed_node_info});
    }

    STRING_CREATE(XPROPERTY_BEACON_STANDBY_KEY);
    serialization::xmsgpack_t<xrec_standby_result_store_t>::serialize_to_string_prop(*this, XPROPERTY_BEACON_STANDBY_KEY, standby_result_store);
}

void xtop_rec_standby_pool_contract2::nodeJoinNetwork(common::xaccount_address_t const & node_id,
                                                      common::xnetwork_id_t const & joined_network_id,
                                                      std::string const & program_version) {
    XMETRICS_TIME_RECORD(XREC_STANDBY "add_node_all_time");

    std::string result = STRING_GET2(data::XPORPERTY_BEACON_REG_KEY, sys_contract_rec_registration_addr2);

    XCONTRACT_ENSURE(!result.empty(), "[xtop_rec_standby_pool_contract2::nodeJoinNetwork] get beacon registration info fail");

    auto const & registration_result_store = serialization::xmsgpack_t<data::registration::xregistration_result_store_t>::deserialize_from_string_prop(result);
    XCONTRACT_ENSURE(!registration_result_store.empty(), "[xtop_rec_standby_pool_contract2::nodeJoinNetwork] registration_result_store empty");

    auto const & registration_chain_result = registration_result_store.result_of(joined_network_id);
    XCONTRACT_ENSURE(!registration_chain_result.empty(), "[xtop_rec_standby_pool_contract2::nodeJoinNetwork] registration_chain_result empty");

    XCONTRACT_ENSURE(registration_chain_result.end() != registration_chain_result.find(node_id),
                     "[xtop_rec_standby_pool_contract2::nodeJoinNetwork] did not find node " + node_id.to_string() + " in registration_chain_result, chain_id " +
                         common::to_string(joined_network_id));

    auto const & rec_registration_node_info = registration_chain_result.result_of(node_id);

    auto standby_result_store = serialization::xmsgpack_t<xrec_standby_result_store_t>::deserialize_from_string_prop(*this, XPROPERTY_BEACON_STANDBY_KEY);

    auto & standby_chain_result = standby_result_store.result_of(joined_network_id);

    bool update_standby{false};
    update_standby = nodeJoinNetworkImpl(node_id, program_version, rec_registration_node_info, standby_chain_result);

    if (update_standby) {
        XMETRICS_PACKET_INFO(XREC_STANDBY "nodeJoinNetwork", "node_id", node_id.value(), "registration_type", common::to_string(rec_registration_node_info.m_registration_type));
        serialization::xmsgpack_t<xrec_standby_result_store_t>::serialize_to_string_prop(*this, XPROPERTY_BEACON_STANDBY_KEY, standby_result_store);
    }
}

bool xtop_rec_standby_pool_contract2::nodeJoinNetworkImpl(common::xaccount_address_t const & node_id,
                                                          std::string const & program_version,
                                                          data::registration::xrec_registration_node_info_t const & registration_node_info,
                                                          data::standby::xrec_standby_chain_result_t & standby_chain_result) {
    XCONTRACT_ENSURE(common::has<common::xregistration_type_t::senior>(registration_node_info.m_registration_type),
                     "[xtop_rec_standby_pool_contract2::nodeJoinNetworkImpl] registration_type should has senior type");

    // might add some minimum stake require.
    // XCONTRACT_ENSURE(registration_node_info.m_account_mortgage > xxx , "");

    data::standby::xrec_standby_node_info_t standby_node_info;

    standby_node_info.ec_stake = registration_node_info.m_account_mortgage;
    standby_node_info.is_genesis_node = false;
    standby_node_info.program_version = program_version;
    standby_node_info.public_key = registration_node_info.m_public_key;

    return standby_chain_result.insert(std::make_pair(node_id, standby_node_info)).second;
}

bool xtop_rec_standby_pool_contract2::update_node_info(data::registration::xrec_registration_node_info_t const & registration_node_info,
                                                       data::standby::xrec_standby_node_info_t & standby_node_info) const {
    bool updated{false};

    if (standby_node_info.ec_stake != registration_node_info.m_account_mortgage) {
        standby_node_info.ec_stake = registration_node_info.m_account_mortgage;
        updated |= true;
    }
    if (standby_node_info.public_key != registration_node_info.m_public_key) {
        standby_node_info.public_key = registration_node_info.m_public_key;
        updated |= true;
    }

    return updated;
}

bool xtop_rec_standby_pool_contract2::update_standby_result_store(data::registration::xregistration_result_store_t const & registration_result_store,
                                                                  data::standby::xrec_standby_result_store_t & standby_result_store) const {
    bool updated{false};

    for (auto & chain_pair : standby_result_store) {
        auto const & _network_id = top::get<const common::xnetwork_id_t>(chain_pair);
        auto & _standby_chain_result = top::get<data::standby::xrec_standby_chain_result_t>(chain_pair);
        auto & _registration_chain_result = registration_result_store.result_of(_network_id);
        for (auto node_iter = _standby_chain_result.begin(); node_iter != _standby_chain_result.end();) {
            auto const & _node_id = top::get<const common::xnode_id_t>(*node_iter);
            auto registration_info_iter = _registration_chain_result.find(_node_id);
            if (registration_info_iter == _registration_chain_result.end() ||
                (!common::has<common::xregistration_type_t::senior>(registration_info_iter->second.m_registration_type))) {
                XMETRICS_PACKET_INFO(XREC_STANDBY "nodeLeaveNetwork", "node_id", _node_id.to_string(), "reason", "dereg");
                node_iter = _standby_chain_result.erase(node_iter);
                updated |= true;
            } else {
                auto & _standby_node_info = top::get<data::standby::xrec_standby_node_info_t>(*node_iter);
                auto const & registration_node_info = top::get<data::registration::xrec_registration_node_info_t>(*registration_info_iter);
                updated |= update_node_info(registration_node_info, _standby_node_info);
            }
            node_iter++;
        }
    }
    return updated;
}

void xtop_rec_standby_pool_contract2::on_timer(common::xlogic_time_t const current_time) {
    XMETRICS_TIME_RECORD(XREC_STANDBY "on_timer_all_time");
    XCONTRACT_ENSURE(SOURCE_ADDRESS() == SELF_ADDRESS().value(), "[xtop_rec_standby_pool_contract2::on_timer] instance is triggled by others");
    XCONTRACT_ENSURE(SELF_ADDRESS().value() == sys_contract_rec_standby_pool_addr2,
                     "[xtop_rec_standby_pool_contract2::on_timer] instance is not triggled by xrec_standby_pool_contract_t");
    XCONTRACT_ENSURE(current_time <= TIME(), "[xtop_rec_standby_pool_contract2::on_timer] current_time > consensus leader's time");

    // reg data
    std::string result = STRING_GET2(data::XPORPERTY_BEACON_REG_KEY, sys_contract_rec_registration_addr2);
    XCONTRACT_ENSURE(!result.empty(), "[xtop_rec_standby_pool_contract2::on_timer] get beacon registration info fail");
    auto const & registration_result_store = serialization::xmsgpack_t<data::registration::xregistration_result_store_t>::deserialize_from_string_prop(result);
    XCONTRACT_ENSURE(!registration_result_store.empty(), "[xtop_rec_standby_pool_contract2::on_timer] registration_result_store empty");

    auto standby_result_store = serialization::xmsgpack_t<xrec_standby_result_store_t>::deserialize_from_string_prop(*this, XPROPERTY_BEACON_STANDBY_KEY);

    bool update_standby{false};
    update_standby = update_standby_result_store(registration_result_store, standby_result_store);

    if (update_standby) {
        xdbg("[xtop_rec_standby_pool_contract2::on_timer] standby pool updated");
        serialization::xmsgpack_t<xrec_standby_result_store_t>::serialize_to_string_prop(*this, XPROPERTY_BEACON_STANDBY_KEY, standby_result_store);
    }
}

NS_END4

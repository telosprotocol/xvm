// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xelection/xzec/xzec_standby_pool_contract2.h"

#include "xbasic/xcrypto_key.h"
#include "xbasic/xutility.h"
#include "xcodec/xmsgpack_codec.hpp"
#include "xcommon/xnode_type.h"
#include "xcommon/xregistration_type.h"
#include "xdata/xnative_contract_address.h"
#include "xdata/xregistration/xregistration_data_struct.h"
#include "xdata/xrootblock.h"
#include "xdata/xstandby/xstandby_data_struct.h"
#include "xvm/xserialization/xserialization.h"

#ifndef XSYSCONTRACT_MODULE
#    define XSYSCONTRACT_MODULE "sysContract_"
#endif
#define XCONTRACT_PREFIX "ZecStandby_"
#define XZEC_STANDBY XSYSCONTRACT_MODULE XCONTRACT_PREFIX

using namespace top::data;

NS_BEG4(top, xvm, system_contracts, zec)

using data::standby::xzec_standby_node_info_t;
using data::standby::xzec_standby_result_t;
using data::standby::xzec_standby_stake_container;

xtop_zec_standby_pool_contract2::xtop_zec_standby_pool_contract2(common::xnetwork_id_t const & network_id) : xbase_t{network_id} {
}

void xtop_zec_standby_pool_contract2::setup() {
    xzec_standby_result_t standby_result_store;
    const std::vector<node_info_t> & seed_nodes = data::xrootblock_t::get_seed_nodes();
    for (size_t i = 0u; i < seed_nodes.size(); i++) {
        auto const & node_data = seed_nodes[i];

        common::xnode_id_t node_id{node_data.m_account};

        xzec_standby_node_info_t seed_node_info;
        seed_node_info.public_key = xpublic_key_t{node_data.m_publickey};
        seed_node_info.stake_container.insert({common::xnode_type_t::storage_archive, 0});
        seed_node_info.stake_container.insert({common::xnode_type_t::consensus_auditor, 0});
        seed_node_info.stake_container.insert({common::xnode_type_t::consensus_validator, 0});
        seed_node_info.stake_container.insert({common::xnode_type_t::edge, 0});
        seed_node_info.program_version = "1.1.0";
        seed_node_info.is_genesis_node = true;

        standby_result_store.insert({node_id, seed_node_info});
    }

    STRING_CREATE(XPROPERTY_ZEC_STANDBY_KEY);
    serialization::xmsgpack_t<xzec_standby_result_t>::serialize_to_string_prop(*this, XPROPERTY_ZEC_STANDBY_KEY, standby_result_store);
}

void xtop_zec_standby_pool_contract2::nodeJoinNetwork(common::xaccount_address_t const & node_id,
                                                      common::xnetwork_id_t const & joined_network_id,
                                                      std::string const & program_version) {
    XMETRICS_TIME_RECORD(XZEC_STANDBY "add_node_all_time");

    std::string result = STRING_GET2(data::XPORPERTY_ZEC_REG_KEY, sys_contract_zec_registration_addr);

    XCONTRACT_ENSURE(!result.empty(), "[xtop_zec_standby_pool_contract2::nodeJoinNetwork] get zec registration info fail");

    auto const & zec_registration_result = serialization::xmsgpack_t<data::registration::xzec_registration_result_t>::deserialize_from_string_prop(result);
    XCONTRACT_ENSURE(!zec_registration_result.empty(), "[xtop_zec_standby_pool_contract2::nodeJoinNetwork] zec_registration_result empty");

    XCONTRACT_ENSURE(zec_registration_result.end() != zec_registration_result.find(node_id),
                     "[xtop_zec_standby_pool_contract2::nodeJoinNetwork] did not find node " + node_id.to_string() + " in zec_registration_result");

    auto const & zec_registration_node_info = zec_registration_result.result_of(node_id);

    auto zec_standby_result = serialization::xmsgpack_t<xzec_standby_result_t>::deserialize_from_string_prop(*this, XPROPERTY_ZEC_STANDBY_KEY);

    bool update_standby{false};
    update_standby = nodeJoinNetworkImpl(node_id, program_version, zec_registration_node_info, zec_standby_result);

    if (update_standby) {
        XMETRICS_PACKET_INFO(XZEC_STANDBY "nodeJoinNetwork", "node_id", node_id.value(), "role_type", common::to_string(zec_registration_node_info.m_role_type));
        serialization::xmsgpack_t<xzec_standby_result_t>::serialize_to_string_prop(*this, XPROPERTY_ZEC_STANDBY_KEY, zec_standby_result);
    }
}

bool xtop_zec_standby_pool_contract2::nodeJoinNetworkImpl(common::xaccount_address_t const & node_id,
                                                          std::string const & program_version,
                                                          data::registration::xzec_registration_node_info_t const & zec_registration_node_info,
                                                          data::standby::xzec_standby_result_t & zec_standby_result) {
    bool is_auditor{zec_registration_node_info.is_valid_auditor_node()};
    bool is_validator{zec_registration_node_info.is_validator_node()};
    bool is_edge{zec_registration_node_info.is_edge_node()};
    bool is_archive{zec_registration_node_info.is_archive_node()};
    bool is_full_node{zec_registration_node_info.is_full_node()};
    uint64_t auditor_stake{0}, validator_stake{0}, edge_stake{0}, archive_stake{0}, full_node_stake{0};

    if (is_auditor) {
        auditor_stake = zec_registration_node_info.auditor_stake();
    }
    if (is_validator) {
        validator_stake = zec_registration_node_info.validator_stake();
    }
    if (is_edge) {
        edge_stake = zec_registration_node_info.edge_stake();
    }
    if (is_archive) {
        archive_stake = zec_registration_node_info.archive_stake();
    }
    if (is_full_node) {
        full_node_stake = zec_registration_node_info.full_node_stake();
    }

    auto role_type = zec_registration_node_info.m_role_type;
    XCONTRACT_ENSURE(role_type != common::xrole_type_t::invalid, "[xtop_zec_standby_pool_contract2::nodeJoinNetworkImpl] fail: invalid role");
    XCONTRACT_ENSURE(zec_registration_node_info.get_required_min_deposit() <= zec_registration_node_info.account_mortgage(),
                     "[xtop_zec_standby_pool_contract2::nodeJoinNetworkImpl] account mortgage < required_min_deposit fail: " + node_id.to_string() +
                         ", role_type : " + common::to_string(role_type));

    xdbg("[xtop_zec_standby_pool_contract2::nodeJoinNetworkImpl] %s", node_id.to_string().c_str());

    data::standby::xzec_standby_node_info_t standby_node_info;

    if (is_auditor) {
        standby_node_info.stake_container.insert(std::make_pair(common::xnode_type_t::consensus_auditor, auditor_stake));
    }
    if (is_validator) {
        standby_node_info.stake_container.insert(std::make_pair(common::xnode_type_t::consensus_validator, validator_stake));
    }
    if (is_edge) {
        standby_node_info.stake_container.insert(std::make_pair(common::xnode_type_t::edge, edge_stake));
    }
    if (is_archive) {
        standby_node_info.stake_container.insert(std::make_pair(common::xnode_type_t::storage_archive, archive_stake));
    }
    if (is_full_node) {
        standby_node_info.stake_container.insert(std::make_pair(common::xnode_type_t::storage_full_node, full_node_stake));
    }
    standby_node_info.program_version = program_version;
    standby_node_info.is_genesis_node = zec_registration_node_info.is_genesis_node();
    standby_node_info.public_key = zec_registration_node_info.public_key();

    return zec_standby_result.insert(std::make_pair(node_id, standby_node_info)).second;
}

bool xtop_zec_standby_pool_contract2::update_node_info(data::registration::xzec_registration_node_info_t const & registration_node_info,
                                                       data::standby::xzec_standby_node_info_t & standby_node_info) const {
    data::standby::xzec_standby_node_info_t new_standby_node_info;
    if (registration_node_info.is_valid_auditor_node()) {
        new_standby_node_info.stake_container.insert(std::make_pair(common::xnode_type_t::consensus_auditor, registration_node_info.auditor_stake()));
    }
    if (registration_node_info.is_validator_node()) {
        new_standby_node_info.stake_container.insert(std::make_pair(common::xnode_type_t::consensus_validator, registration_node_info.validator_stake()));
    }
    if (registration_node_info.is_edge_node()) {
        new_standby_node_info.stake_container.insert(std::make_pair(common::xnode_type_t::edge, registration_node_info.edge_stake()));
    }
    if (registration_node_info.is_archive_node()) {
        new_standby_node_info.stake_container.insert(std::make_pair(common::xnode_type_t::storage_archive, registration_node_info.archive_stake()));
    }
    if (registration_node_info.is_full_node()) {
        new_standby_node_info.stake_container.insert(std::make_pair(common::xnode_type_t::storage_full_node, registration_node_info.full_node_stake()));
    }

    new_standby_node_info.is_genesis_node = registration_node_info.is_genesis_node();
    new_standby_node_info.public_key = registration_node_info.public_key();
    new_standby_node_info.program_version = standby_node_info.program_version;

    if (new_standby_node_info != standby_node_info) {
        standby_node_info = new_standby_node_info;
        return true;
    }
    return false;
}

bool xtop_zec_standby_pool_contract2::update_standby_result(data::registration::xzec_registration_result_t const & zec_registration_result,
                                                            data::standby::xzec_standby_result_t & zec_standby_result) const {
    bool update{false};
    for (auto node_iter = zec_standby_result.begin(); node_iter != zec_standby_result.end();) {
        auto const & _node_id = top::get<const common::xnode_id_t>(*node_iter);

        auto registration_info_iter = zec_registration_result.find(_node_id);
        if (registration_info_iter == zec_registration_result.end()) {
            XMETRICS_PACKET_INFO(XZEC_STANDBY "nodeLeaveNetwork", "node_id", _node_id.to_string(), "reason", "dereg");
            node_iter = zec_standby_result.erase(node_iter);
            update |= true;
        } else {
            auto & standby_node_info = top::get<data::standby::xzec_standby_node_info_t>(*node_iter);
            auto const & registration_node_info = top::get<data::registration::xzec_registration_node_info_t>(*registration_info_iter);
            update |= update_node_info(registration_node_info, standby_node_info);
        }
        node_iter++;
    }
    return update;
}

void xtop_zec_standby_pool_contract2::on_timer(common::xlogic_time_t const current_time) {
    XMETRICS_TIME_RECORD(XZEC_STANDBY "on_timer_all_time");
    XCONTRACT_ENSURE(SOURCE_ADDRESS() == SELF_ADDRESS().value(), "[xtop_zec_standby_pool_contract2::on_timer] instance is triggled by others");
    XCONTRACT_ENSURE(SELF_ADDRESS().value() == sys_contract_zec_standby_pool_addr2,
                     "[xtop_zec_standby_pool_contract2::on_timer] instance is not triggled by xzec_standby_pool_contract_t");
    XCONTRACT_ENSURE(current_time <= TIME(), "[xtop_zec_standby_pool_contract2::on_timer] current_time > consensus leader's time");

    // reg data
    std::string result = STRING_GET2(data::XPORPERTY_ZEC_REG_KEY, sys_contract_zec_registration_addr);
    XCONTRACT_ENSURE(!result.empty(), "[xtop_zec_standby_pool_contract2::on_timer] get beacon registration info fail");
    auto const & zec_registration_result = serialization::xmsgpack_t<data::registration::xzec_registration_result_t>::deserialize_from_string_prop(result);
    XCONTRACT_ENSURE(!zec_registration_result.empty(), "[xtop_zec_standby_pool_contract2::on_timer] zec_registration_result empty");

    auto zec_standby_result = serialization::xmsgpack_t<xzec_standby_result_t>::deserialize_from_string_prop(*this, XPROPERTY_ZEC_STANDBY_KEY);

    if (update_standby_result(zec_registration_result, zec_standby_result)) {
        xdbg("[xtop_zec_standby_pool_contract2::on_timer] standby pool updated");
        serialization::xmsgpack_t<xzec_standby_result_t>::serialize_to_string_prop(*this, XPROPERTY_ZEC_STANDBY_KEY, zec_standby_result);
    }
}

NS_END4

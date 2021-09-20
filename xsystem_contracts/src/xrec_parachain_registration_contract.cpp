// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xparachain/xrec_parachain_registration_contract.h"

#include "xbasic/xutility.h"
#include "xdata/xcodec/xmsgpack/xparachain_result_codec.hpp"
#include "xvm/xserialization/xserialization.h"

NS_BEG4(top, xvm, system_contracts, rec)

#ifndef XSYSCONTRACT_MODULE
#    define XSYSCONTRACT_MODULE "sysContract_"
#endif
#define XCONTRACT_PREFIX "RecParachain"
#define XREC_PARACHAIN_REG XSYSCONTRACT_MODULE XCONTRACT_PREFIX

using top::data::parachain::xparachain_chain_info_t;
using top::data::parachain::xparachain_result_t;

xtop_rec_parachain_registration_contract::xtop_rec_parachain_registration_contract(common::xnetwork_id_t const & network_id) : base_t{network_id} {
}

void xtop_rec_parachain_registration_contract::setup() {
    STRING_CREATE(XPROPERTY_PARACHAIN_KEY);
    xparachain_result_t parachain_result;
    serialization::xmsgpack_t<xparachain_result_t>::serialize_to_string_prop(*this, XPROPERTY_PARACHAIN_KEY, parachain_result);
}

void xtop_rec_parachain_registration_contract::on_timer(common::xlogic_time_t const current_time) {
    // test tmp .
    auto parachain_result = serialization::xmsgpack_t<xparachain_result_t>::deserialize_from_string_prop(*this, XPROPERTY_PARACHAIN_KEY);
    xinfo("[xtop_rec_parachain_registration_contract] parachain_result , size: %zu", parachain_result.size());

    for (auto _info : parachain_result.results()) {
        auto const & chain_id = top::get<const uint32_t>(_info);
        auto const & chain_info = top::get<xparachain_chain_info_t>(_info);
        xinfo("[xtop_rec_parachain_registration_contract] parachain_result.info: id:%u name:%s node_size:%zu",
              chain_id,
              chain_info.chain_name.c_str(),
              chain_info.genesis_node_info.size());
    }
}

void xtop_rec_parachain_registration_contract::register_parachain(std::string chain_name, uint32_t chain_id, uint32_t genesis_node_size) {
    XCONTRACT_ENSURE(chain_id > 0 && chain_id < 255, "[xtop_rec_parachain_registration_contract][register_parachain]chain_id should between 1-254");
    XCONTRACT_ENSURE(chain_name.size() > 3 && chain_name.size() < 13, "[xtop_rec_parachain_registration_contract][register_parachain]chain_name length should between 4-12");

    auto parachain_result = serialization::xmsgpack_t<xparachain_result_t>::deserialize_from_string_prop(*this, XPROPERTY_PARACHAIN_KEY);
    XCONTRACT_ENSURE(parachain_result.empty(chain_id), "[xtop_rec_parachain_registration_contract][register_parachain]chain id has been register");

    xparachain_chain_info_t chain_info;
    std::vector<common::xnode_id_t> genesis_nodes;
    // get genesis_nodes;
    // do_some_func(genesis_nodes_size)

    chain_info.genesis_node_info = genesis_nodes;
    chain_info.chain_name = chain_name;
    chain_info.chain_id = chain_id;

    parachain_result.insert({chain_id, chain_info});
    serialization::xmsgpack_t<xparachain_result_t>::serialize_to_string_prop(*this, XPROPERTY_PARACHAIN_KEY, parachain_result);
}

NS_END4
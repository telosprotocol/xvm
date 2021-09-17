// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xregistration/xzec_registration_contract.h"

#include "xcommon/xrole_type.h"
#include "xconfig/xconfig_register.h"
#include "xdata/xproperty.h"
#include "xdata/xregistration/xregistration_data_struct.h"
#include "xdata/xrootblock.h"
#include "xmetrics/xmetrics.h"
#include "xvm/xserialization/xserialization.h"

using top::base::xcontext_t;
using top::base::xstream_t;

#if !defined(XZEC_MODULE)
#    define XZEC_MODULE "sysContract_"
#endif
#define XCONTRACT_PREFIX "zec_registration_"

#define XREG_CONTRACT XZEC_MODULE XCONTRACT_PREFIX

NS_BEG4(top, xvm, system_contracts, zec)

xtop_zec_registration_contract::xtop_zec_registration_contract(common::xnetwork_id_t const & network_id) : xbase_t{network_id} {
}

void xtop_zec_registration_contract::setup() {
    STRING_CREATE(XPORPERTY_ZEC_REG_KEY);
    data::registration::xzec_registration_result_t zec_registration_result_store;

    std::vector<node_info_t> const & seed_nodes = data::xrootblock_t::get_seed_nodes();

    for (size_t i = 0; i < seed_nodes.size(); i++) {
        node_info_t node_data = seed_nodes[i];
        common::xnode_id_t node_id = node_data.m_account;
        data::registration::xrec_registration_node_info_t node_info;
        node_info.is_genesis_node = true;
        node_info.m_account_mortgage = 0;
        node_info.m_public_key = node_data.m_publickey;
        node_info.m_registration_type = common::xregistration_type_t::hardcode;

        data::registration::xzec_registration_node_info_t zec_node_info;
        zec_node_info.m_rec_registration_node_info = node_info;
        zec_node_info.m_nickname = std::string("bootnode") + std::to_string(i + 1);
        zec_node_info.m_role_type = common::xrole_type_t::edge | common::xrole_type_t::advance | common::xrole_type_t::validator;

        zec_registration_result_store.insert({node_id, zec_node_info});
    }

    serialization::xmsgpack_t<data::registration::xzec_registration_result_t>::serialize_to_string_prop(*this, XPORPERTY_ZEC_REG_KEY, zec_registration_result_store);
}

void xtop_zec_registration_contract::on_timer(common::xlogic_time_t current_time) {
    // assert(false);
    // todo
}

NS_END4

#undef XREG_CONTRACT
#undef XCONTRACT_PREFIX
#undef XZEC_MODULE

// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xregistration/xrec_registration_contract2.h"

#include "xcommon/xrole_type.h"
#include "xconfig/xconfig_register.h"
#include "xdata/xproperty.h"
#include "xdata/xregistration/xregistration_data_struct.h"
#include "xdata/xrootblock.h"
#include "xmetrics/xmetrics.h"
#include "xvm/xserialization/xserialization.h"

using top::base::xcontext_t;
using top::base::xstream_t;
using namespace top::data::registration;

#if !defined(XREC_MODULE)
#    define XREC_MODULE "sysContract_"
#endif
#define XCONTRACT_PREFIX "rec_registration_"

#define XREG_CONTRACT XREC_MODULE XCONTRACT_PREFIX

NS_BEG4(top, xvm, system_contracts, rec)

xtop_rec_registration_contract2::xtop_rec_registration_contract2(common::xnetwork_id_t const & network_id) : xbase_t{network_id} {
}

void xtop_rec_registration_contract2::setup() {
    STRING_CREATE(XPORPERTY_BEACON_REG_KEY);
    xregistration_result_store_t registration_result_store;

    common::xnetwork_id_t network_id{top::config::to_chainid(XGET_CONFIG(chain_name))};
    std::vector<node_info_t> const & seed_nodes = data::xrootblock_t::get_seed_nodes();
    auto & registration_chain_result = registration_result_store.result_of(network_id);

    for (size_t i = 0; i < seed_nodes.size(); i++) {
        node_info_t node_data = seed_nodes[i];
        common::xnode_id_t node_id = node_data.m_account;
        xrec_registration_node_info_t node_info;
        node_info.is_genesis_node = true;
        node_info.m_account_mortgage = 0;
        node_info.m_public_key = node_data.m_publickey;
        node_info.m_registration_type = common::xregistration_type_t::hardcode;

        registration_chain_result.insert({node_id, node_info});
    }

    serialization::xmsgpack_t<xregistration_result_store_t>::serialize_to_string_prop(*this, XPORPERTY_BEACON_REG_KEY, registration_result_store);
}

void xtop_rec_registration_contract2::registerNode(common::xnetwork_id_t const & network_id,
                                                   std::string const & registration_type,
                                                   std::string const & signing_key
#if defined XENABLE_MOCK_ZEC_STAKE
                                                   ,
                                                   common::xaccount_address_t const & registration_account
#endif
) {

#if defined XENABLE_MOCK_ZEC_STAKE
    auto const & account = registration_account;
#else
    auto const & account = common::xaccount_address_t{SOURCE_ADDRESS()};
#endif

    xrec_registration_node_info_t node_info;

    const xtransaction_ptr_t trans_ptr = GET_TRANSACTION();
    XCONTRACT_ENSURE(trans_ptr->get_source_action().get_action_type() == xaction_type_asset_out && !account.empty(),
                     "xtop_rec_registration_contract2::registerNode: source_action type must be xaction_type_asset_out and account must be not empty");

    xstream_t stream(xcontext_t::instance(), (uint8_t *)trans_ptr->get_source_action().get_action_param().data(), trans_ptr->get_source_action().get_action_param().size());

    data::xproperty_asset asset_out{0};
    stream >> asset_out.m_token_name;
    stream >> asset_out.m_amount;
#if defined XENABLE_MOCK_ZEC_STAKE
    node_info.m_account_mortgage = 100000000000000;
#else
    node_info.m_account_mortgage += asset_out.m_amount;
#endif

    node_info.m_public_key = xpublic_key_t{signing_key};
    node_info.is_genesis_node = false;
    node_info.m_registration_type = common::to_registration_type(registration_type);
    XCONTRACT_ENSURE(node_info.m_registration_type != common::xregistration_type_t::invalid, "xtop_rec_registration_contract2::registerNode registration_type invalid");

    auto registration_result_store = serialization::xmsgpack_t<xregistration_result_store_t>::deserialize_from_string_prop(*this, XPORPERTY_BEACON_REG_KEY);

    registration_result_store.result_of(network_id).insert({account, node_info});

    serialization::xmsgpack_t<xregistration_result_store_t>::serialize_to_string_prop(*this, XPORPERTY_BEACON_REG_KEY, registration_result_store);
}

void xtop_rec_registration_contract2::unregisterNode() {
    assert(false);
    // todo
}

void xtop_rec_registration_contract2::updateNodeInfo(common::xnetwork_id_t const & network_id,
                                                     const int updateDepositType,
                                                     const uint64_t deposit,
                                                     const std::string & registration_type,
                                                     const std::string & node_sign_key) {
    assert(false);
    // todo
}

void xtop_rec_registration_contract2::updateNodeSignKey(const std::string & node_sign_key) {
    assert(false);
    // todo
}

void xtop_rec_registration_contract2::updateNodeType(const std::string & node_types) {
    assert(false);
    // todo
}

void xtop_rec_registration_contract2::stakeDeposit() {
    assert(false);
    // todo
}

void xtop_rec_registration_contract2::unstakeDeposit(uint64_t unstake_deposit) {
    assert(false);
    // todo
}

NS_END4

#undef XREG_CONTRACT
#undef XCONTRACT_PREFIX
#undef XREC_MODULE

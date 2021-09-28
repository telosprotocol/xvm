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
#include "xbasic/xutility.h"

#include <cinttypes>

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
        zec_node_info.m_vote_amount = 0;

        zec_registration_result_store.insert({node_id, zec_node_info});
    }

    serialization::xmsgpack_t<data::registration::xzec_registration_result_t>::serialize_to_string_prop(*this, XPORPERTY_ZEC_REG_KEY, zec_registration_result_store);

    STRING_CREATE(XPORPERTY_ZEC_CROSS_READING_REC_REG_BLOCK_HEIGHT);
    STRING_CREATE(XPORPERTY_ZEC_CROSS_READING_REC_REG_LOGIC_TIME);
    STRING_CREATE(XPORPERTY_ZEC_CROSS_READING_REC_STEP);
    STRING_CREATE(XPORPERTY_ZEC_CROSS_READING_REC_TIMEOUT_LIMIT);
    STRING_SET(data::XPORPERTY_ZEC_CROSS_READING_REC_REG_BLOCK_HEIGHT, std::to_string(0));
    STRING_SET(data::XPORPERTY_ZEC_CROSS_READING_REC_REG_LOGIC_TIME, std::to_string(0));
    STRING_SET(data::XPORPERTY_ZEC_CROSS_READING_REC_STEP, std::to_string(3));
    STRING_SET(data::XPORPERTY_ZEC_CROSS_READING_REC_TIMEOUT_LIMIT, std::to_string(30));  // todo make in constexpr;

}

void xtop_zec_registration_contract::on_timer(common::xlogic_time_t current_time) {
    XCONTRACT_ENSURE(SOURCE_ADDRESS() == SELF_ADDRESS().value(), "xtop_zec_registration_contract instance is triggled by others");
    XCONTRACT_ENSURE(SELF_ADDRESS().value() == sys_contract_zec_registration_addr, "xtop_zec_registration_contract instance is not triggled by sys_contract_zec_registration_addr");

    xdbg("[xtop_zec_registration_contract] on_timer: %" PRIu64, current_time);

    bool update{false};
    auto const last_read_height = static_cast<std::uint64_t>(std::stoull(STRING_GET(data::XPORPERTY_ZEC_CROSS_READING_REC_REG_BLOCK_HEIGHT)));
    auto const last_read_time = static_cast<std::uint64_t>(std::stoull(STRING_GET(data::XPORPERTY_ZEC_CROSS_READING_REC_REG_LOGIC_TIME)));
    auto const read_step = static_cast<std::uint64_t>(std::stoull(STRING_GET(data::XPORPERTY_ZEC_CROSS_READING_REC_STEP)));
    auto const timeout_limit = static_cast<std::uint64_t>(std::stoull(STRING_GET(data::XPORPERTY_ZEC_CROSS_READING_REC_TIMEOUT_LIMIT)));

    uint64_t latest_height = get_blockchain_height(sys_contract_rec_registration_addr2);
    xdbg("[xtop_zec_registration_contract] get latest_height: %" PRIu64, latest_height);
    XCONTRACT_ENSURE(latest_height >= last_read_height, "xtop_zec_registration_contract::on_timer latest_height < last_read_height");
    if (latest_height == last_read_height) {
        XMETRICS_PACKET_INFO(XREG_CONTRACT "update_status", "next_read_height", last_read_height, "current_time", current_time)
        STRING_SET(data::XPORPERTY_ZEC_CROSS_READING_REC_REG_LOGIC_TIME, std::to_string(current_time));
        return;
    }
    uint64_t next_read_height = last_read_height;
    if (latest_height - last_read_height >= read_step) {
        next_read_height = last_read_height + read_step;
        update = true;
    } else if (current_time - last_read_time > timeout_limit) {
        next_read_height = latest_height;
        update = true;
    }
    xinfo("[xtop_zec_registration_contract] next_read_height: %" PRIu64, next_read_height);
    base::xauto_ptr<xblock_t> block_ptr = get_block_by_height(sys_contract_rec_registration_addr2, next_read_height);
    std::string result;
    if (block_ptr == nullptr) {
        xwarn("[xtop_zec_registration_contract] fail to get the rec_registration data. next_read_block height:%" PRIu64, next_read_height);
        return;
    }
    if (update) {
        XMETRICS_PACKET_INFO(XREG_CONTRACT "update_status", "next_read_height", next_read_height, "current_time", current_time)
        STRING_SET(data::XPORPERTY_ZEC_CROSS_READING_REC_REG_BLOCK_HEIGHT, std::to_string(next_read_height));
        STRING_SET(data::XPORPERTY_ZEC_CROSS_READING_REC_REG_LOGIC_TIME, std::to_string(current_time));
        STRING_SET(data::XPORPERTY_ZEC_CROSS_READING_REC_STEP, std::to_string(read_step + 1));
    } else {
        STRING_SET(data::XPORPERTY_ZEC_CROSS_READING_REC_STEP, std::to_string(read_step > 0 ? (read_step - 1) : (read_step)));
    }

    if (update) {
        do_update();
    }

    return;
}

void xtop_zec_registration_contract::do_update() {
    xdbg("[xtop_zec_registration_contract] do_update");

    data::registration::xzec_registration_result_t zec_registration_result_store;
    serialization::xmsgpack_t<data::registration::xzec_registration_result_t>::deserialize_from_string_prop(*this, XPORPERTY_ZEC_REG_KEY);

    auto const read_height = static_cast<std::uint64_t>(std::stoull(STRING_GET(data::XPORPERTY_ZEC_CROSS_READING_REC_REG_BLOCK_HEIGHT)));
    xinfo("[xtop_zec_registration_contract] read_height: %" PRIu64, read_height);

    std::string result;
    GET_STRING_PROPERTY(data::XPORPERTY_BEACON_REG_KEY, result, read_height, sys_contract_rec_registration_addr2);
    auto const rec_registration_data = serialization::xmsgpack_t<data::registration::xregistration_result_store_t>::deserialize_from_string_prop(result);
    auto const & main_chain_result = rec_registration_data.result_of(network_id());
    for(auto const & _p : main_chain_result){
        auto const & node_id = top::get<const common::xnode_id_t>(_p);
        auto const & rec_reg_node_info = top::get<data::registration::xrec_registration_node_info_t>(_p);
        xdbg("[xtop_zec_registration_contract] get node: %s", node_id.c_str());

        update_zec_reg_node_info(node_id,rec_reg_node_info,zec_registration_result_store);
    }

    serialization::xmsgpack_t<data::registration::xzec_registration_result_t>::serialize_to_string_prop(*this, XPORPERTY_ZEC_REG_KEY, zec_registration_result_store);
}

void xtop_zec_registration_contract::update_zec_reg_node_info(common::xnode_id_t const & node_id,
                                                              data::registration::xrec_registration_node_info_t const & rec_node_info,
                                                              data::registration::xzec_registration_result_t & zec_result) {
    xdbg("[xtop_zec_registration_contract] update_zec_reg_node_info %s", node_id.c_str());
    if (zec_result.empty(node_id)) {
        // new nodes.
        data::registration::xzec_registration_node_info_t zec_node_info;
        zec_node_info.m_rec_registration_node_info = rec_node_info;
        zec_node_info.m_role_type = common::to_mainchain_role_type(rec_node_info.m_registration_type);
        zec_node_info.m_nickname = "";
        zec_result.insert({node_id, zec_node_info});
    } else {
        data::registration::xzec_registration_node_info_t & zec_node_info = zec_result.result_of(node_id);
        if (zec_node_info.m_rec_registration_node_info != rec_node_info) {
            zec_node_info.m_rec_registration_node_info = rec_node_info;
            zec_node_info.m_role_type = common::to_mainchain_role_type(rec_node_info.m_registration_type);
            // zec_node_info.m_nickname = "";
        }
    }
}

NS_END4

#undef XREG_CONTRACT
#undef XCONTRACT_PREFIX
#undef XZEC_MODULE

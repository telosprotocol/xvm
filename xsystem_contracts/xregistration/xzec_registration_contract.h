// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xcommon/xip.h"
#include "xvm/xcontract/xcontract_base.h"
#include "xvm/xcontract/xcontract_exec.h"

#include "xdata/xregistration/xregistration_data_struct.h"

#include <type_traits>

NS_BEG4(top, xvm, system_contracts, zec)

using namespace xvm;
using namespace xvm::xcontract;

class xtop_zec_registration_contract final : public xcontract_base {
    using xbase_t = xcontract_base;

public:
    XDECLARE_DELETED_COPY_DEFAULTED_MOVE_SEMANTICS(xtop_zec_registration_contract);
    XDECLARE_DEFAULTED_OVERRIDE_DESTRUCTOR(xtop_zec_registration_contract);

    explicit xtop_zec_registration_contract(common::xnetwork_id_t const & network_id);

    xcontract_base * clone() override {
        return new xtop_zec_registration_contract(network_id());
    }

    void setup();

    BEGIN_CONTRACT_WITH_PARAM(xtop_zec_registration_contract)
    CONTRACT_FUNCTION_PARAM(xtop_zec_registration_contract, on_timer);
    // CONTRACT_FUNCTION_PARAM(xtop_zec_registration_contract, setNodeName);
    // CONTRACT_FUNCTION_PARAM(xtop_zec_registration_contract, setDividendRatio);
    // CONTRACT_FUNCTION_PARAM(xtop_zec_registration_contract, update_batch_stake);
    // CONTRACT_FUNCTION_PARAM(xtop_zec_registration_contract, slash_unqualified_node);
    END_CONTRACT_WITH_PARAM

private:
    void on_timer(common::xlogic_time_t const current_time);

    void do_update();

    void update_zec_reg_node_info(common::xnode_id_t const & node_id,
                                  data::registration::xrec_registration_node_info_t const & rec_node_info,
                                  data::registration::xzec_registration_result_t & zec_result);
};
using xzec_registration_contract_t = xtop_zec_registration_contract;

NS_END4

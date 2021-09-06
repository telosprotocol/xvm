// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xcommon/xlogic_time.h"
#include "xdata/xparachain/xparachain_result.h"
#include "xvm/xcontract/xcontract_base.h"
#include "xvm/xcontract/xcontract_exec.h"

NS_BEG4(top, xvm, system_contract, rec)

class xtop_rec_parachain_registration_contract final : public xcontract::xcontract_base {
    using base_t = xcontract::xcontract_base;

public:
    XDECLARE_DELETED_COPY_DEFAULTED_MOVE_SEMANTICS(xtop_rec_parachain_registration_contract);
    XDECLARE_DEFAULTED_OVERRIDE_DESTRUCTOR(xtop_rec_parachain_registration_contract);

    explicit xtop_rec_parachain_registration_contract(common::xnetwork_id_t const & network_id);

    xcontract::xcontract_base * clone() override {
        return new xtop_rec_parachain_registration_contract(network_id());
    }

    void setup();

    BEGIN_CONTRACT_WITH_PARAM(xtop_rec_parachain_registration_contract)
    CONTRACT_FUNCTION_PARAM(xtop_rec_parachain_registration_contract, register_parachain);
    CONTRACT_FUNCTION_PARAM(xtop_rec_parachain_registration_contract, on_timer);
    END_CONTRACT_WITH_PARAM

private:
    void register_parachain(std::string chain_name, uint32_t chain_id, uint32_t genesis_node_size);

    void on_timer(common::xlogic_time_t const current_time);
};

using xrec_parachain_registration_contract_t = xtop_rec_parachain_registration_contract;

NS_END4
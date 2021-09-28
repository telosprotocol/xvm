// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xcommon/xlogic_time.h"
#include "xvm/xcontract/xcontract_exec.h"
#include "xvm/xsystem_contracts/xelection/xelect_consensus_group_contract.h"
#include "xdata/xelection/xelection_data_struct.h"
#include "xdata/xstandby/xstandby_data_struct.h"
#include "xdata/xparachain/xparachain_chain_info.h"

NS_BEG4(top, xvm, system_contracts, rec)

class xtop_rec_elect_ec_contract final : public xelect_consensus_group_contract_t {
    using xbase_t = xelect_consensus_group_contract_t;

public:
    XDECLARE_DELETED_COPY_DEFAULTED_MOVE_SEMANTICS(xtop_rec_elect_ec_contract);
    XDECLARE_DEFAULTED_OVERRIDE_DESTRUCTOR(xtop_rec_elect_ec_contract);

    explicit xtop_rec_elect_ec_contract(common::xnetwork_id_t const & network_id);

    xcontract_base * clone() override {
        return new xtop_rec_elect_ec_contract(network_id());
    }

    void setup();

    void on_timer(common::xlogic_time_t const current_time);

    BEGIN_CONTRACT_WITH_PARAM(xtop_rec_elect_ec_contract)
    CONTRACT_FUNCTION_PARAM(xtop_rec_elect_ec_contract, on_timer);
    END_CONTRACT_WITH_PARAM

private:
    bool elect_mainnet_ec(common::xlogic_time_t const current_time,
                          data::standby::xsimple_standby_result_t const & zec_standby_result,
                          data::election::xelection_network_result_t & zec_election_result);

    bool elect_parachain_genesis(common::xlogic_time_t const current,
                                 uint32_t chain_id,
                                 data::standby::xsimple_standby_result_t const & zec_standby_result,
                                 data::election::xelection_network_result_t & zec_election_result);

    bool elect_parachain_ec(common::xlogic_time_t const current,
                            uint32_t chain_id,
                            data::standby::xsimple_standby_result_t const & zec_standby_result,
                            data::election::xelection_network_result_t & zec_election_result);
};
using xrec_elect_ec_contract_t = xtop_rec_elect_ec_contract;

NS_END4

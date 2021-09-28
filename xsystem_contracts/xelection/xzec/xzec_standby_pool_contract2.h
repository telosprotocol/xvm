// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xcommon/xip.h"
#include "xcommon/xlogic_time.h"
#include "xcommon/xrole_type.h"
#include "xdata/xregistration/xregistration_data_struct.h"
#include "xdata/xstandby/xstandby_data_struct.h"
#include "xvm/xcontract/xcontract_base.h"
#include "xvm/xcontract/xcontract_exec.h"

#include <string>

NS_BEG4(top, xvm, system_contracts, zec)

class xtop_zec_standby_pool_contract2 final : public xcontract::xcontract_base {
    using xbase_t = xcontract::xcontract_base;

public:
    XDECLARE_DELETED_COPY_DEFAULTED_MOVE_SEMANTICS(xtop_zec_standby_pool_contract2);
    XDECLARE_DEFAULTED_OVERRIDE_DESTRUCTOR(xtop_zec_standby_pool_contract2);

    explicit xtop_zec_standby_pool_contract2(common::xnetwork_id_t const & network_id);

    xcontract::xcontract_base * clone() override {
        return new xtop_zec_standby_pool_contract2(network_id());
    }

    void setup();

    BEGIN_CONTRACT_WITH_PARAM(xtop_zec_standby_pool_contract2)
    CONTRACT_FUNCTION_PARAM(xtop_zec_standby_pool_contract2, nodeJoinNetwork);
    CONTRACT_FUNCTION_PARAM(xtop_zec_standby_pool_contract2, on_timer);
    END_CONTRACT_WITH_PARAM

private:
    void nodeJoinNetwork(common::xaccount_address_t const & node_id, common::xnetwork_id_t const & joined_network_id, std::string const & program_version);

    bool nodeJoinNetworkImpl(common::xaccount_address_t const & node_id,
                             std::string const & program_version,
                             data::registration::xzec_registration_node_info_t const & zec_registration_node_info,
                             data::standby::xzec_standby_result_t & zec_standby_result);

    void on_timer(common::xlogic_time_t const current_time);

    bool update_standby_result(data::registration::xzec_registration_result_t const & zec_registration_result, data::standby::xzec_standby_result_t & zec_standby_result) const;

    bool update_node_info(data::registration::xzec_registration_node_info_t const & registration_node_info, data::standby::xzec_standby_node_info_t & standby_node_info) const;
};
using xzec_standby_pool_contract2_t = xtop_zec_standby_pool_contract2;

NS_END4

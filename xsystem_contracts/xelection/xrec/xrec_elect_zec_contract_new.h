// Copyright (c) 2017-2021 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xcommon/xip.h"
#include "xcontract_common/xproperties/xproperty_bytes.h"
#include "xdata/xelection/xelection_result_property.h"
#include "xdata/xelection/xelection_result_store.h"
#include "xsystem_contract_runtime/xsystem_contract_runtime_helper.h"
#include "xvm/xsystem_contracts/xelection/xelect_consensus_group_contract_new.h"

NS_BEG2(top, system_contracts)

class xtop_rec_elect_zec_contract_new final : public xelect_consensus_group_contract_new_t {
    contract_common::properties::xbytes_property_t m_result{data::election::get_property_by_group_id(common::xcommittee_group_id), this};

public:
    xtop_rec_elect_zec_contract_new() = default;
    xtop_rec_elect_zec_contract_new(xtop_rec_elect_zec_contract_new const &) = delete;
    xtop_rec_elect_zec_contract_new & operator=(xtop_rec_elect_zec_contract_new const &) = delete;
    xtop_rec_elect_zec_contract_new(xtop_rec_elect_zec_contract_new &&) = default;
    xtop_rec_elect_zec_contract_new & operator=(xtop_rec_elect_zec_contract_new &&) = default;
    ~xtop_rec_elect_zec_contract_new() override = default;

    BEGIN_CONTRACT_API()
        DECLARE_API(xtop_rec_elect_zec_contract_new::setup);
        DECLARE_SELF_ONLY_API(xtop_rec_elect_zec_contract_new::on_timer);
    END_CONTRACT_API

private:
    void setup();
    void on_timer(common::xlogic_time_t const current_time);

#ifdef STATIC_CONSENSUS
    void elect_config_nodes(common::xlogic_time_t const current_time);
#endif
};
using xrec_elect_zec_contract_new_t = xtop_rec_elect_zec_contract_new;

NS_END2

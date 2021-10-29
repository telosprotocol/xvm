// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xcontract_common/xproperties/xproperty_string.h"
#include "xsystem_contract_runtime/xsystem_contract_runtime_helper.h"
#include "xsystem_contracts/xbasic_system_contract.h"

NS_BEG2(top, system_contracts)

class xtop_zec_standby_pool_contract_new final : public xbasic_system_contract_t {
    contract_common::properties::xstring_property_t m_rec_standby_pool_data_last_read_height{data::XPROPERTY_LAST_READ_REC_STANDBY_POOL_CONTRACT_BLOCK_HEIGHT, this};
    contract_common::properties::xstring_property_t m_rec_standby_pool_data_last_read_time{data::XPROPERTY_LAST_READ_REC_STANDBY_POOL_CONTRACT_LOGIC_TIME, this};

public:
    xtop_zec_standby_pool_contract_new() = default;
    xtop_zec_standby_pool_contract_new(xtop_zec_standby_pool_contract_new const &) = delete;
    xtop_zec_standby_pool_contract_new & operator=(xtop_zec_standby_pool_contract_new const &) = delete;
    xtop_zec_standby_pool_contract_new(xtop_zec_standby_pool_contract_new &&) = default;
    xtop_zec_standby_pool_contract_new & operator=(xtop_zec_standby_pool_contract_new &&) = default;
    ~xtop_zec_standby_pool_contract_new() override = default;

    BEGIN_CONTRACT_API()
        DECLARE_API(xtop_zec_standby_pool_contract_new::setup);
        DECLARE_SELF_ONLY_API(xtop_zec_standby_pool_contract_new::on_timer);
    END_CONTRACT_API

private:
    void setup();
    void on_timer(common::xlogic_time_t const current_time);
};
using xzec_standby_pool_contract_new_t = xtop_zec_standby_pool_contract_new;

NS_END2

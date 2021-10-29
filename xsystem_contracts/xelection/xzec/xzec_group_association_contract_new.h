// Copyright (c) 2017-2021 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xcontract_common/xproperties/xproperty_bytes.h"
#include "xdata/xproperty.h"
#include "xsystem_contract_runtime/xsystem_contract_runtime_helper.h"
#include "xsystem_contracts/xbasic_system_contract.h"

NS_BEG2(top, system_contracts)

class xtop_group_association_contract_new final : public xbasic_system_contract_t {
    contract_common::properties::xbytes_property_t m_result{data::XPROPERTY_CONTRACT_GROUP_ASSOC_KEY, this};

public:
    xtop_group_association_contract_new() = default;
    xtop_group_association_contract_new(xtop_group_association_contract_new const &) = delete;
    xtop_group_association_contract_new & operator=(xtop_group_association_contract_new const &) = delete;
    xtop_group_association_contract_new(xtop_group_association_contract_new &&) = default;
    xtop_group_association_contract_new & operator=(xtop_group_association_contract_new &&) = default;
    ~xtop_group_association_contract_new() override = default;

    BEGIN_CONTRACT_API()
        DECLARE_API(xtop_group_association_contract_new::setup);
    END_CONTRACT_API

private:
    void setup();
};
using xgroup_association_contract_new_t = xtop_group_association_contract_new;

NS_END2

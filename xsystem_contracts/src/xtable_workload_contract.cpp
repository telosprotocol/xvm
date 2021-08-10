// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xworkload/xtable_workload_contract.h"

NS_BEG3(top, xvm, system_contracts)

xtable_workload_contract::xtable_workload_contract(common::xnetwork_id_t const & network_id) : xbase_t{network_id} {
}

void xtable_workload_contract::setup() {
    STRING_CREATE(xstake::XPORPERTY_CONTRACT_TABLEBLOCK_HEIGHT_KEY);
}

void xtable_workload_contract::on_timer(const common::xlogic_time_t timestamp) {
    xdbg("[xtable_workload_contract::on_timer] call from %s", SOURCE_ADDRESS().c_str());
}

NS_END3
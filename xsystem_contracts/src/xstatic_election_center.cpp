// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xelection/xstatic_election_center.h"

NS_BEG3(top, xvm, system_contracts)

void set_signal_true(int signal) {
    std::cout << "[static_consensus][xstatic_election_center]recv signal,allow election" << std::endl;
    xinfo("[static_consensus][xstatic_election_center]recv signal,set allow election true");
    xstatic_election_center::instance().allow_elect();
}

NS_END3
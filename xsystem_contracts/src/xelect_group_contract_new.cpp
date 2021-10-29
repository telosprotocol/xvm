// Copyright (c) 2017-2021 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xelection/xelect_group_contract_new.h"

#include <cassert>

NS_BEG2(top, system_contracts)

bool xtop_elect_group_contract_new::elect_group(common::xzone_id_t const &,
                                                common::xcluster_id_t const &,
                                                common::xgroup_id_t const &,
                                                common::xlogic_time_t const,
                                                common::xlogic_time_t const,
                                                std::uint64_t const,
                                                xrange_t<config::xgroup_size_t> const &,
                                                data::election::xstandby_network_result_t const &,
                                                data::election::xelection_network_result_t &) {
    assert(false);
    return false;
}

bool xtop_elect_group_contract_new::elect_group(common::xzone_id_t const &,
                                                common::xcluster_id_t const &,
                                                common::xgroup_id_t const &,
                                                common::xlogic_time_t const,
                                                common::xlogic_time_t const,
                                                xrange_t<config::xgroup_size_t> const &,
                                                data::election::xstandby_network_result_t const &,
                                                data::election::xelection_network_result_t &) {
    assert(false);
    return false;
}

NS_END2

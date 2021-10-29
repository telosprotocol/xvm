// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xbasic/xrange.hpp"
#include "xcommon/xip.h"
#include "xcommon/xlogic_time.h"
#include "xdata/xelection/xelection_network_result.h"
#include "xdata/xelection/xstandby_network_result.h"
#include "xsystem_contracts/xbasic_system_contract.h"

NS_BEG2(top, system_contracts)

class xtop_elect_group_contract_new : public xbasic_system_contract_t {
public:
    xtop_elect_group_contract_new(xtop_elect_group_contract_new const &) = delete;
    xtop_elect_group_contract_new & operator=(xtop_elect_group_contract_new const &) = delete;
    xtop_elect_group_contract_new(xtop_elect_group_contract_new &&) = default;
    xtop_elect_group_contract_new & operator=(xtop_elect_group_contract_new &&) = default;
    ~xtop_elect_group_contract_new() override = default;

protected:
    xtop_elect_group_contract_new() = default;

    /**
     * @brief elect consensus group
     * 
     * @param zid Zone id
     * @param cid Cluster id
     * @param gid Group id
     * @param election_timestamp Timestamp that triggers the election
     * @param start_time The time that this election result starts to work
     * @param random_seed Random seed for FTS algorithm internally used by election process
     * @param group_size_range Maximum and minimum values for the group
     * @param standby_network_result Standby pool
     * @param election_network_result Election result
     * @return true election successful
     * @return false election failed
     */
    virtual bool elect_group(common::xzone_id_t const & zid,
                             common::xcluster_id_t const & cid,
                             common::xgroup_id_t const & gid,
                             common::xlogic_time_t const election_timestamp,
                             common::xlogic_time_t const start_time,
                             std::uint64_t const random_seed,
                             xrange_t<config::xgroup_size_t> const & group_size_range,
                             data::election::xstandby_network_result_t const & standby_network_result,
                             data::election::xelection_network_result_t & election_network_result);

    /**
     * @brief elect non-consensus group
     * 
     * @param zid Zone id
     * @param cid Cluster id
     * @param gid Group id
     * @param election_timestamp Timestamp that triggers the election
     * @param start_time The time that this election result starts to work
     * @param group_size_range Maximum and minimum values for the group
     * @param standby_network_result Standby pool
     * @param election_network_result Election result
     * @return true election successful
     * @return false election failed
     */
    virtual bool elect_group(common::xzone_id_t const & zid,
                             common::xcluster_id_t const & cid,
                             common::xgroup_id_t const & gid,
                             common::xlogic_time_t const election_timestamp,
                             common::xlogic_time_t const start_time,
                             xrange_t<config::xgroup_size_t> const & group_size_range,
                             data::election::xstandby_network_result_t const & standby_network_result,
                             data::election::xelection_network_result_t & election_network_result);
};
using xelect_group_contract_new_t = xtop_elect_group_contract_new;

NS_END2

// Copyright (c) 2017-2021 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xdata/xelection/xelection_association_result_store.h"

#include "xcommon/xip.h"
#include "xcontract_common/xproperties/xproperty_bytes.h"
#include "xdata/xelection/xelection_result_property.h"
#include "xdata/xelection/xelection_result_store.h"
#include "xsystem_contract_runtime/xsystem_contract_runtime_helper.h"
#include "xvm/xsystem_contracts/xelection/xelect_consensus_group_contract_new.h"

NS_BEG2(top, system_contracts)

class xtop_zec_elect_consensus_group_contract_new final : public xelect_consensus_group_contract_new_t {
    common::xelection_round_t m_zec_round_version{0};
    mutable bool m_update_registration_contract_read_status{ false };

    contract_common::properties::xbytes_property_t m_consensus1_result{
        data::election::get_property_by_group_id(common::xgroup_id_t{static_cast<top::common::xgroup_id_t::value_type>(common::xauditor_group_id_value_begin)}),
        this};
    contract_common::properties::xbytes_property_t m_consensus2_result{
        data::election::get_property_by_group_id(common::xgroup_id_t{static_cast<top::common::xgroup_id_t::value_type>(common::xauditor_group_id_value_begin + 1)}),
        this};

    contract_common::properties::xbytes_property_t m_election_executed{data::XPROPERTY_CONTRACT_ELECTION_EXECUTED_KEY, this};

public:
    xtop_zec_elect_consensus_group_contract_new() = default;
    xtop_zec_elect_consensus_group_contract_new(xtop_zec_elect_consensus_group_contract_new const &) = delete;
    xtop_zec_elect_consensus_group_contract_new & operator=(xtop_zec_elect_consensus_group_contract_new const &) = delete;
    xtop_zec_elect_consensus_group_contract_new(xtop_zec_elect_consensus_group_contract_new &&) = default;
    xtop_zec_elect_consensus_group_contract_new & operator=(xtop_zec_elect_consensus_group_contract_new &&) = default;
    ~xtop_zec_elect_consensus_group_contract_new() override = default;

    BEGIN_CONTRACT_API()
        DECLARE_API(xtop_zec_elect_consensus_group_contract_new::setup);
        DECLARE_SELF_ONLY_API(xtop_zec_elect_consensus_group_contract_new::on_timer);
    END_CONTRACT_API

private:
    void setup();
    void on_timer(common::xlogic_time_t const current_time);

#ifdef STATIC_CONSENSUS
    void swap_election_result(common::xlogic_time_t const current_time);
    void elect_config_nodes(common::xlogic_time_t const current_time);
#endif

    void elect(common::xzone_id_t const zone_id, common::xcluster_id_t const cluster_id, std::uint64_t const random_seed, common::xlogic_time_t const election_timestamp);

    bool elect_auditor_validator(common::xzone_id_t const & zone_id,
                                 common::xcluster_id_t const & cluster_id,
                                 common::xgroup_id_t const & auditor_group_id,
                                 std::uint64_t const random_seed,
                                 common::xlogic_time_t const election_timestamp,
                                 common::xlogic_time_t const start_time,
                                 data::election::xelection_association_result_store_t const & association_result_store,
                                 data::election::xstandby_network_result_t const & standby_network_result,
                                 std::unordered_map<common::xgroup_id_t, data::election::xelection_result_store_t> & all_cluster_election_result_store);

    bool elect_auditor(common::xzone_id_t const & zid,
                       common::xcluster_id_t const & cid,
                       common::xgroup_id_t const & gid,
                       common::xlogic_time_t const election_timestamp,
                       common::xlogic_time_t const start_time,
                       std::uint64_t const random_seed,
                       data::election::xstandby_network_result_t const & standby_network_result,
                       data::election::xelection_network_result_t & election_network_result);

    bool elect_validator(common::xzone_id_t const & zid,
                         common::xcluster_id_t const & cid,
                         common::xgroup_id_t const & auditor_gid,
                         common::xgroup_id_t const & validator_gid,
                         common::xlogic_time_t const election_timestamp,
                         common::xlogic_time_t const start_time,
                         std::uint64_t const random_seed,
                         data::election::xstandby_network_result_t const & standby_network_result,
                         data::election::xelection_network_result_t & election_network_result);

    bool genesis_elected() const;
};
using xzec_elect_consensus_group_contract_new_t = xtop_zec_elect_consensus_group_contract_new;

NS_END2

// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xdata/xtableblock.h"
#include "xdata/xworkload_info.h"
#include "xstake/xstake_algorithm.h"
#include "xvm/xcontract/xcontract_base.h"
#include "xvm/xcontract/xcontract_exec.h"
#include "xvm/xcontract_helper.h"

NS_BEG3(top, xvm, system_contracts)

//using namespace xvm;
//using namespace xvm::xcontract;

class xzec_workload_contract_v2 : public xcontract::xcontract_base
{
    using xbase_t = xcontract::xcontract_base;

public:
    XDECLARE_DELETED_COPY_DEFAULTED_MOVE_SEMANTICS(xzec_workload_contract_v2);
    XDECLARE_DEFAULTED_OVERRIDE_DESTRUCTOR(xzec_workload_contract_v2);

    explicit xzec_workload_contract_v2(common::xnetwork_id_t const &network_id);

    xcontract_base *clone() override { return new xzec_workload_contract_v2(network_id()); }

    /**
     * @brief setup the contract
     *
     */
    void setup();

    /**
     * @brief call zec reward contract to calculate reward
     *
     * @param timestamp the time to call
     */
    void on_timer(common::xlogic_time_t const timestamp);

    BEGIN_CONTRACT_WITH_PARAM(xzec_workload_contract_v2)
    CONTRACT_FUNCTION_PARAM(xzec_workload_contract_v2, on_timer);
    END_CONTRACT_WITH_PARAM

private:
    /**
     * @brief update tgas
     *
     * @param table_pledge_balance_change_tgas table pledge balance change tgas
     */
    void update_tgas(int64_t table_pledge_balance_change_tgas);

    /**
     * @brief get_fullblock
     */
    std::vector<xobject_ptr_t<data::xblock_t>> get_fullblock(const uint32_t table_id, const uint64_t timestamp);

    /**
     * @brief add_workload_with_fullblock
     */
    void accumulate_workload(xstatistics_data_t const &stat_data,
                             std::map<common::xgroup_address_t, xauditor_workload_info_t> &bookload_auditor_group_workload_info,
                             std::map<common::xgroup_address_t, xvalidator_workload_info_t> &bookload_validator_group_workload_info);

    /**
     * @brief accumulate_validator_workload
     */
    void accumulate_validator_workload(common::xgroup_address_t const &group_addr,
                                       std::string const &account_str,
                                       const uint32_t slotid,
                                       xgroup_related_statistics_data_t const &group_account_data,
                                       const uint32_t workload_per_tableblock,
                                       const uint32_t workload_per_tx,
                                       std::map<common::xgroup_address_t, xvalidator_workload_info_t> &validator_group_workload);

    /**
     * @brief accumulate_auditor_workload
     */
    void accumulate_auditor_workload(common::xgroup_address_t const &group_addr,
                                     std::string const &account_str,
                                     const uint32_t slotid,
                                     xgroup_related_statistics_data_t const &group_account_data,
                                     const uint32_t workload_per_tableblock,
                                     const uint32_t workload_per_tx,
                                     std::map<common::xgroup_address_t, xauditor_workload_info_t> &auditor_group_workload);

    /**
     * @brief add_workload_with_fullblock
     */
    void accumulate_workload_with_fullblock(common::xlogic_time_t const timestamp,
                                            std::map<common::xgroup_address_t, xauditor_workload_info_t> & auditor_group_workload,
                                            std::map<common::xgroup_address_t, xvalidator_workload_info_t> & validator_group_workload);

    /**
     * @brief get_table_height
     */
    uint64_t get_table_height(const uint32_t table_id) const;

    /**
     * @brief update_table_height
     */
    void update_table_height(const uint32_t table_id, uint64_t cur_read_height);
};

NS_END3

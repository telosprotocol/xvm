// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xcommon/xlogic_time.h"
#include "xdata/xfull_tableblock.h"
#include "xdata/xslash.h"
#include "xstake/xstake_algorithm.h"
#include "xvledger/xvcnode.h"
#include "xvm/xcontract/xcontract_base.h"
#include "xvm/xcontract/xcontract_exec.h"

NS_BEG3(top, xvm, xcontract)

/**
 * @brief the table slash contract
 *
 */
class xtable_statistic_info_collection_contract final : public xcontract_base {
    using xbase_t = xcontract_base;
public:
    XDECLARE_DELETED_COPY_DEFAULTED_MOVE_SEMANTICS(xtable_statistic_info_collection_contract);
    XDECLARE_DEFAULTED_OVERRIDE_DESTRUCTOR(xtable_statistic_info_collection_contract);

    explicit
    xtable_statistic_info_collection_contract(common::xnetwork_id_t const & network_id);

    xcontract_base*
    clone() override {
        return new xtable_statistic_info_collection_contract(network_id());
    }

    void
    setup();

    /**
     * @brief  collect the statistic info on tables
     *
     * @param statistic_info  the info to collect
     * @param block_height the fullblock height
     */
    void
    on_collect_statistic_info(std::string const& statistic_info, uint64_t block_height, int64_t tgas);

    /**
     * @brief report the summarized statistic info
     * @param timestamp  the clock timer
     *
     */
    void
    report_summarized_statistic_info(common::xlogic_time_t timestamp);

    BEGIN_CONTRACT_WITH_PARAM(xtable_statistic_info_collection_contract)
        CONTRACT_FUNCTION_PARAM(xtable_statistic_info_collection_contract, on_collect_statistic_info);
        CONTRACT_FUNCTION_PARAM(xtable_statistic_info_collection_contract, report_summarized_statistic_info);
    END_CONTRACT_WITH_PARAM

private:
    /**
     * @brief accumulate  node info
     *
     * @param  node_info  the node info to accumulate
     * @param  summarize_info  in&out  the accumulated node info
     *
     */
    void  accumulate_node_info(xunqualified_node_info_t const&  node_info, xunqualified_node_info_t& summarize_info);

    /**
     * @brief process statistic data to get nodeinfo
     *
     * @param block_statistic_data  the statistic data of a fulltable block
     * @return xunqualified_node_info_t  the node info from statistic data
     */
    xunqualified_node_info_t process_statistic_data(top::data::xstatistics_data_t const& block_statistic_data, base::xvnodesrv_t * node_service);

    /**
     * @brief process workload statistic data
     *
     * @param  xstatistics_data_t  statistic data
     *
     */
    void process_workload_statistic_data(xstatistics_data_t const & statistic_data, const int64_t tgas);

    /**
     * @brief get_workload
     *
     * @param  xstatistics_data_t  statistic data
     *
     */
    std::map<common::xgroup_address_t, xstake::xgroup_workload_t> get_workload(xstatistics_data_t const & statistic_data);

    /**
     * @brief update_workload
     *
     * @param  group_workload  group workload
     *
     */
    void update_workload(std::map<common::xgroup_address_t, xstake::xgroup_workload_t> const & group_workload);

    /**
     * @brief update_tgas
     *
     * @param  table_pledge_balance_change_tgas  table_pledge_balance_change_tgas
     *
     */
    void update_tgas(const int64_t table_pledge_balance_change_tgas);

    /**
     * @brief upload_workload
     */
    void upload_workload();
};

NS_END3

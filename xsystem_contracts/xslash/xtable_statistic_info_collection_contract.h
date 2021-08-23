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
class xtable_statistic_info_collection_contract : public xcontract_base {
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
     * @brief
     *
     * @param statistic_data
     * @param node_service
     * @param summarize_info_str
     * @param summarize_fulltableblock_num_str
     * @param summarize_info  in&out
     * @param summarize_fulltableblock_num in&out
     * @return true
     * @return false
     */
    bool collect_slash_statistic_info(xstatistics_data_t const& statistic_data,  base::xvnodesrv_t * node_service, std::string const& summarize_info_str, std::string const& summarize_fulltableblock_num_str,
                                        xunqualified_node_info_t& summarize_info, uint32_t& summarize_fulltableblock_num);

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
     * @param auditor_n
     * @return xunqualified_node_info_t  the node info from statistic data
     */
    xunqualified_node_info_t process_statistic_data(top::data::xstatistics_data_t const& block_statistic_data, base::xvnodesrv_t * node_service);

    /**
     * @brief process workload statistic data internal
     *
     * @param  node_service node_service
     * @param  xstatistics_data_t  statistic data
     * @param  tgas tgas
     * @param  worklaod_str worklaod_str
     * @param  tgas_str tgas_str
     * @param  worklaod_str_new worklaod_str_new
     * @param  tgas_str_new tgas_str_new
     *
     */
    void process_workload_data_internal(const base::xvnodesrv_t * node_service,
                                        const xstatistics_data_t & statistic_data,
                                        const int64_t tgas,
                                        const std::map<std::string, std::string> & workload_str,
                                        const std::string & tgas_str,
                                        std::map<std::string, std::string> & workload_str_new,
                                        std::string & tgas_str_new);


    /**
     * @brief process workload statistic data
     *
     * @param  node_service node_service
     * @param  xstatistics_data_t  statistic data
     * @param  tgas tgas
     *
     */
    void process_workload_data(const base::xvnodesrv_t * node_service, const xstatistics_data_t & statistic_data, const int64_t tgas);

    /**
     * @brief get_workload
     *
     * @param  node_service node_service
     * @param  xstatistics_data_t  statistic data
     * @param  worklaod_str worklaod_str
     * @param  worklaod_str_new worklaod_str_new
     *
     */
    void get_workload_from_data(const base::xvnodesrv_t * node_service,
                                const xstatistics_data_t & statistic_data,
                                const std::map<std::string, std::string> & workload_str,
                                std::map<std::string, std::string> & workload_str_new);

    /**
     * @brief get_workload
     */
    xstake::xgroup_workload_t get_workload(common::xgroup_address_t const & group_address);

    /**
     * @brief set_workload
     */
    void set_workload(common::xgroup_address_t const & group_address, xstake::xgroup_workload_t const & group_workload);

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
     * @brief upload_workload_internal
     */
    void upload_workload_internal(std::string & call_contract_str);
    
    /**
     * @brief upload_workload
     */
    void upload_workload();
};

NS_END3

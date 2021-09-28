// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xcommon/xlogic_time.h"
#include "xcontract_common/xproperties/xproperty_map.h"
#include "xcontract_common/xproperties/xproperty_string.h"
#include "xdata/xfull_tableblock.h"
#include "xdata/xfulltableblock_account_data.h"
#include "xdata/xslash.h"
#include "xstake/xstake_algorithm.h"
#include "xsystem_contracts/xbasic_system_contract.h"
#include "xsystem_contract_runtime/xsystem_contract_runtime_helper.h"
#include "xvledger/xvcnode.h"

NS_BEG2(top, system_contracts)

/**
 * @brief the table slash contract
 *
 */
class xtable_statistic_info_collection_contract_new : public xbasic_system_contract_t {
    using xbase_t = xbasic_system_contract_t;
public:

    void
    setup();

    /**
     * @brief  collect the statistic info on tables
     *
     * @param statistic_info  the info to collect
     * @param block_height the fullblock height
     */
    void
    on_collect_statistic_info(xstatistics_data_t const& statistic_data,  xfulltableblock_statistic_accounts const& statistic_accounts, uint64_t block_height, int64_t tgas);

    /**
     * @brief report the summarized statistic info
     * @param timestamp  the clock timer
     *
     */
    void
    report_summarized_statistic_info(common::xlogic_time_t timestamp);

    BEGIN_CONTRACT_API()
        DECLARE_API(xtable_statistic_info_collection_contract_new::setup);
        DECLARE_API(xtable_statistic_info_collection_contract_new::on_collect_statistic_info);
        // DECLARE_API(xtable_statistic_info_collection_contract_new::report_summarized_statistic_info);
    END_CONTRACT_API

private:
    /**
     * @brief collection slash statistic info
     *
     * @param statistic_data
     * @param node_service
     * @param summarize_info_str
     * @param summarize_fulltableblock_num_str
     * @param summarize_info  in&out
     * @param summarize_fulltableblock_num in&out
     */
    void collect_slash_statistic_info(xstatistics_data_t const& statistic_data,  xfulltableblock_statistic_accounts const& statistic_accounts, std::string const& summarize_info_str, std::string const& summarize_fulltableblock_num_str,
                                        xunqualified_node_info_t& summarize_info, uint32_t& summarize_fulltableblock_num);


    /**
     * @brief update slash statistic info
     *
     * @param summarize_info
     * @param summarize_fulltableblock_num
     * @param block_height
     */
    void update_slash_statistic_info( xunqualified_node_info_t const& summarize_info, uint32_t summarize_fulltableblock_num, uint64_t block_height);


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
    xunqualified_node_info_t process_statistic_data(top::data::xstatistics_data_t const& block_statistic_data, xfulltableblock_statistic_accounts const& statistic_accounts);

    /**
     * @brief process workload statistic data
     *
     * @param  xstatistics_data_t  statistic data
     * @param  tgas tgas
     *
     */
    void process_workload_statistic_data(xstatistics_data_t const & statistic_data, xfulltableblock_statistic_accounts const& statistic_accounts, const int64_t tgas);

    /**
     * @brief get_workload
     *
     * @param  xstatistics_data_t  statistic data
     *
     */
    std::map<common::xgroup_address_t, xstake::xgroup_workload_t> get_workload_from_data(xstatistics_data_t const & statistic_data, xfulltableblock_statistic_accounts const& statistic_accounts);

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
     * @brief upload_workload
     */
    void upload_workload();

private:


    contract_common::properties::xmap_property_t<std::string, std::string> m_workload_prop{xstake::XPORPERTY_CONTRACT_WORKLOAD_KEY, this};
    contract_common::properties::xmap_property_t<std::string, std::string> m_slash_prop{xstake::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY, this};
    contract_common::properties::xmap_property_t<std::string, std::string> m_extend_func_prop{xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, this};

    contract_common::properties::xstring_property_t m_tgas_prop{xstake::XPORPERTY_CONTRACT_TGAS_KEY, this};
};

NS_END2

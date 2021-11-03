// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xcommon/xlogic_time.h"
#include "xdata/xslash.h"
#include "xdata/xblock_statistics_data.h"
#include "xstake/xstake_algorithm.h"
#include "xsystem_contracts/xbasic_system_contract.h"
#include "xsystem_contract_runtime/xsystem_contract_runtime_helper.h"

NS_BEG2(top, system_contracts)

/**
 * @brief the zec slash contract
 *
 */
class xzec_slash_info_contract_new : public xbasic_system_contract_t {
    using xbase_t = xbasic_system_contract_t;
public:
    void
    setup();

    /**
     * @brief summarize the slash info from table slash contract
     *
     * @param slash_info  the table slash info
     */
    void
    summarize_slash_info(std::string const& slash_info);


    /**
     * @brief do slash according the summarized slash info
     *
     * @param timestamp  the logic time to do the slash
     */
    void
    do_unqualified_node_slash(common::xlogic_time_t const timestamp);

    BEGIN_CONTRACT_API()
        DECLARE_API(xzec_slash_info_contract_new::setup);
        DECLARE_API(xzec_slash_info_contract_new::summarize_slash_info);
        DECLARE_SELF_ONLY_API(xzec_slash_info_contract_new::do_unqualified_node_slash);
    END_CONTRACT_API

private:
    /**
     * @brief internal function to process summarize info
     *
     * @return true
     * @return false
     */

    bool summarize_slash_info_internal(std::string const& slash_info, std::string const& summarize_info_str, std::string const& summarize_tableblock_count_str, uint64_t const summarized_height,
                                      xunqualified_node_info_t& summarize_info,  uint32_t&  summarize_tableblock_count, std::uint64_t& cur_statistic_height);


    bool do_unqualified_node_slash_internal(std::string const& last_slash_time_str, uint32_t summarize_tableblock_count, uint32_t punish_interval_table_block_param, uint32_t punish_interval_time_block_param , common::xlogic_time_t const timestamp,
                                            xunqualified_node_info_t const & summarize_info, uint32_t slash_vote, uint32_t slash_persent, uint32_t award_vote, uint32_t award_persent, std::vector<xaction_node_info_t>& node_to_action);

    /**
     * @brief print the summarize info
     *
     * @param summarize_slash_info   the current summarized slash info to print
     */
    void print_summarize_info(data::xunqualified_node_info_t const & summarize_slash_info);

    /**
     * @brief print stored table height info
     *
     */
    void print_table_height_info();

    /**
     * @brief get current stored property info for slash, judge some pre-condition
     *
     * @param summarize_info   in&out  the slash summarize_info property
     * @param tableblock_count  in&out  the tableblock count property
     *
     */
    void pre_condition_process(xunqualified_node_info_t& summarize_info, uint32_t& tableblock_count);

    /**
     * @brief filter out the slash node according the summarized slash info
     *
     * @param summarize_info   the summarized slash info
     * @return std::vector<data::xaction_node_info_t>  the node to slash or reward
     */
    std::vector<data::xaction_node_info_t>
    filter_nodes(data::xunqualified_node_info_t const & summarize_info, uint32_t slash_vote, uint32_t slash_persent, uint32_t award_vote, uint32_t award_persent);

    /**
     * @brief filter helper to filter out the slash node
     *
     * @param node_map  the summarized node info
     * @param slash_vote the vote threshhold to slash
     * @param slash_percent the persent of node to slash
     * @param award_vote the vote threshhold to award
     * @param award_percent the persent of node to award
     * @return std::vector<data::xaction_node_info_t>  the node to slash or reward
     */
    std::vector<data::xaction_node_info_t>
    filter_helper(data::xunqualified_node_info_t const & node_map, uint32_t slash_vote, uint32_t slash_persent, uint32_t award_vote, uint32_t award_persent);

    // /**
    //  * @brief get the latest tablefullblock from last read height
    //  *
    //  * @param owner the owner addr of the full tableblock
    //  * @param time_interval  the interval to judge the latest block to processes
    //  * @param last_read_height the height of full tableblock last time read
    //  * @
    //  */
    // std::vector<base::xauto_ptr<xblock_t>> get_next_fulltableblock(common::xaccount_address_t const& owner, uint64_t time_interval, uint64_t last_read_height) const;


    // /**
    //  * @brief process statistic data to get nodeinfo
    //  *
    //  * @param block_statistic_data  the statistic data of a fulltable block
    //  * @return xunqualified_node_info_t  the node info from statistic data
    //  */
    // xunqualified_node_info_t process_statistic_data(top::data::xstatistics_data_t const& block_statistic_data, base::xvnodesrv_t * node_service);


    /**
     * @brief accumulate  node info of all tables
     *
     * @param  node_info  the node info to accumulate
     * @param  summarize_info  in&out  the accumulated node info
     *
     */
    void  accumulate_node_info(xunqualified_node_info_t const&  node_info, xunqualified_node_info_t& summarize_info);

    /**
     * @brief check if statisfy the slash condition
     * @param summarize_tableblock_count  current summarized table block
     * @param timestamp  current slash timestamp
     *
     * @return bool  true means statisfy the slash condition
     */
    bool slash_condition_check(std::string const& last_slash_time_str, uint32_t summarize_tableblock_count, uint32_t punish_interval_table_block_param,
                                uint32_t punish_interval_time_block_param , common::xlogic_time_t const timestamp);

    /**
     * @brief get fulltable height of table
     *
     * @param table_id
     * @return uint64_t
     */
    uint64_t read_fulltable_height_of_table(uint32_t table_id);

    /**
     * @brief process reset data
     *
     * @param db_kv_131 the reset property data
     */
    void process_reset_data(std::vector<std::pair<std::string, std::string>> const& db_kv_131);

private:
    contract_common::properties::xmap_property_t<std::string, std::string> m_unqualified_node_prop{xstake::XPORPERTY_CONTRACT_UNQUALIFIED_NODE_KEY, this};
    contract_common::properties::xmap_property_t<std::string, std::string> m_tableblock_num_prop{xstake::XPROPERTY_CONTRACT_TABLEBLOCK_NUM_KEY, this};
    contract_common::properties::xmap_property_t<std::string, std::string> m_extend_func_prop{xstake::XPROPERTY_CONTRACT_EXTENDED_FUNCTION_KEY, this};

};

NS_END2

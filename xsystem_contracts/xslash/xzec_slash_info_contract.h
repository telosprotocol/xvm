// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xcommon/xlogic_time.h"
#include "xdata/xslash.h"
#include "xvm/xcontract/xcontract_base.h"
#include "xvm/xcontract/xcontract_exec.h"

NS_BEG3(top, xvm, xcontract)

/**
 * @brief the zec slash contract
 *
 */
class xzec_slash_info_contract final : public xcontract_base {
    using xbase_t = xcontract_base;
public:
    XDECLARE_DELETED_COPY_DEFAULTED_MOVE_SEMANTICS(xzec_slash_info_contract);
    XDECLARE_DEFAULTED_OVERRIDE_DESTRUCTOR(xzec_slash_info_contract);

    explicit
    xzec_slash_info_contract(common::xnetwork_id_t const & network_id);

    xcontract_base*
    clone() override {
        return new xzec_slash_info_contract(network_id());
    }

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

    BEGIN_CONTRACT_WITH_PARAM(xzec_slash_info_contract)
        CONTRACT_FUNCTION_PARAM(xzec_slash_info_contract, summarize_slash_info);
        CONTRACT_FUNCTION_PARAM(xzec_slash_info_contract, do_unqualified_node_slash);
    END_CONTRACT_WITH_PARAM

private:
    /**
     * @brief print the summarize info
     *
     * @param summarize_slash_info   the current summarized slash info to print
     */
    void print_summarize_info(data::xunqualified_node_info_t const & summarize_slash_info);

    /**
     * @brief filter out the slash node according the summarized slash info
     *
     * @param summarize_info   the summarized slash info
     * @return std::vector<data::xaction_node_info_t>  the node to slash or reward
     */
    std::vector<data::xaction_node_info_t>
    filter_nodes(data::xunqualified_node_info_t const & summarize_info);

    /**
     * @brief filter helper to filter out the slash node
     *
     * @param node_map  the summarized node info
     * @return std::vector<data::xaction_node_info_t>  the node to slash or reward
     */
    std::vector<data::xaction_node_info_t>
    filter_helper(data::xunqualified_node_info_t const & node_map);

    /**
     * @brief get the latest tablefullblock from last read height
     *
     * @param owner the owner addr of the full tableblock
     * @param block_num in&out the block num processed
     * @param last_read_height in&out the height of full tableblock last time read
     */
    std::vector<base::xauto_ptr<xblock_t>> get_next_fulltableblock(common::xaccount_address_t const& owner, uint32_t& block_num, uint64_t& last_read_height) const;


    /**
     * @brief process full tableblocks to get statistic data
     *
     * @param full_blocks  the full blocks to process
     * @return xunqualified_node_info_t  the statistic data
     */
    xunqualified_node_info_t process_fulltableblock(std::vector<base::xauto_ptr<xblock_t>> const& full_blocks);


    /**
     * @brief accumulate  node info of all tables
     *
     * @param  node_info  the node info
     *
     */
    xunqualified_node_info_t  accumulate_node_info(xunqualified_node_info_t const&  node_info);


};

NS_END3

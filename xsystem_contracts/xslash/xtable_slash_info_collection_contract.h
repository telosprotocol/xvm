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
 * @brief the table slash contract
 *
 */
class xtable_slash_info_collection_contract final : public xcontract_base {
    using xbase_t = xcontract_base;
public:
    XDECLARE_DELETED_COPY_DEFAULTED_MOVE_SEMANTICS(xtable_slash_info_collection_contract);
    XDECLARE_DEFAULTED_OVERRIDE_DESTRUCTOR(xtable_slash_info_collection_contract);

    explicit
    xtable_slash_info_collection_contract(common::xnetwork_id_t const & network_id);

    xcontract_base*
    clone() override {
        return new xtable_slash_info_collection_contract(network_id());
    }

    void
    setup();

    /**
     * @brief  collect the slash info on tables
     *
     * @param timestamp  the logic time to collect slash info
     */
    void
    on_collect_slash_info(common::xlogic_time_t const timestamp);

    BEGIN_CONTRACT_WITH_PARAM(xtable_slash_info_collection_contract)
        CONTRACT_FUNCTION_PARAM(xtable_slash_info_collection_contract, on_collect_slash_info);
    END_CONTRACT_WITH_PARAM

private:
    /**
     * @brief judge auditor or validator group by group id
     *
     * @param id the group id
     * @return true
     * @return false
     */
    bool is_validtor_group(common::xgroup_id_t const& id);
    bool is_auditor_group(common::xgroup_id_t const& id);

    /**
     * @brief get the latest tablefullblocks from last read height
     *
     * @param owner the owner addr of the full tableblock
     * @param blockchain_height current tableblock height
     * @param last_read_height the height last time read
     */
    std::vector<base::xauto_ptr<xblock_t>> get_next_fulltableblock(common::xaccount_address_t const& owner, uint64_t blockchain_height, uint64_t last_read_height = 0);


    /**
     * @brief process full tableblocks to get statistic data
     *
     * @param full_blocks  the full blocks to process
     * @return xunqualified_node_info_t  the statistic data
     */
    xunqualified_node_info_t process_fulltableblock(std::vector<base::xauto_ptr<xblock_t>> const& full_blocks);

};

NS_END3

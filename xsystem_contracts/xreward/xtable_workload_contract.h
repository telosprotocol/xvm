// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xcommon/xrole_type.h"
#include "xstake/xstake_algorithm.h"
#include "xvm/xcontract/xcontract_base.h"
#include "xvm/xcontract/xcontract_exec.h"

#include <type_traits>

NS_BEG2(top, xstake)

using namespace xvm;
using namespace xvm::xcontract;

class xtable_workload_contract final : public xcontract_base {
    using xbase_t = xcontract_base;
public:
    XDECLARE_DELETED_COPY_DEFAULTED_MOVE_SEMANTICS(xtable_workload_contract);
    XDECLARE_DEFAULTED_OVERRIDE_DESTRUCTOR(xtable_workload_contract);

    explicit
    xtable_workload_contract(common::xnetwork_id_t const & network_id);

    xcontract_base*
    clone() override {return new xtable_workload_contract(network_id());}

    /**
     * @brief setup the contract
     *
     */
    void setup();

    /**
     * @brief report workload to zec if we satisfy the conditions
     *
     * @param onchain_timer_round chain timer round
     */
    void on_timer(const uint64_t onchain_timer_round);

    BEGIN_CONTRACT_WITH_PARAM(xtable_workload_contract)
        CONTRACT_FUNCTION_PARAM(xtable_workload_contract, on_timer);
    END_CONTRACT_WITH_PARAM

private:
    /**
     * @brief get_next_fulltableblock
     */
    std::vector<base::xauto_ptr<data::xblock_t>> get_fullblock(common::xaccount_address_t const& owner, uint64_t last_read_height, uint64_t & cur_read_height);
    
    /**
     * @brief get_table_height
     */
    void get_table_height(common::xaccount_address_t const & table_owner, uint64_t & last_read_height, uint64_t & old_version_height);
    
    /**
     * @brief update_table_height
     */    
    void update_table_height(common::xaccount_address_t const & table_owner, const uint64_t cur_read_height, const uint64_t old_version_height);

    /**
     * @brief add_workload_with_fullblock
     */ 
    void add_workload_with_fullblock(common::xaccount_address_t const & account);
};


NS_END2

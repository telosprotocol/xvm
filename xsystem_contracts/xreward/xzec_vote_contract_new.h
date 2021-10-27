// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xdata/xtableblock.h"
#include "xstake/xstake_algorithm.h"
#include "xsystem_contracts/xbasic_system_contract.h"
#include "xsystem_contract_runtime/xsystem_contract_runtime_helper.h"

NS_BEG2(top, system_contracts)

class xzec_vote_contract_new : public xbasic_system_contract_t {
    using xbase_t = xbasic_system_contract_t;
public:
    /**
     * @brief setup the contract
     *
     */
    void        setup();

    /**
     * @brief receive contract nodes votes
     *
     * @param report_time
     * @param contract_adv_votes
     */
    void on_receive_shard_votes_v2(uint64_t report_time, std::map<std::string, std::string> const & contract_adv_votes);

    BEGIN_CONTRACT_API()
        DECLARE_API(xzec_vote_contract_new::setup);
        DECLARE_API(xzec_vote_contract_new::on_receive_shard_votes_v2);
    END_CONTRACT_API

private:
    /**
     * @brief check if mainnet is activated
     *
     * @return int 0 - not activated, other - activated
     */
    int         is_mainnet_activated();

    /**
     * @brief
     *
     * @param report_time
     * @param last_report_time
     * @param contract_adv_votes
     * @param merge_contract_adv_votes
     * @return true
     * @return false
     */
    bool        handle_receive_shard_votes(uint64_t report_time, uint64_t last_report_time, std::map<std::string, std::string> const & contract_adv_votes, std::map<std::string, std::string> & merge_contract_adv_votes);

private:
    contract_common::properties::xmap_property_t<std::string, std::string> m_ticket_prop{xstake::XPORPERTY_CONTRACT_TICKETS_KEY, this};
    contract_common::properties::xmap_property_t<std::string, std::string> m_vote_report_time_prop{xstake::XPORPERTY_CONTRACT_VOTE_REPORT_TIME_KEY, this};
};

NS_END2

// Copyright (c) 2017-present Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.#pragma once

#pragma once

#include "xbase/xdata.h"
#include "xcontract_common/xproperties/xproperty_string.h"
#include "xcontract_common/xproperties/xproperty_map.h"
#include "xdata/xdata_common.h"
#include "xdata/xgenesis_data.h"
#include "xdata/xproposal_data.h"
#include "xsystem_contracts/xbasic_system_contract.h"
#include "xsystem_contract_runtime/xsystem_contract_runtime_helper.h"

NS_BEG2(top, system_contracts)

/**
 * @brief the proposal contract
 *
 */
class xtop_rec_tcc_contract final : public xbasic_system_contract_t {
    using xbase_t = xbasic_system_contract_t;

    contract_common::properties::xmap_property_t<std::string, std::string> m_tcc_parameters{ONCHAIN_PARAMS, this};
    contract_common::properties::xstring_property_t m_tcc_next_unused_proposal_id{SYSTEM_GENERATED_ID, this};
    contract_common::properties::xmap_property_t<std::string, std::string> m_tcc_proposal_ids{PROPOSAL_MAP_ID, this};
    contract_common::properties::xmap_property_t<std::string, std::string> m_tcc_vote_ids{VOTE_MAP_ID, this};
    contract_common::properties::xstring_property_t m_tcc_voted_proposal{CURRENT_VOTED_PROPOSAL, this};

public:
    void setup();

    void charge(std::string const & token_name, uint64_t token_amount);

    /**
     * @brief sumbit a tcc proposal
     *
     * @param target  the proposal target
     * @param value  the proposal target value
     * @param type  the proposal type
     * @param effective_timer_height  the proposal take effect timer height
     */
    void submitProposal(const std::string & target, const std::string & value, tcc::proposal_type type, uint64_t effective_timer_height);

    /**
     * @brief withdraw the proposal
     *
     * @param proposal_id  the proposal id
     */
    void withdrawProposal(const std::string & proposal_id);

    /**
     * @brief tcc member vote the proposal
     *
     * @param proposal_id  the proposal id
     * @param option  the vote option, true means agree, false means disagree
     */
    void tccVote(std::string const & proposal_id, bool option);
    /**
     * @brief Get the proposal info object
     *
     * @param proposal_id  the proposal id
     * @param proposal  to store the proposal geted
     * @return true
     * @return false
     */

    BEGIN_CONTRACT_API()
        DECLARE_API(xtop_rec_tcc_contract::setup);
        DECLARE_API(xtop_rec_tcc_contract::submitProposal);
        DECLARE_API(xtop_rec_tcc_contract::withdrawProposal);
        DECLARE_API(xtop_rec_tcc_contract::tccVote);
        DECLARE_SEND_ONLY_API(xtop_rec_tcc_contract::charge);
        // DECLARE_CONFIRM_ONLY_API(xtop_rec_tcc_contract::confirm_charge);
    END_CONTRACT_API

private:

    bool get_proposal_info(const std::string& proposal_id, tcc::proposal_info& proposal);

    /**
     * @brief check whether the proposal expired
     *
     * @param proposal_id  the proposal id
     * @return true
     * @return false
     */
    bool proposal_expired(const std::string& proposal_id);

    /**
     * @brief check whether the voter is committee member
     *
     * @param voter_client_addr
     * @return true
     * @return false
     */
    bool voter_in_committee(const std::string& voter_client_addr);

    /**
     * @brief check whether the proposal tpye is valid
     *
     * @param type  the proposal type
     * @return true
     * @return false
     */
    bool is_valid_proposal_type(tcc::proposal_type type);

    /**
     * @brief delete the expired proposal
     *
     */
    void delete_expired_proposal();

    /**
     * @brief check bwlist proposal parameter
     *
     * @param bwlist the whitelist or blacklist
     *
     */
    void check_bwlist_proposal(std::string const& bwlist);
};
using xrec_tcc_contract_new_t = xtop_rec_tcc_contract;

NS_END2

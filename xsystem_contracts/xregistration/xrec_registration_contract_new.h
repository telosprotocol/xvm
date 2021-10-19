// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xcontract_common/xproperties/xproperty_map.h"
#include "xcontract_common/xproperties/xproperty_string.h"
#include "xcontract_common/xproperties/xproperty_token.h"
#include "xsystem_contract_runtime/xsystem_contract_runtime_helper.h"
#include "xstake/xstake_algorithm.h"
#include "xsystem_contracts/xbasic_system_contract.h"

NS_BEG2(top, system_contracts)

class xtop_rec_registration_contract_new final : public xbasic_system_contract_t {
    using xbase_t = xbasic_system_contract_t;
public:
    xtop_rec_registration_contract_new() = default;
    xtop_rec_registration_contract_new(xtop_rec_registration_contract_new const &) = delete;
    xtop_rec_registration_contract_new & operator=(xtop_rec_registration_contract_new const &) = delete;
    xtop_rec_registration_contract_new(xtop_rec_registration_contract_new &&) = default;
    xtop_rec_registration_contract_new & operator=(xtop_rec_registration_contract_new &&) = default;
    ~xtop_rec_registration_contract_new() override = default;

    explicit xtop_rec_registration_contract_new(observer_ptr<contract_common::xcontract_execution_context_t> initial_construction_exection_context);

    BEGIN_CONTRACT_API()
        DECLARE_API(xtop_rec_registration_contract_new::setup);
        DECLARE_API(xtop_rec_registration_contract_new::registerNode);
        DECLARE_API(xtop_rec_registration_contract_new::unregisterNode);
        DECLARE_SENDER_ONLY_API(xtop_rec_registration_contract_new::src_action_registerNode);
        // DECLARE_API(xtop_rec_registration_contract_new::updateNodeInfo);
        // DECLARE_API(xtop_rec_registration_contract_new::setDividendRatio);
        // DECLARE_API(xtop_rec_registration_contract_new::setNodeName);
        // DECLARE_API(xtop_rec_registration_contract_new::update_batch_stake);
        // DECLARE_API(xtop_rec_registration_contract_new::update_batch_stake_v2);
        // DECLARE_API(xtop_rec_registration_contract_new::redeemNodeDeposit);
        // DECLARE_API(xtop_rec_registration_contract_new::updateNodeType);
        // DECLARE_API(xtop_rec_registration_contract_new::stakeDeposit);
        // DECLARE_API(xtop_rec_registration_contract_new::unstakeDeposit);
        // DECLARE_API(xtop_rec_registration_contract_new::updateNodeSignKey);
        // DECLARE_API(xrec_registration_contract_new::slash_unqualified_node);
    END_CONTRACT_API

    /**
     * @brief setup the contract
     *
     */
    void setup();

    /**
     * @brief register the node
     *
     * @param node_types
     * @param nickname
     * @param signing_key
     */
    void registerNode(const std::string & node_types,
                      const std::string & nickname,
                      const std::string & signing_key,
                      const uint32_t dividend_rate
#if defined XENABLE_MOCK_ZEC_STAKE
                   , common::xaccount_address_t const & registration_account
#endif
    );

    void src_action_registerNode(std::string const& token_name, uint64_t token_amount);
    /**
     * @brief unregister the node
     *
     */
    void unregisterNode();
#if 0
    void updateNodeInfo(const std::string & nickname, const int updateDepositType, const uint64_t deposit, const uint32_t dividend_rate, const std::string & node_types, const std::string & node_sign_key);
    /**
     * @brief Set the Dividend Ratio
     *
     * @param dividend_rate
     */
    void setDividendRatio(uint32_t dividend_rate);

    /**
     * @brief Set the Node Name
     *
     * @param nickname
     */
    void setNodeName(const std::string & nickname);

    // void update_node_credit(std::set<std::string> const& accounts);

    /**
     * @brief batch update stakes
     *
     * @param adv_votes
     */
    void update_batch_stake(std::map<std::string, std::string> const& adv_votes);

    /**
     * @brief batch update stakes v2, considering trx splitting
     *
     * @param report_time
     * @param contract_adv_votes
     */
    void update_batch_stake_v2(uint64_t report_time, std::map<std::string, std::string> const & contract_adv_votes);

    /**
     * @brief redeem node deposit
     *
     */
    void redeemNodeDeposit();

    /**
     * @brief update node type
     *
     * @param node_types
     */
    void updateNodeType(const std::string & node_types);

    /**
     * @brief stake deposit
     *
     */
    void stakeDeposit();

    /**
     * @brief unstake deposit
     *
     * @param unstake_deposit
     */
    void unstakeDeposit(uint64_t unstake_deposit);

    void updateNodeSignKey(const std::string & node_sign_key);

    /**
     * @brief slave unqualified node
     *
     * @param punish_node_str
     */
    void slash_unqualified_node(std::string const& punish_node_str);
#endif
private:
    /**
     * @brief register the node
     *
     * @param node_types
     * @param nickname
     * @param signing_key
     * @param network_ids
     */
    void registerNode2(const std::string & node_types,
                       const std::string & nickname,
                       const std::string & signing_key,
                       const uint32_t dividend_rate,
                       const std::set<common::xnetwork_id_t> & network_ids
#if defined XENABLE_MOCK_ZEC_STAKE
                     , common::xaccount_address_t const & registration_account
#endif
    );

    /**
     * @brief update node info
     *
     * @param node_info
     */
    void update_node_info(xstake::xreg_node_info & node_info);

    /**
     * @brief delete node info
     *
     * @param account
     */
    void delete_node_info(std::string const & account);

    /**
     * @brief Get the node info
     *
     * @param account
     * @param node_info
     * @return int32_t
     */
    int32_t get_node_info(const std::string & account, xstake::xreg_node_info & node_info);

    /**
     * @brief insert the refund info
     *
     * @param account
     * @param refund_amount
     * @return int32_t
     */
    int32_t ins_refund(const std::string & account, uint64_t const & refund_amount);

    /**
     * @brief delete the refund info
     *
     * @param account
     * @return int32_t
     */
    int32_t del_refund(const std::string & account);

    /**
     * @brief Get the refund info
     *
     * @param account
     * @param refund
     * @return int32_t
     */
    int32_t get_refund(const std::string & account, xstake::xrefund_info & refund);

    /**
     * @brief check and set mainnet activation
     *
     */
    void check_and_set_genesis_stage();

    /**
     * @brief Get the slash info
     *
     * @param account
     * @param node_slash_info
     * @return int32_t
     */
    int32_t get_slash_info(std::string const & account, xstake::xslash_info & node_slash_info);
#if 0
    /**
     * @brief get slash staking time
     *
     * @param node_addr
     */
    void        slash_staking_time(std::string const& node_addr);
#endif
    /**
     * @brief check if a valid nickname
     *
     * @param nickname
     * @return true
     * @return false
     */
    bool is_valid_name(const std::string & nickname) const;
#if 0
    /**
     * @brief check if signing key exists
     *
     * @param signing_key
     * @return true
     * @return false
     */
    bool        check_if_signing_key_exist(const std::string & signing_key);

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
#endif
private:
    contract_common::properties::xstring_property_t m_genesis_prop{xstake::XPORPERTY_CONTRACT_GENESIS_STAGE_KEY, this};
    contract_common::properties::xmap_property_t<std::string, std::string> m_reg_prop{xstake::XPORPERTY_CONTRACT_REG_KEY, this};
    contract_common::properties::xmap_property_t<std::string, std::string> m_tickets_prop{xstake::XPORPERTY_CONTRACT_TICKETS_KEY, this};
    contract_common::properties::xmap_property_t<std::string, std::string> m_refund_prop{xstake::XPORPERTY_CONTRACT_REFUND_KEY, this};
    contract_common::properties::xmap_property_t<std::string, std::string> m_slash_prop{xstake::XPROPERTY_CONTRACT_SLASH_INFO_KEY, this};
    contract_common::properties::xmap_property_t<std::string, std::string> m_votes_report_time_prop{xstake::XPORPERTY_CONTRACT_VOTE_REPORT_TIME_KEY, this};
};
using xrec_registration_contract_new_t = xtop_rec_registration_contract_new;


NS_END2

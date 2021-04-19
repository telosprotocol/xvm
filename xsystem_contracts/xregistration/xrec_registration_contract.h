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

struct xproperty_instruction_t {
    std::string name;
    data::xproperty_op_code_t op_code;
    std::string op_para1;
    std::string op_para2;
};

class xrec_registration_contract : public xcontract_base {
    using xbase_t = xcontract_base;
public:
    XDECLARE_DELETED_COPY_DEFAULTED_MOVE_SEMANTICS(xrec_registration_contract);
    XDECLARE_DEFAULTED_OVERRIDE_DESTRUCTOR(xrec_registration_contract);

    explicit
    xrec_registration_contract(common::xnetwork_id_t const & network_id);

    xcontract_base*
    clone() override {return new xrec_registration_contract(network_id());}

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
                      ,
                      std::string const & registration_account
#endif
                      );
    /**
     * @brief unregister the node
     *
     */
    void unregisterNode();

    void updateNodeInfo(const std::string & nickname, const int updateDepositType, const uint64_t deposit, const uint32_t dividend_rate, const std::string & node_types, const std::string & node_sign_key);
    
    /**
     * @brief update node type
     *
     * @param node_types
     */
    
    void updateNodeType(const std::string & node_types);  

    void updateNodeSignKey(const std::string & node_sign_key);

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

    /**
     * @brief slave unqualified node
     *
     * @param punish_node_str
     */
    void slash_unqualified_node(std::string const& punish_node_str);

    BEGIN_CONTRACT_WITH_PARAM(xrec_registration_contract)
        CONTRACT_FUNCTION_PARAM(xrec_registration_contract, registerNode);
        CONTRACT_FUNCTION_PARAM(xrec_registration_contract, unregisterNode);
        CONTRACT_FUNCTION_PARAM(xrec_registration_contract, updateNodeInfo);
        CONTRACT_FUNCTION_PARAM(xrec_registration_contract, setDividendRatio);
        CONTRACT_FUNCTION_PARAM(xrec_registration_contract, setNodeName);
        CONTRACT_FUNCTION_PARAM(xrec_registration_contract, update_batch_stake);
        CONTRACT_FUNCTION_PARAM(xrec_registration_contract, update_batch_stake_v2);
        CONTRACT_FUNCTION_PARAM(xrec_registration_contract, redeemNodeDeposit);
        CONTRACT_FUNCTION_PARAM(xrec_registration_contract, updateNodeType);
        CONTRACT_FUNCTION_PARAM(xrec_registration_contract, stakeDeposit);
        CONTRACT_FUNCTION_PARAM(xrec_registration_contract, unstakeDeposit);
        CONTRACT_FUNCTION_PARAM(xrec_registration_contract, updateNodeSignKey);
        CONTRACT_FUNCTION_PARAM(xrec_registration_contract, slash_unqualified_node);
    END_CONTRACT_WITH_PARAM

private:
    /**
     * @brief register the node
     * @param account
     * @param node_types
     * @param nickname
     * @param signing_key
     * @param dividend_rate
     * @param network_ids
     * @param asset_out
     * @param node_info
     */
    xproperty_instruction_t registerNode2(common::xaccount_address_t const & account,
                                          const std::string & node_types,
                                          const std::string & nickname,
                                          const std::string & signing_key,
                                          const uint32_t dividend_rate,
                                          const std::set<common::xnetwork_id_t> & network_ids,
                                          data::xproperty_asset const & asset_out,
                                          xreg_node_info & node_info);

    /**
     * @brief handleNodeInfo
     * @param cur_time
     * @param nickname
     * @param dividend_rate
     * @param node_types
     * @param node_sign_key
     * @param node_info
     */
    xproperty_instruction_t handleNodeInfo(const uint64_t cur_time,
                                           const std::string & nickname,
                                           const uint32_t dividend_rate,
                                           const std::string & node_types,
                                           const std::string & node_sign_key,
                                           xreg_node_info & node_info);

    /**
     * @brief handleNodeType
     *
     * @param node_types
     * @param asset_out
     * @param node_info
     * @return xproperty_instruction_t
     */
    xproperty_instruction_t handleNodeType(const std::string & node_types, const data::xproperty_asset & asset_out, xreg_node_info & node_info);
    
    /**
     * @brief handleNodeSignKey
     *
     * @param node_sign_key
     * @param node_info
     * @return xproperty_instruction_t
     */
    xproperty_instruction_t handleNodeSignKey(const std::string & node_sign_key, xreg_node_info & node_info);
    
    /**
     * @brief handleDividendRatio
     *
     * @param cur_time
     * @param dividend_rate
     * @param node_info
     * @return xproperty_instruction_t
     */
    xproperty_instruction_t handleDividendRatio(common::xlogic_time_t cur_time, uint32_t dividend_rate, xreg_node_info & node_info);
    
    /**
     * @brief handleNodeName
     *
     * @param nickname
     * @param node_info
     * @return xproperty_instruction_t
     */
    xproperty_instruction_t handleNodeName(const std::string & nickname, xreg_node_info & node_info);
    
    /**
     * @brief create_binlog
     *
     * @param name
     * @param op_code
     * @param op_para1
     * @param op_para2
     * @return xproperty_instruction_t
     */
    xproperty_instruction_t create_binlog(std::string name, data::xproperty_op_code_t op_code, std::string op_para1, xreg_node_info op_para2);
    
    /**
     * @brief execute_instruction
     *
     * @param instruction
     */
    void execute_instruction(xproperty_instruction_t instruction);

    /**
     * @brief update node info
     *
     * @param node_info
     */
    void update_node_info(xreg_node_info & node_info, std::error_code & ec);

    /**
     * @brief delete node info
     *
     * @param account
     */
    void delete_node_info(common::xaccount_address_t const & account);

    /// @brief Query the node info.
    /// @param account The account to be queried.
    /// @param ec Error code.
    /// @return The registration info.
    xreg_node_info get_node_info(common::xaccount_address_t const & account, std::error_code & ec) const ;

    /**
     * @brief insert the refund info
     *
     * @param account
     * @param refund_amount
     * @param ec
     * @return xproperty_instruction_t
     */
    void update_node_refund(common::xaccount_address_t const & account, uint64_t const & refund_amount, std::error_code & ec);

    /**
     * @brief delete the refund info
     *
     * @param account
     * @return xproperty_instruction_t
     */
    void delete_node_refund(common::xaccount_address_t const & account);

    /**
     * @brief Get the refund info
     *
     * @param account
     * @param ec
     * @return xrefund_info
     */
    xrefund_info get_node_refund(common::xaccount_address_t const & account, std::error_code & ec) const;

    /**
     * @brief check and set mainnet activation
     *
     */
    void        check_and_set_genesis_stage();

    /**
     * @brief Get the slash info
     *
     * @param account
     * @param node_slash_info
     * @return xslash_info
     */
    xslash_info get_slash_info(common::xaccount_address_t const & account, std::error_code & ec) const ;
    
    /**
     * @brief Get the slash info
     *
     * @param account
     * @param s_info
     * @param ec
     * @return xproperty_instruction_t
     */
    void update_slash_info(common::xaccount_address_t const & account, xslash_info const & s_info, std::error_code & ec) ;
    
    /**
     * @brief get slash staking time
     *
     * @param node_addr
     */
    void slash_staking_time(common::xaccount_address_t const & account);

    /**
     * @brief check if a valid nickname
     *
     * @param nickname
     * @return true
     * @return false
     */
    bool        is_valid_name(const std::string & nickname) const;

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
};


NS_END2

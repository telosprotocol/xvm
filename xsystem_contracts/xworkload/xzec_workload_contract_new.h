// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xcontract_common/xproperties/xproperty_map.h"
#include "xcontract_common/xproperties/xproperty_string.h"
#include "xsystem_contract_runtime/xsystem_contract_runtime_helper.h"
#include "xsystem_contracts/xbasic_system_contract.h"

NS_BEG2(top, system_contracts)

class xtop_zec_workload_contract_new : public xbasic_system_contract_t {
    using xbase_t = xbasic_system_contract_t;

public:
    xtop_zec_workload_contract_new() = default;
    xtop_zec_workload_contract_new(xtop_zec_workload_contract_new const &) = delete;
    xtop_zec_workload_contract_new & operator=(xtop_zec_workload_contract_new const &) = delete;
    xtop_zec_workload_contract_new(xtop_zec_workload_contract_new &&) = default;
    xtop_zec_workload_contract_new & operator=(xtop_zec_workload_contract_new &&) = default;
    ~xtop_zec_workload_contract_new() = default;

    BEGIN_CONTRACT_API()
        DECLARE_API(xtop_zec_workload_contract_new::setup);
        DECLARE_API(xtop_zec_workload_contract_new::on_receive_workload);
        DECLARE_SELF_ONLY_API(xtop_zec_workload_contract_new::on_timer);
    END_CONTRACT_API

    /**
     * @brief setup the contract
     *
     */
    void setup();

    /**
     * @brief call zec reward contract to calculate reward
     *
     * @param timestamp the time to call
     */
    void on_timer(common::xlogic_time_t const timestamp);

    /**
     * @brief process on receiving workload
     *
     * @param workload_str workload
     */
    void on_receive_workload(std::string const & workload_str);

private:
    /**
     * @brief handle_workload_str
     *
     * @param workload_str workload
     * @param activation_record_str is_mainnet_active
     */
    void handle_workload_str(const std::string & activation_record_str,
                             const std::string & table_info_str,
                             const std::map<std::string, std::string> & workload_str,
                             const std::string & tgas_str,
                             const std::string & height_str,
                             std::map<std::string, std::string> & workload_str_new,
                             std::string & tgas_str_new);

    /**
     * @brief stash_workload
     */
    void update_workload(std::map<common::xgroup_address_t, xstake::xgroup_workload_t> const & group_workload,
                         const std::map<std::string, std::string> & workload_str,
                         std::map<std::string, std::string> & workload_new);

    /**
     * @brief upload_workload
     */
    void upload_workload(common::xlogic_time_t const timestamp);

    /**
     * @brief upload_workload_internal
     */
    void upload_workload_internal(common::xlogic_time_t const timestamp, std::map<std::string, std::string> const & workload_str, std::string & call_contract_str);

    /**
     * @brief clear_workload
     */
    void clear_workload();

    contract_common::properties::xmap_property_t<std::string, std::string> m_table_height{xstake::XPORPERTY_CONTRACT_TABLEBLOCK_HEIGHT_KEY, this};
    contract_common::properties::xmap_property_t<std::string, std::string> m_workload{xstake::XPORPERTY_CONTRACT_WORKLOAD_KEY, this};
    contract_common::properties::xmap_property_t<std::string, std::string> m_validator_workload{xstake::XPORPERTY_CONTRACT_VALIDATOR_WORKLOAD_KEY, this};
    contract_common::properties::xstring_property_t m_tgas{xstake::XPORPERTY_CONTRACT_TGAS_KEY, this};
};
using xzec_workload_contract_new_t = xtop_zec_workload_contract_new;

NS_END2

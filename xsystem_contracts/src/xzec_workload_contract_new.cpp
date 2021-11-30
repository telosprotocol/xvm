// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xworkload/xzec_workload_contract_new.h"

#include "xdata/xnative_contract_address.h"
#include "xstake/xstake_algorithm.h"
#include "xstate_accessor/xproperties/xproperty_identifier.h"

#if !defined(XZEC_MODULE)
#    define XZEC_MODULE "SysContract_"
#endif
#define XCONTRACT_PREFIX "ZecWorkload_"
#define XWORKLOAD_CONTRACT XZEC_MODULE XCONTRACT_PREFIX

NS_BEG2(top, system_contracts)

void xtop_zec_workload_contract_new::setup() {
}

void xtop_zec_workload_contract_new::on_receive_workload(std::string const & table_info_str) {
    XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "on_receive_workload");
    XMETRICS_COUNTER_INCREMENT(XWORKLOAD_CONTRACT "on_receive_workload", 1);
    XCONTRACT_ENSURE(!table_info_str.empty(), "workload_str empty");

    auto const & source_address = sender();
    auto const & table_id = source_address.table_id();
    auto const & base_addr = source_address.base_address();
    if (sys_contract_sharding_statistic_info_addr != base_addr.to_string()) {
        xwarn("[xzec_workload_contract_new_t::on_receive_workload] invalid call from %s", source_address.c_str());
        return;
    }
    xinfo("[xzec_workload_contract_new_t::on_receive_workload] on_receive_workload call from %s", source_address.c_str());

    std::string activation_str;
    std::map<std::string, std::string> workload_str;
    std::string tgas_str;
    std::string height_str;
    std::map<std::string, std::string> workload_str_new;
    std::string tgas_str_new;
    {
        XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "on_receive_workload_map_get");
        auto const & activation_property =
            get_property<contract_common::properties::xstring_property_t>(state_accessor::properties::xtypeless_property_identifier_t{xstake::XPORPERTY_CONTRACT_GENESIS_STAGE_KEY},
                                                                          common::xaccount_address_t{sys_contract_rec_registration_addr});
        activation_str = activation_property.value();
        workload_str = m_workload.value();
        tgas_str = m_tgas.value();
        height_str = m_table_height.get(top::to_string(table_id));
    }

    handle_workload_str(activation_str, table_info_str, workload_str, tgas_str, height_str, workload_str_new, tgas_str_new);

    {
        XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "on_receive_workload_map_set");
        for (auto it = workload_str_new.cbegin(); it != workload_str_new.end(); it++) {
            m_workload.set(it->first, it->second);
        }
        if (!tgas_str_new.empty() && tgas_str != tgas_str_new) {
            m_tgas.set(tgas_str_new);
        }
    }
}

void xtop_zec_workload_contract_new::handle_workload_str(const std::string & activation_record_str,
                                                    const std::string & table_info_str,
                                                    const std::map<std::string, std::string> & workload_str,
                                                    const std::string & tgas_str,
                                                    const std::string & height_str,
                                                    std::map<std::string, std::string> & workload_str_new,
                                                    std::string & tgas_str_new) {
    XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "handle_workload_str");
    std::map<common::xgroup_address_t, xstake::xgroup_workload_t> group_workload;
    int64_t table_pledge_balance_change_tgas = 0;
    uint64_t height = 0;
    {
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)table_info_str.data(), table_info_str.size());
        MAP_OBJECT_DESERIALZE2(stream, group_workload);
        stream >> table_pledge_balance_change_tgas;
        stream >> height;
    }
    xinfo("[xzec_workload_contract::handle_workload_str] group_workload size: %zu, table_pledge_balance_change_tgas: %lld, height: %lu, last height: %lu\n",
          group_workload.size(),
          table_pledge_balance_change_tgas,
          height,
          base::xstring_utl::touint64(height_str));

    XCONTRACT_ENSURE(base::xstring_utl::touint64(height_str) < height, "zec_last_read_height >= table_previous_height");

    // update system total tgas
    int64_t tgas = 0;
    if (!tgas_str.empty()) {
        tgas = base::xstring_utl::toint64(tgas_str);
    }
    tgas_str_new = base::xstring_utl::tostring(tgas += table_pledge_balance_change_tgas);

    xstake::xactivation_record record;
    if (!activation_record_str.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)activation_record_str.c_str(), (uint32_t)activation_record_str.size());
        record.serialize_from(stream);
    }

    xdbg("[xzec_workload_contract_new_t::is_mainnet_activated] activated: %d\n", record.activated);
    if (!record.activated) {
        return;
    }
    update_workload(group_workload, workload_str, workload_str_new);
}

void xtop_zec_workload_contract_new::update_workload(const std::map<common::xgroup_address_t, xstake::xgroup_workload_t> & group_workload,
                                                const std::map<std::string, std::string> & workload_str,
                                                std::map<std::string, std::string> & workload_new) {
    XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "update_workload");
    for (auto const & one_group_workload : group_workload) {
        auto const & group_address = one_group_workload.first;
        auto const & workload = one_group_workload.second;
        // get
        std::string group_address_str;
        {
            base::xstream_t stream(base::xcontext_t::instance());
            stream << group_address;
            group_address_str = std::string((const char *)stream.data(), stream.size());
        }
        xstake::xgroup_workload_t total_workload;
        {
            auto it = workload_str.find(group_address_str);
            if (it == workload_str.end()) {
                xdbg("[xzec_workload_contract_new_t::update_workload] group not exist: %s", group_address.to_string().c_str());
            } else {
                base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)it->second.data(), it->second.size());
                total_workload.serialize_from(stream);
            }
        }
        // update
        for (auto const & leader_workload : workload.m_leader_count) {
            auto const & leader = leader_workload.first;
            auto const & count = leader_workload.second;
            total_workload.m_leader_count[leader] += count;
            total_workload.cluster_total_workload += count;
            xdbg("[xzec_workload_contract_new_t::update_workload] group: %u, leader: %s, count: %d, total_count: %d, total_workload: %d",
                 group_address.group_id().value(),
                 leader.c_str(),
                 count,
                 total_workload.m_leader_count[leader],
                 total_workload.cluster_total_workload);
        }
        // set
        {
            base::xstream_t stream(base::xcontext_t::instance());
            total_workload.serialize_to(stream);
            workload_new.insert(std::make_pair(group_address_str, std::string((const char *)stream.data(), stream.size())));
        }
    }
}

void xtop_zec_workload_contract_new::upload_workload(common::xlogic_time_t const timestamp) {
    XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "upload_workload_time");
    XMETRICS_CPU_TIME_RECORD(XWORKLOAD_CONTRACT "upload_workload_cpu_time");
    std::string call_contract_str{};
    auto const & group_workload_str = m_workload.value();
    upload_workload_internal(timestamp, group_workload_str, call_contract_str);
    if (!call_contract_str.empty()) {
        call(common::xaccount_address_t{sys_contract_zec_reward_addr}, "calculate_reward", call_contract_str, contract_common::xfollowup_transaction_schedule_type_t::immediately);
    }
}

void xtop_zec_workload_contract_new::upload_workload_internal(common::xlogic_time_t const timestamp,
                                                              std::map<std::string, std::string> const & workload_str,
                                                              std::string & call_contract_str) {
    std::map<common::xgroup_address_t, xstake::xgroup_workload_t> group_workload_upload;

    for (auto it = workload_str.begin(); it != workload_str.end(); it++) {
        base::xstream_t key_stream(base::xcontext_t::instance(), (uint8_t *)it->first.data(), it->first.size());
        common::xgroup_address_t group_address;
        key_stream >> group_address;
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)it->second.data(), it->second.size());
        xstake::xgroup_workload_t group_workload;
        group_workload.serialize_from(stream);

        for (auto const & leader_workload : group_workload.m_leader_count) {
            auto const & leader = leader_workload.first;
            auto const & workload = leader_workload.second;

            auto it2 = group_workload_upload.find(group_address);
            if (it2 == group_workload_upload.end()) {
                xstake::xgroup_workload_t empty_workload;
                auto ret = group_workload_upload.insert(std::make_pair(group_address, empty_workload));
                it2 = ret.first;
            }
            it2->second.m_leader_count[leader] += workload;
        }
    }
    if (group_workload_upload.size() > 0) {
        std::string group_workload_upload_str;
        {
            base::xstream_t stream(base::xcontext_t::instance());
            MAP_OBJECT_SERIALIZE2(stream, group_workload_upload);
            group_workload_upload_str = std::string((char *)stream.data(), stream.size());

            xinfo("[xzec_workload_contract_new_t::upload_workload] report workload to zec reward, group_workload_upload size: %d, timer round: %" PRIu64,
                  group_workload_upload.size(),
                  timestamp);
        }
        {
            base::xstream_t stream(base::xcontext_t::instance());
            stream << timestamp;
            stream << group_workload_upload_str;
            call_contract_str = std::string((char *)stream.data(), stream.size());
            group_workload_upload.clear();
        }
    }
}

void xtop_zec_workload_contract_new::clear_workload() {
    XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "XPORPERTY_CONTRACT_WORKLOAD_KEY_SetExecutionTime");
    m_workload.clear();
}

void xtop_zec_workload_contract_new::on_timer(common::xlogic_time_t const timestamp) {
    XMETRICS_TIME_RECORD(XWORKLOAD_CONTRACT "on_timer");
    XMETRICS_CPU_TIME_RECORD(XWORKLOAD_CONTRACT "on_timer_cpu_time");

    auto const & source_address = sender();
    auto const & self_account = recver();
    if (self_account != source_address) {
        xwarn("[xzec_workload_contract_new_t::on_timer] invalid call from %s", source_address.c_str());
        return;
    }
    xinfo("[xzec_workload_contract_new_t::on_timer] timestamp: %lu, self: %s, src: %s", timestamp, self_account.value().c_str(), source_address.value().c_str());
    upload_workload(timestamp);
    clear_workload();
}

NS_END2

#undef XWORKLOAD_CONTRACT
#undef XCONTRACT_PREFIX
#undef XZEC_MODULE

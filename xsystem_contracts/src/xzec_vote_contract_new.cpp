// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xreward/xzec_vote_contract_new.h"

#include "xbase/xutl.h"
#include "xbasic/xutility.h"
#include "xchain_upgrade/xchain_data_processor.h"
#include "xdata/xgenesis_data.h"
#include "xstake/xstake_algorithm.h"
#include "xstore/xstore_error.h"

#include <iomanip>

using top::base::xstream_t;
using top::base::xcontext_t;
using top::base::xstring_utl;
using namespace top::data;

#if !defined (XZEC_MODULE)
#    define XZEC_MODULE "SysContract_"
#endif
#define XCONTRACT_PREFIX "ZecVote_"
#define XVOTE_CONTRACT XZEC_MODULE XCONTRACT_PREFIX

NS_BEG2(top, system_contracts)

void xzec_vote_contract_new::setup() {
    // save shard total tickets
    // vote related
    const uint32_t old_tables_count = 256;
    for (auto table = 0; table < enum_vledger_const::enum_vbucket_has_tables_count; table++) {
        std::string table_address{std::string{sys_contract_sharding_vote_addr} + "@" + std::to_string(table)};
        std::map<std::string, uint64_t> adv_get_votes_detail;
        for (auto i = 1; i <= xstake::XPROPERTY_SPLITED_NUM; i++) {
            std::string property;
            property = property + xstake::XPORPERTY_CONTRACT_VOTES_KEY_BASE + "-" + std::to_string(i);
            {
                std::map<std::string, std::map<std::string, uint64_t>> votes_detail;
                for (uint32_t j = 0; j < old_tables_count; j++) {
                    auto table_addr = std::string{sys_contract_sharding_vote_addr} + "@" + base::xstring_utl::tostring(j);
                    std::vector<std::pair<std::string, std::string>> db_kv_112;
                    chain_data::xchain_data_processor_t::get_stake_map_property(common::xlegacy_account_address_t{table_addr}, property, db_kv_112);
                    for (auto const & _p : db_kv_112) {
                        base::xvaccount_t vaccount{_p.first};
                        auto account_table_id = vaccount.get_ledger_subaddr();
                        if (static_cast<uint16_t>(account_table_id) != static_cast<uint16_t>(table)) {
                            continue;
                        }
                        std::map<std::string, uint64_t> votes;
                        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)_p.second.c_str(), (uint32_t)_p.second.size());
                        stream >> votes;
                        for (auto const & vote : votes) {
                            if (votes_detail[_p.first].count(vote.first)) {
                                votes_detail[_p.first][vote.first] += vote.second;
                            } else {
                                votes_detail[_p.first][vote.first] = vote.second;
                            }
                        }
                    }
                }
                for (auto const & vote_detail : votes_detail) {
                    for (auto const & adv_get_votes : vote_detail.second) {
                        if (adv_get_votes_detail.count(adv_get_votes.first)) {
                            adv_get_votes_detail[adv_get_votes.first] += adv_get_votes.second;
                        } else {
                            adv_get_votes_detail[adv_get_votes.first] = adv_get_votes.second;
                        }
                    }
                }
            }
        }
        {
            if(adv_get_votes_detail.empty()) {
                continue;
            }
            std::map<std::string, std::string> adv_get_votes_str_detail;
            for (auto const & adv_get_votes : adv_get_votes_detail) {
                adv_get_votes_str_detail.insert(std::make_pair(adv_get_votes.first, base::xstring_utl::tostring(adv_get_votes.second)));
            }
            xstream_t stream(xcontext_t::instance());
            stream << adv_get_votes_str_detail;
            std::string adv_get_votes_str{std::string((const char*)stream.data(), stream.size())};
            m_ticket_prop.set(table_address, adv_get_votes_str);
        }
    }

    /*{
        // test
        std::map<std::string, std::string> contract_auditor_votes;
        auto table_address = "T20000MVfDLsBKVcy1wMp4CoEHWxUeBEAVBL9ZEa@0";

        uint64_t votes = 1000000;
        for (auto i = 0; i < 700; i++) {
            std::stringstream ss;
            ss << std::setw(40) << std::setfill('0') << i;
            auto node_account = ss.str();

            contract_auditor_votes[node_account] = base::xstring_utl::tostring(votes);
        }

        xstream_t stream(xcontext_t::instance());
        stream << contract_auditor_votes;
        std::string contract_adv_votes_str = std::string((const char*)stream.data(), stream.size());
        MAP_SET(XPORPERTY_CONTRACT_TICKETS_KEY, table_address, contract_adv_votes_str);
    }*/

    auto const& source_address = sender();
    m_vote_report_time_prop.set(source_address.value(), base::xstring_utl::tostring(0));
}

int xzec_vote_contract_new::is_mainnet_activated() {
    xstake::xactivation_record record;
    auto const genesis_stage = get_property<contract_common::properties::xstring_property_t>(
        state_accessor::properties::xtypeless_property_identifier_t{xstake::XPORPERTY_CONTRACT_REG_KEY}, common::xaccount_address_t{sys_contract_rec_registration_addr});
    std::string value_str = genesis_stage.value();
    if (!value_str.empty()) {
        base::xstream_t stream(base::xcontext_t::instance(),
                    (uint8_t*)value_str.c_str(), (uint32_t)value_str.size());
        record.serialize_from(stream);
    }
    xdbg("[xzec_vote_contract_new_t::is_mainnet_activated] activated: %d, pid:%d\n", record.activated, getpid());
    return record.activated;
};

bool xzec_vote_contract_new::handle_receive_shard_votes(uint64_t report_time, uint64_t last_report_time, std::map<std::string, std::string> const & contract_adv_votes, std::map<std::string, std::string> & merge_contract_adv_votes) {
    xdbg("[xzec_vote_contract_new_t::handle_receive_shard_votes] report vote table size: %d, original vote table size: %d",
            contract_adv_votes.size(), merge_contract_adv_votes.size());
    if (report_time < last_report_time) {
        return false;
    }
    if (report_time == last_report_time) {
        merge_contract_adv_votes.insert(contract_adv_votes.begin(), contract_adv_votes.end());
        xdbg("[xzec_vote_contract_new_t::handle_receive_shard_votes] same batch of vote report, report vote table size: %d, total size: %d",
            contract_adv_votes.size(), merge_contract_adv_votes.size());
    } else {
        merge_contract_adv_votes = contract_adv_votes;
    }
    return true;
}

void xzec_vote_contract_new::on_receive_shard_votes_v2(uint64_t report_time, std::map<std::string, std::string> const & contract_adv_votes) {
    XMETRICS_COUNTER_INCREMENT(XVOTE_CONTRACT "on_receive_shard_votes_Called", 1);
    XMETRICS_TIME_RECORD(XVOTE_CONTRACT "on_receive_shard_votes_ExecutionTime");
    auto const& source_account = sender();
    xdbg("[xzec_vote_contract_new_t::on_receive_shard_votes_v2] contract addr: %s, contract_adv_votes size: %d, report_time: %llu, pid:%d\n",
        source_account.c_str(), contract_adv_votes.size(), report_time, getpid());

    std::string base_addr;
    uint32_t    table_id;
    if (!data::xdatautil::extract_parts(source_account.value(), base_addr, table_id) || sys_contract_sharding_vote_addr != base_addr) {
        xwarn("[xzec_vote_contract_new_t::on_receive_shard_votes_v2] invalid call from %s", source_account.c_str());
        XCONTRACT_ENSURE(false, "[xzec_vote_contract_new_t::on_receive_shard_votes_v2] invalid call");
    }

    if ( !is_mainnet_activated() ) return;

    // if (!MAP_PROPERTY_EXIST(XPORPERTY_CONTRACT_VOTE_REPORT_TIME_KEY)) {
    //     MAP_CREATE(XPORPERTY_CONTRACT_VOTE_REPORT_TIME_KEY);
    //     MAP_SET(XPORPERTY_CONTRACT_VOTE_REPORT_TIME_KEY, source_address, base::xstring_utl::tostring(0));
    // }
    bool replace = true;
    std::string value_str = m_vote_report_time_prop.get(source_account.value());
    uint64_t last_report_time = 0;
    if (!value_str.empty()) {
        last_report_time = base::xstring_utl::touint64(value_str);
        xdbg("[xzec_vote_contract_new_t::on_receive_shard_votes_v2] last_report_time: %llu", last_report_time);
    }
    m_vote_report_time_prop.set(source_account.value(), base::xstring_utl::tostring(report_time));

    {
        XMETRICS_TIME_RECORD(XVOTE_CONTRACT "XPORPERTY_CONTRACT_TICKETS_KEY_SetExecutionTime");
        std::map<std::string, std::string> auditor_votes;
        std::string auditor_votes_str = m_ticket_prop.get(source_account.value());
        if (!auditor_votes_str.empty()) {
            base::xstream_t votes_stream(base::xcontext_t::instance(), (uint8_t *)auditor_votes_str.c_str(), (uint32_t)auditor_votes_str.size());
            votes_stream >> auditor_votes;
        }
        if ( !handle_receive_shard_votes(report_time, last_report_time, contract_adv_votes, auditor_votes) ) {
            XCONTRACT_ENSURE(false, "[xzec_vote_contract_new_t::on_receive_shard_votes_v2] handle_receive_shard_votes fail");
        }

        xstream_t stream(xcontext_t::instance());
        stream << auditor_votes;

        std::string contract_adv_votes_str = std::string((const char*)stream.data(), stream.size());
        m_ticket_prop.set(source_account.value(), contract_adv_votes_str);
    }

    XMETRICS_COUNTER_INCREMENT(XVOTE_CONTRACT "on_receive_shard_votes_Executed", 1);
}

NS_END2

#undef XVOTE_CONTRACT
#undef XCONTRACT_PREFIX
#undef XZEC_MODULE

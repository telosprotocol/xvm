// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xreward/xtable_workload_contract.h"

#include "xbase/xmem.h"
#include "xbase/xutl.h"
#include "xbasic/xutility.h"
#include "xchain_upgrade/xchain_upgrade_center.h"
#include "xcommon/xrole_type.h"
#include "xdata/xgenesis_data.h"
#include "xdata/xproperty.h"
#include "xdata/xworkload_info.h"
#include "xelect_common/elect_option.h"
#include "xmetrics/xmetrics.h"
#include "xstore/xstore_error.h"


using top::base::xcontext_t;
using top::base::xstream_t;

using namespace top::data;  // NOLINT

NS_BEG2(top, xstake)

xtable_workload_contract::xtable_workload_contract(common::xnetwork_id_t const & network_id) : xbase_t{network_id} {}

void xtable_workload_contract::setup() {
    STRING_CREATE(XPORPERTY_CONTRACT_TABLEBLOCK_HEIGHT_KEY);

    xdbg("[xtable_workload_contract::setup] pid:%d\n", getpid());
}

void xtable_workload_contract::on_timer(const uint64_t onchain_timer_round) {
    XMETRICS_TIME_RECORD("sysContract_tableWorkload_on_timer");
    // xinfo("[xtable_workload_contract::on_timer] pid: %d, SELF_ADDRESS: %s, onchain_timer_round: %llu, this: %p\n", getpid(), SELF_ADDRESS().c_str(), onchain_timer_round, this);

    auto const & account = SELF_ADDRESS();
    std::string source_address = SOURCE_ADDRESS();
    if (account.value() != source_address) {
        xwarn("[xtable_workload_contract::on_timer] invalid call from %s", source_address.c_str());
        return;
    }

    // xdbg("[xtable_workload_contract::on_timer] pid: %d, SELF_ADDRESS: %s, this: %p\n", getpid(), account.c_str(), this);
    uint32_t table_id = 0;
    if (!EXTRACT_TABLE_ID(account, table_id)) {
        xdbg("[xtable_workload_contract::on_timer] pid: %d, table_id: %u\n", getpid(), table_id);
        return;
    }

    std::map<common::xcluster_address_t, xauditor_workload_info_t> bookload_auditor_group_workload_info;
    std::map<common::xcluster_address_t, xvalidator_workload_info_t> bookload_validator_group_workload_info;
    int64_t table_pledge_balance_change_tgas = 0;

    auto workload_report_min_table_block_num = XGET_ONCHAIN_GOVERNANCE_PARAMETER(workload_report_min_table_block_num);
    auto workload_per_tableblock = XGET_ONCHAIN_GOVERNANCE_PARAMETER(workload_per_tableblock);
    auto workload_per_tx = XGET_ONCHAIN_GOVERNANCE_PARAMETER(workload_per_tx);

    std::string table_owner = xdatautil::serialize_owner_str(sys_contract_sharding_table_block_addr, table_id);
    std::string value_str;
    uint64_t start_height = 1;
    uint32_t total_table_block_count = 0;
    uint32_t workload = 0;
    auto blockchain_height = get_blockchain_height(table_owner);

    {
        XMETRICS_TIME_RECORD("sysContract_tableWorkload_get_property_tableblk_height_time");
        value_str = STRING_GET(XPORPERTY_CONTRACT_TABLEBLOCK_HEIGHT_KEY);
    }
    if (!value_str.empty()) {
        start_height = base::xstring_utl::touint64(value_str);
    }

    for (uint64_t height = start_height; height <= blockchain_height; height++) {
        base::xauto_ptr<data::xblock_t> tableblock(get_block_by_height(table_owner, height));
        if (tableblock == nullptr) {
            // xdbg("[xtable_workload_contract::on_timer] tableblock no load info. table_owner: %s, height: %llu", table_owner.c_str(), height);
            break;
        }

        workload = workload_per_tableblock + tableblock->get_txs_count() * workload_per_tx;

        std::string leader;
        xvip2_t leader_xip;
        int32_t ret = 0;
        bool is_auditor = true;
        // leader is auditor
        leader_xip = tableblock->get_cert()->get_auditor();
        if (leader_xip.high_addr != 0 && leader_xip.low_addr != 0 && get_node_id_from_xip2(leader_xip) != 0x3FF) {
            xdbg("[xtable_workload_contract::on_timer] auditor xvip2: %llu, %llu, table_owner: %s, height: %llu",
                 leader_xip.high_addr,
                 leader_xip.low_addr,
                 table_owner.c_str(),
                 height);
        } else {
            is_auditor = false;
            leader_xip = tableblock->get_cert()->get_validator();
            xdbg("[xtable_workload_contract::on_timer] validator xvip2: %llu, %llu, table_owner: %s, height: %llu",
                 leader_xip.high_addr,
                 leader_xip.low_addr,
                 table_owner.c_str(),
                 height);
        }
        ret = get_account_from_xip(leader_xip, leader);
        XCONTRACT_ENSURE(ret == 0, "get_auditor failed");

        common::xcluster_address_t cluster{common::xip_t{leader_xip.low_addr}};

        if (is_auditor) {
            auto it2 = bookload_auditor_group_workload_info.find(cluster);
            if (it2 == bookload_auditor_group_workload_info.end()) {
                xauditor_workload_info_t auditor_workload_info;
                std::pair<std::map<common::xcluster_address_t, xauditor_workload_info_t>::iterator, bool> ret =
                    bookload_auditor_group_workload_info.insert(std::make_pair(cluster, auditor_workload_info));
                XCONTRACT_ENSURE(ret.second, "insert auditor workload failed");
                it2 = ret.first;
            }
            it2->second.m_leader_count[leader] += workload;
        } else {
            auto it2 = bookload_validator_group_workload_info.find(cluster);
            if (it2 == bookload_validator_group_workload_info.end()) {
                xvalidator_workload_info_t validator_workload_info;
                std::pair<std::map<common::xcluster_address_t, xvalidator_workload_info_t>::iterator, bool> ret =
                    bookload_validator_group_workload_info.insert(std::make_pair(cluster, validator_workload_info));
                XCONTRACT_ENSURE(ret.second, "insert auditor workload failed");
                it2 = ret.first;
            }
            it2->second.m_leader_count[leader] += workload;
        }

        // m_table_pledge_balance_change_tgas
        table_pledge_balance_change_tgas += tableblock->get_pledge_balance_change_tgas();
        total_table_block_count++;
        xdbg(
            "[xtable_workload_contract::on_timer] cluster: [%s, network_id: %u, zone_id: %u, cluster_id: %u, group_id: %u], leader: %s, is_auditor: %d, workload: %u, tx_count: "
            "%u, total_table_block_count: %u",
            cluster.to_string().c_str(),
            cluster.network_id().value(),
            cluster.zone_id().value(),
            cluster.cluster_id().value(),
            cluster.group_id().value(),
            leader.c_str(),
            is_auditor,
            workload,
            tableblock->get_txs_count(),
            total_table_block_count);
    }
    // xinfo("[xtable_workload_contract::on_timer -----] pid: %d, SELF_ADDRESS: %s, this: %p, total_table_block_count: %u, workload_report_min_table_block_num: %u, cur_time: %llu,
    // start_height: %llu\n",
    //    getpid(), account.c_str(), this, total_table_block_count, workload_report_min_table_block_num, onchain_timer_round, start_height);
    if (total_table_block_count >  workload_report_min_table_block_num) {
        xinfo(
            "[xtable_workload_contract::on_timer] timer round: %" PRIu64 ", pid: %d, SELF_ADDRESS: %s, total_table_block_count: %d, start_height: %llu, table_pledge_balance_change_tgas: %lld, "
            "workload_report_min_table_block_num: %u, this: %p, workload_per_tableblock: %u, workload_per_tx: %u\n",
            TIME(),
            getpid(),
            account.c_str(),
            total_table_block_count,
            start_height,
            table_pledge_balance_change_tgas,
            workload_report_min_table_block_num,
            this,
            workload_per_tableblock,
            workload_per_tx);
        XMETRICS_PACKET_INFO("sysContract_tableWorkload", "effective report timer round", std::to_string(TIME()));

        start_height += total_table_block_count;
        {
            XMETRICS_TIME_RECORD("sysContract_tableWorkload_set_property_tableblk_height_time");
            STRING_SET(XPORPERTY_CONTRACT_TABLEBLOCK_HEIGHT_KEY, base::xstring_utl::tostring(start_height));
        }

        for (auto & entity : bookload_auditor_group_workload_info) {
            auto const & group = entity.first;
            for (auto const & wl : entity.second.m_leader_count) {
                xdbg("[xtable_workload_contract::on_timer] workload auditor group: %s, leader: %s, workload: %u", group.to_string().c_str(), wl.first.c_str(), wl.second);
            }
            xdbg("[xtable_workload_contract::on_timer] workload auditor group: %s, group id: %u, ends", group.to_string().c_str(), group.group_id().value());
        }

        for (auto & entity : bookload_validator_group_workload_info) {
            auto const & group = entity.first;
            for (auto const & wl : entity.second.m_leader_count) {
                xdbg("[xtable_workload_contract::on_timer] workload validator group: %s, leader: %s, workload: %u", group.to_string().c_str(), wl.first.c_str(), wl.second);
            }
            xdbg("[xtable_workload_contract::on_timer] workload validator group: %s, group id: %u, ends", group.to_string().c_str(), group.group_id().value());
        }

        xstream_t stream(xcontext_t::instance());
        MAP_OBJECT_SERIALIZE2(stream, bookload_auditor_group_workload_info);
        MAP_OBJECT_SERIALIZE2(stream, bookload_validator_group_workload_info);
        stream << table_pledge_balance_change_tgas;

        std::string workload_str = std::string((char *)stream.data(), stream.size());
        {
            xstream_t stream(xcontext_t::instance());
            stream << workload_str;
            CALL(common::xaccount_address_t{sys_contract_zec_workload_addr}, "on_receive_workload2", std::string((char *)stream.data(), stream.size()));
        }

        return;
    }
}

NS_END2

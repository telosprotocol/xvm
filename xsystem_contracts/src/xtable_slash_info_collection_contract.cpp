// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xslash/xtable_slash_info_collection_contract.h"

#include "xbase/xmem.h"
#include "xcertauth/xcertauth_face.h"
#include "xchain_upgrade/xchain_upgrade_center.h"
#include "xcommon/xip.h"
#include "xdata/xdata_common.h"
#include "xdata/xblock_statistics_data.h"
#include "xdata/xnative_contract_address.h"
#include "xmetrics/xmetrics.h"
#include "xstake/xstake_algorithm.h"
#include "xvm/manager/xcontract_manager.h"

using namespace top::data;  // NOLINE

NS_BEG3(top, xvm, xcontract)

xtable_slash_info_collection_contract::xtable_slash_info_collection_contract(common::xnetwork_id_t const & network_id) : xbase_t{network_id} {}

void xtable_slash_info_collection_contract::setup() {
    STRING_CREATE(xstake::XPORPERTY_CONTRACT_TABLEBLOCK_HEIGHT_KEY);
}

void xtable_slash_info_collection_contract::on_collect_slash_info(common::xlogic_time_t const timestamp) {
    XMETRICS_TIME_RECORD("sysContract_tableSlash_on_collect_slash_info");

    auto const & source_addr = SOURCE_ADDRESS();
    auto const & account = SELF_ADDRESS();

    std::string base_addr = "";
    uint32_t table_id = 0;

    XCONTRACT_ENSURE(data::xdatautil::extract_parts(source_addr, base_addr, table_id), "source address extract base_addr or table_id error!");
    xdbg("[xtable_slash_info_collection_contract][on_collect_slash_info] self_account %s, source_addr %s, base_addr %s\n", account.c_str(), source_addr.c_str(), base_addr.c_str());
    XCONTRACT_ENSURE(source_addr == account.value() && base_addr == top::sys_contract_sharding_slash_info_addr, "invalid source addr's call!");

    xinfo("[xtable_slash_info_collection_contract][on_collect_slash_info] timer round: %" PRIu64 ", contract addr: %s, table_id: %u, pid: %d",
         timestamp,
         source_addr.c_str(),
         table_id,
         getpid());

    // the node block generating info
    xunqualified_node_info_t node_info;

    std::string table_owner = xdatautil::serialize_owner_str(sys_contract_sharding_table_block_addr, table_id);
    auto blockchain_height = get_blockchain_height(table_owner);
    uint64_t cur_start_height = 0;
    std::string value_str;

    try {
        XMETRICS_TIME_RECORD("sysContract_tableSlash_get_property_tableblk_height_time");
        value_str = STRING_GET(xstake::XPORPERTY_CONTRACT_TABLEBLOCK_HEIGHT_KEY);
    } catch (std::runtime_error & e) {
        xwarn("[xtable_slash_info_collection_contract][on_collect_slash_info] read table height error:%s", e.what());
        throw;
    }

    if (!value_str.empty()) {
        cur_start_height = base::xstring_utl::touint64(value_str);
    }

    xdbg("[xtable_slash_info_collection_contract][on_collect_slash_info] timer round: %" PRIu64 ", contract addr: %s, pid: %d, cur_start_height: %" PRIu64
         ", blockchain_height % " PRIu64,
         timestamp,
         account.c_str(),
         getpid(),
         cur_start_height,
         blockchain_height);

    uint64_t BLOCKNUM = XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_table_block_report);
    xdbg("[xtable_slash_info_collection_contract][on_collect_slash_info] slash needed collect blocknum: %u, timer round %" PRIu64, BLOCKNUM, timestamp);
    assert(BLOCKNUM > 0);
    if (cur_start_height + BLOCKNUM < blockchain_height) {

        auto const fork_config = chain_upgrade::xchain_fork_config_center_t::chain_fork_config();
        if (chain_upgrade::xtop_chain_fork_config_center::is_forked(fork_config.slash_workload_contract_upgrade, timestamp)) {
            auto const full_blocks = get_next_fulltableblock(common::xaccount_address_t{table_owner}, blockchain_height, cur_start_height);
            node_info = process_fulltableblock(full_blocks);


        } else {
            // handle blocks
            for (auto i = cur_start_height + 1; i <= blockchain_height; ++i) {
                // block consensus state
                base::xauto_ptr<data::xblock_t> tableblock(get_block_by_height(table_owner, i));
                if (nullptr == tableblock) {
                    xwarn("[xtable_slash_info_collection_contract][on_collect_slash_info] get tableblock error, height: %" PRIu64, i);
                    break;
                }

                // do auditor & validator check
                try {
    #ifdef DEBUG
                    std::string auditor_debug_str{""};
                    std::string validator_debug_str{""};
    #endif

                    std::vector<base::xvoter> auditor_info;
                    std::vector<base::xvoter> validator_info;
                    auto auditor_xip = tableblock->get_cert()->get_auditor();
                    auto validator_xip = tableblock->get_cert()->get_validator();

                    xdbg("[xtable_slash_info_collection_contract][on_collect_slash_info] auditor xip(%" PRIu64 " : %" PRIu64 "), validator xip (%" PRIu64 " : %" PRIu64 ")",
                        auditor_xip.high_addr,
                        auditor_xip.low_addr,
                        validator_xip.high_addr,
                        validator_xip.low_addr);

                    if (is_xip2_empty(auditor_xip)) {
                        xwarn("[xtable_slash_info_collection_contract][on_collect_slash_info] get block auditor info zero, table owner: %s, height: %" PRIu64, table_owner.c_str(), i);
                    } else {
                        if (get_node_id_from_xip2(auditor_xip)) {
                            xdbg("[xtable_slash_info_collection_contract][on_collect_slash_info] get block auditor info is leader, table owner: %s, height: %" PRIu64,
                                table_owner.c_str(),
                                i);
                        } else {
                            xdbg("[xtable_slash_info_collection_contract][on_collect_slash_info] get block auditor info group xip, table owner: %s, height: %" PRIu64,
                                table_owner.c_str(),
                                i);
                        }

                        auditor_info = auth::xauthcontext_t::query_auditors(*tableblock.get(), *contract::xcontract_manager_t::instance().get_node_service());
                        // xassert(!auditor_info.empty());

                        // handle auditor
                        for (auto const & item : auditor_info) {
                            node_info.auditor_info[common::xnode_id_t{item.account}].subset_count++;
                            if (item.is_voted) {
                                node_info.auditor_info[common::xnode_id_t{item.account}].block_count++;
                            }

    #ifdef DEBUG
                            auditor_debug_str += item.account + ":" + std::to_string(item.is_voted) + "||";
    #endif
                        }
                    }

                    if (is_xip2_empty(validator_xip)) {
                        xwarn(
                            "[xtable_slash_info_collection_contract][on_collect_slash_info] get block validator info zero, table owner: %s, height: %" PRIu64, table_owner.c_str(), i);
                    } else {
                        if (get_node_id_from_xip2(validator_xip)) {
                            xdbg("[xtable_slash_info_collection_contract][on_collect_slash_info] get block validator info is leader, table owner: %s, height: %" PRIu64,
                                table_owner.c_str(),
                                i);
                        } else {
                            xdbg("[xtable_slash_info_collection_contract][on_collect_slash_info] get block validator info group xip, table owner: %s, height: %" PRIu64,
                                table_owner.c_str(),
                                i);
                        }

                        validator_info = auth::xauthcontext_t::query_validators(*tableblock.get(), *contract::xcontract_manager_t::instance().get_node_service());
                        // xassert(!validator_info.empty());

                        for (auto const & item : validator_info) {
                            node_info.validator_info[common::xnode_id_t{item.account}].subset_count++;
                            if (item.is_voted) {
                                node_info.validator_info[common::xnode_id_t{item.account}].block_count++;
                            }

    #ifdef DEBUG
                            validator_debug_str += item.account + ":" + std::to_string(item.is_voted) + "||";
    #endif
                        }
                    }

    #ifdef DEBUG
                    xdbg("[xtable_slash_info_collection_contract][on_collect_slash_info][validator] timer round: %" PRIu64
                        ", contract addr: %s, pid: %d, cur_tableblock_height: %" PRIu64 ", validator_consensus_size: %zu, validator_info_string: %s",
                        timestamp,
                        account.c_str(),
                        getpid(),
                        i,
                        validator_info.size(),
                        validator_debug_str.c_str());
                    xdbg("[xtable_slash_info_collection_contract][on_collect_slash_info][auditor] timer round: %" PRIu64 ", contract addr: %s, pid: %d, cur_tableblock_height: %" PRIu64
                        ", auditor_consensus_size: %zu, auditor_info_string: %s",
                        timestamp,
                        account.c_str(),
                        getpid(),
                        i,
                        auditor_info.size(),
                        auditor_debug_str.c_str());
    #endif

                } catch (std::exception & e) {
                    xdbg("[xtable_slash_info_collection_contract][on_collect_slash_info], exception %s", e.what());
                    throw;
                }
            }

         }


        {
            XMETRICS_TIME_RECORD("sysContract_tableSlash_set_property_tableblk_height_time");
            STRING_SET(xstake::XPORPERTY_CONTRACT_TABLEBLOCK_HEIGHT_KEY, base::xstring_utl::tostring(blockchain_height));
        }

        base::xstream_t stream(base::xcontext_t::instance());
        node_info.serialize_to(stream);
        uint32_t tableblock_num = (uint32_t)(blockchain_height - cur_start_height);
        stream << tableblock_num;

        xkinfo("[xtable_slash_info_collection_contract][on_collect_slash_info] effective summarize_slash_info, timer round %" PRIu64
             ", tableblock num: %u, contract addr: %s, pid: %d",
             timestamp,
             tableblock_num,
             account.c_str(),
             getpid());
        XMETRICS_PACKET_INFO("sysContract_tableSlash", "effective report timer round", std::to_string(TIME()));

        std::string shard_slash_collect = std::string((char *)stream.data(), stream.size());
        {
            stream.reset();
            stream << shard_slash_collect;
            CALL(common::xaccount_address_t{sys_contract_zec_slash_info_addr}, "summarize_slash_info", std::string((char *)stream.data(), stream.size()));
        }
    }
}

std::vector<base::xauto_ptr<data::xblock_t>> xtable_slash_info_collection_contract::get_next_fulltableblock(common::xaccount_address_t const& owner, uint64_t blockchain_height, uint64_t last_read_height) const {
    std::vector<base::xauto_ptr<data::xblock_t>> res;

    for (auto i = last_read_height + 1; i <= blockchain_height; ++i) {
        base::xauto_ptr<data::xblock_t> tableblock = get_block_by_height(owner.value(), i);
        if (tableblock == nullptr) {
            xwarn("[xtable_slash_info_collection_contract][get_next_fulltableblock] invalid tableblock owner: %s, height: %" PRIu64, owner.value().c_str(), last_read_height);
            throw xvm::xvm_error { xvm::enum_xvm_error_code::enum_vm_exception, "[xtable_slash_info_collection_contract][get_next_fulltableblock] invalid tableblock"};
        }

        if (tableblock->is_fulltable()) {
            xdbg("[xtable_slash_info_collection_contract][get_next_fulltableblock] tableblock owner: %s, height: %" PRIu64, owner.value().c_str(), i);
            res.push_back(std::move(tableblock));
        }
    }

    return res;
}

xunqualified_node_info_t xtable_slash_info_collection_contract::process_fulltableblock(std::vector<base::xauto_ptr<xblock_t>> const& full_blocks) {
    xunqualified_node_info_t res_node_info;

    for (std::size_t block_index = 0; block_index < full_blocks.size(); ++block_index) {

        xfull_tableblock_t* full_tableblock = dynamic_cast<xfull_tableblock_t*>(full_blocks[block_index].get());
        auto fulltable_statisitc_data = full_tableblock->get_fulltable_statistics_resource()->get_statistics_data();
        base::xstream_t stream(base::xcontext_t::instance(), (uint8_t *)fulltable_statisitc_data.data(), fulltable_statisitc_data.size());
        data::xstatistics_data_t stat_data;
        stream >> stat_data;

        // process one full tableblock statistic data
        auto node_service = contract::xcontract_manager_t::instance().get_node_service();
        for (auto const static_item: stat_data.detail) {
            auto elect_statistic = static_item.second;
            for (auto const& group_item: elect_statistic.group_statistics_data) {
                common::xgroup_address_t const& group_addr = group_item.first;
                xvip2_t const& group_xvip2 = top::common::xip2_t{group_addr.network_id(), group_addr.zone_id(), group_addr.cluster_id(), group_addr.group_id()};
                xgroup_related_statistics_data_t const& group_account_data = group_item.second;

                // process auditor group
                if (top::common::has<top::common::xnode_type_t::auditor>(group_addr.type())) {
                    for (std::size_t slotid = 0; slotid < group_account_data.account_statistics_data.size(); ++slotid) {
                        auto account_addr = node_service->get_group(group_xvip2)->get_node(slotid)->get_account();
                        res_node_info.auditor_info[common::xnode_id_t{account_addr}].subset_count += group_account_data.account_statistics_data[slotid].block_data.block_count;
                        res_node_info.auditor_info[common::xnode_id_t{account_addr}].block_count += group_account_data.account_statistics_data[slotid].vote_data.vote_count;
                        xdbg("[xtable_slash_info_collection_contract][process_fulltableblock] incremental auditor data: {gourp id: %d, account addr: %s, slot id: %u, subset count: %u, block_count: %u}", group_addr.group_id().value(), account_addr.c_str(),
                            slotid, group_account_data.account_statistics_data[slotid].block_data.block_count, group_account_data.account_statistics_data[slotid].vote_data.vote_count);
                    }
                } else if (top::common::has<top::common::xnode_type_t::validator>(group_addr.type())) {// process validator group
                    for (std::size_t slotid = 0; slotid < group_account_data.account_statistics_data.size(); ++slotid) {
                        auto account_addr = node_service->get_group(group_xvip2)->get_node(slotid)->get_account();
                        res_node_info.validator_info[common::xnode_id_t{account_addr}].subset_count += group_account_data.account_statistics_data[slotid].block_data.block_count;
                        res_node_info.validator_info[common::xnode_id_t{account_addr}].block_count += group_account_data.account_statistics_data[slotid].vote_data.vote_count;
                        xdbg("[xtable_slash_info_collection_contract][process_fulltableblock] incremental validator data: {gourp id: %d, account addr: %s, slot id: %u, subset count: %u, block_count: %u}", group_addr.group_id().value(), account_addr.c_str(),
                            slotid, group_account_data.account_statistics_data[slotid].block_data.block_count, group_account_data.account_statistics_data[slotid].vote_data.vote_count);
                    }

                } else { // invalid group
                    xwarn("[xtable_slash_info_collection_contract][process_fulltableblock] invalid group id: %d", group_addr.group_id().value());
                    throw xvm::xvm_error { xvm::enum_xvm_error_code::enum_vm_exception, "[xtable_slash_info_collection_contract][process_fulltableblock] invalid group"};
                }
            }
        }
    }

    return res_node_info;
}


NS_END3

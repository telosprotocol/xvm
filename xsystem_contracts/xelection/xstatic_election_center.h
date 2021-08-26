// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// only used in static_consensus election
#pragma once
#include "xconfig/xconfig_register.h"
#include "xconfig/xpredefined_configurations.h"
#include "xdata/xcodec/xmsgpack/xelection_result_store_codec.hpp"
#include "xdata/xcodec/xmsgpack/xstandby_node_info_codec.hpp"
#include "xdata/xcodec/xmsgpack/xstandby_result_store_codec.hpp"
#include "xdata/xelection/xelection_result_property.h"
#include "xdata/xgenesis_data.h"

#include <atomic>
#include <csignal>
#include <iostream>

NS_BEG3(top, xvm, system_contracts)

void set_signal_true(int signal);

struct node_info {
    common::xnode_id_t node_id;
    uint64_t stake;
    top::xpublic_key_t pub_key;
    node_info(std::string _node_id, uint64_t _stake, std::string _pub_key) : node_id{_node_id}, stake{_stake}, pub_key{_pub_key} {
    }
};

struct standby_node_info{
    common::xnode_id_t node_id;
    top::xpublic_key_t pub_key;
    std::vector<std::pair<common::xnode_type_t,uint64_t>> type_stake_pair;

    standby_node_info(std::string _node_id, std::string _pub_key, std::vector<std::pair<common::xnode_type_t,uint64_t>> _type_stake_pair):node_id{_node_id},pub_key{_pub_key},type_stake_pair{_type_stake_pair}{}
};

class xstatic_election_center {
public:
    xstatic_election_center(xstatic_election_center const &) = delete;
    xstatic_election_center & operator=(xstatic_election_center const &) = delete;
    xstatic_election_center(xstatic_election_center &&) = delete;
    xstatic_election_center & operator=(xstatic_election_center &&) = delete;
    static xstatic_election_center & instance() {
        static xstatic_election_center instance;
        return instance;
    }

    // only used by set_signal_true trigged by signal.
    void allow_elect() {
        allow_election.store(true);
    }

    bool if_allow_elect() {
        return allow_election.load();
    }

    std::map<std::string,common::xnode_type_t> node_type_dict = {
       {"rec",common::xnode_type_t::rec}, 
       {"zec",common::xnode_type_t::zec}, 
       {"adv",common::xnode_type_t::consensus_auditor}, 
       {"con",common::xnode_type_t::consensus_validator},
       {"edge",common::xnode_type_t::edge},
       {"archive",common::xnode_type_t::storage_archive},
    };

    std::vector<standby_node_info> get_standby_config() {
        std::vector<standby_node_info> res;
        auto & config_register = top::config::xconfig_register_t::get_instance();
        std::string key = "standby_start_nodes";
        std::string config_infos;
        config_register.get(key, config_infos);
        xinfo("[xstatic_election_center][get_standby_config] read_key:%s content:%s", key.c_str(), config_infos.c_str());
        std::vector<std::string> node_standby_infos;
        top::base::xstring_utl::split_string(config_infos, ',', node_standby_infos);
        // node_id:pub_key:type.stake|type.stake,node_id:pub_key:type.stake|type.stake,...
        for (auto const & nodes : node_standby_infos) {
            std::vector<std::string> one_node_info;
            top::base::xstring_utl::split_string(nodes, ':', one_node_info);
            std::string node_id_str = one_node_info[0];
            std::string node_pub_key_str = one_node_info[1];
            std::vector<std::string> type_stake_pairs;
            top::base::xstring_utl::split_string(one_node_info[2], '|', type_stake_pairs);
            std::vector<std::pair<common::xnode_type_t, uint64_t>> _type_stake_pair;
            for (auto const & each_pair : type_stake_pairs) {
                std::vector<std::string> type_stake;
                top::base::xstring_utl::split_string(each_pair, '.', type_stake);
                common::xnode_type_t node_type = node_type_dict.at(type_stake[0]);
                uint64_t stake = static_cast<std::uint64_t>(std::atoi(type_stake[1].c_str()));
                _type_stake_pair.push_back(std::make_pair(node_type, stake));
            }
            res.push_back(standby_node_info{node_id_str,node_pub_key_str, _type_stake_pair});
        }
        return res;
    }

    std::vector<node_info> get_static_election_nodes(std::string const & key) {
        assert(key == "zec_start_nodes" || key == "archive_start_nodes" || key == "edge_start_nodes" );
        std::vector<node_info> res;

        auto & config_register = top::config::xconfig_register_t::get_instance();
        std::string config_infos;

        config_register.get(key, config_infos);
        xinfo("[xstatic_election_center][get_static_election_nodes] read_key:%s content:%s", key.c_str(), config_infos.c_str());
        std::vector<std::string> nodes_info;
        top::base::xstring_utl::split_string(config_infos, ',', nodes_info);
        for (auto nodes : nodes_info) {
            std::vector<std::string> one_node_info;
            top::base::xstring_utl::split_string(nodes, '.', one_node_info);
            res.push_back(node_info{one_node_info[0], static_cast<std::uint64_t>(std::atoi(one_node_info[1].c_str())), one_node_info[2]});
        }
        return res;
    }

private:
    xstatic_election_center() {
        xinfo("[static_consensus][xstatic_election_center] init allow election false");
        std::signal(SIGUSR1, set_signal_true);
    }
    std::atomic<bool> allow_election{false};
};

NS_END3
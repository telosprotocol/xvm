// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xserialization/xserialization.h"

NS_BEG3(top, xvm, serialization)


std::string sys_contract_addr_to_string(common::xaccount_address_t const & sys_contract_addr){
    if(sys_contract_addr == common::xaccount_address_t{sys_contract_rec_elect_rec_addr}){
        return "election_rec_elect_rec";
    } else if (sys_contract_addr == common::xaccount_address_t{sys_contract_rec_elect_archive_addr}) {
        return "election_rec_elect_archive";
    } else if (sys_contract_addr == common::xaccount_address_t{sys_contract_rec_elect_edge_addr}) {
        return "election_rec_elect_edge";
    } else if (sys_contract_addr == common::xaccount_address_t{sys_contract_rec_elect_zec_addr}) {
        return "election_rec_elect_zec";
    } else if (sys_contract_addr == common::xaccount_address_t{sys_contract_rec_standby_pool_addr}) {
        return "rec_standby_pool";
    } else if (sys_contract_addr == common::xaccount_address_t{sys_contract_rec_parachain_registration_addr}) {
        return "rec_parachain_registration";
    } else if (sys_contract_addr == common::xaccount_address_t{sys_contract_zec_elect_consensus_addr}) {
        return "election_zec_elect_consensus";
    } else if (sys_contract_addr == common::xaccount_address_t{sys_contract_zec_group_assoc_addr}) {
        return "election_zec_group_association";
    } else if (sys_contract_addr == common::xaccount_address_t{sys_contract_rec_registration_addr2}) {
        return "rec_registration";
    } else if (sys_contract_addr == common::xaccount_address_t{sys_contract_zec_registration_addr}) {
        return "zec_registration";
    } else if (sys_contract_addr == common::xaccount_address_t{sys_contract_rec_standby_pool_addr2}) {
        return "rec_standby";
    } else if (sys_contract_addr == common::xaccount_address_t{sys_contract_zec_standby_pool_addr2}) {
        return "zec_standby";
    } else {
        assert(false);
    }
}

NS_END3

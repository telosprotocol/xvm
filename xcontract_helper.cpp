// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xcontract_helper.h"
#include "xvm/xerror/xvm_error.h"
#include "xstore/xstore_error.h"
#include "xchain_upgrade/xchain_upgrade_center.h"

using namespace top::data;

NS_BEG2(top, xvm)
using store::xaccount_context_t;
using std::string;
using std::vector;
using store::xstore_success;
using store::xaccount_property_not_create;
using store::xaccount_property_map_field_not_create;

xcontract_helper::xcontract_helper(xaccount_context_t* account_context, common::xnode_id_t const & contract_account, const string& exec_account)
:m_account_context(account_context)
,m_contract_account(contract_account)
,m_exec_account(exec_account) {
}

void xcontract_helper::set_transaction(const xtransaction_ptr_t& ptr) {
    m_transaction = ptr;
}

xtransaction_ptr_t xcontract_helper::get_transaction() const {
    return m_transaction;
}

string xcontract_helper::get_source_account() const {
    return m_exec_account;
}

std::string xcontract_helper::get_parent_account() const {
    std::string parent{""};
    if (store::xaccount_property_parent_account_exist != m_account_context->get_parent_account(parent)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "get contract parent account error"};
    }

    return parent;
}

common::xnode_id_t const & xcontract_helper::get_self_account() const noexcept {
    return m_contract_account;
}

uint64_t xcontract_helper::get_balance() const {
    return m_account_context->get_blockchain()->balance();
}

common::xlogic_time_t xcontract_helper::get_timer_height() const {
    return m_account_context->get_timer_height();
}

const data::xaction_asset_out& xcontract_helper::get_pay_fee() const {
    return m_account_context->get_source_pay_info();
}

void xcontract_helper::set_contract_code(const string& code) {
    if (m_account_context->set_contract_code(code)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "set_contract_code error"};
    }
}

void xcontract_helper::get_contract_code(string &code)  const{
    if (m_account_context->get_contract_code(code)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "get_contract_code error"};
    }
}

void xcontract_helper::create_transfer_tx(const string& grant_account, const uint64_t amount) {
    m_account_context->create_transfer_tx(grant_account, amount);
}


void xcontract_helper::string_create(const string& key) {
    if (m_account_context->string_create(key)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "STRING_CREATE " + key + " error"};
    }
}
void xcontract_helper::string_set(const string& key, const string& value, bool native) {
    if (m_account_context->string_set(key, value, native)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "STRING_SET " + key + " error"};
    }
}
string xcontract_helper::string_get(const string& key, const std::string& addr) {
    string value;
    if (m_account_context->string_get(key, value, addr)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "STRING_GET " + key + " error"};
    }
    return value;
}

string xcontract_helper::string_get2(const string& key, const std::string& addr) {
    string value;
    m_account_context->string_get(key, value, addr);
    return value;
}

bool xcontract_helper::string_exist(const string& key, const std::string& addr) {
    string value;
    int32_t ret = m_account_context->string_get(key, value, addr);
    if (xaccount_property_not_create == ret) {
        return false;
    } else if (xstore_success == ret) {
        return true;
    } else {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "STRING_EXIST error" + std::to_string(ret)};
    }
    return true;
}

void xcontract_helper::list_create(const string& key) {
    if (m_account_context->list_create(key)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "LIST_CREATE " + key + " error"};
    }
}

void xcontract_helper::list_push_back(const string& key, const string& value, bool native) {
    if (m_account_context->list_push_back(key, value, native)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "LIST_PUSH_BACK  " + key + " error"};
    }
}

void xcontract_helper::list_push_front(const string& key, const string& value, bool native) {
    if (m_account_context->list_push_front(key, value, native)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "LIST_PUSH_FRONT " + key + " error"};
    }
}

void xcontract_helper::list_pop_back(const string& key, string& value, bool native) {
    if (m_account_context->list_pop_back(key, value, native)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "LIST_POP_BACK " + key + " error"};
    }
}

void xcontract_helper::list_pop_front(const string& key, string& value, bool native) {
    if (m_account_context->list_pop_front(key, value, native)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, key + " LIST_POP_FRONT " + key + " error"};
    }
}

void xcontract_helper::list_clear(const string& key, bool native) {
    if (m_account_context->list_clear(key, native)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, key + " LIST_CLEAR " + key + " error"};
    }
}

std::string xcontract_helper::list_get(const std::string& key, int32_t index, const std::string& addr) {
    std::string value{};
    if (m_account_context->list_get(key, index, value, addr)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "LIST_GET " + key + " error"};
    }
    return value;
}

int32_t xcontract_helper::list_size(const string& key, const std::string& addr) {
    int32_t size;
    if (m_account_context->list_size(key, size, addr)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "LIST_SIZE " + key + " error"};
    }
    return size;
}

vector<string> xcontract_helper::list_get_all(const string& key, const string& addr) {
    vector<string> value_list{};
    if (m_account_context->list_get_all(key, value_list, addr)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "LIST_GET_ALL " + key + " error"};
    }
    return std::move(value_list);
}

bool xcontract_helper::list_exist(const string& key) {
    vector<string> value_list{};
    int32_t ret = m_account_context->list_get_all(key, value_list);
    if (xaccount_property_not_create == ret) {
        return false;
    } else if (xstore_success == ret) {
        return true;
    } else {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "LIST_EXIST error" + std::to_string(ret)};
    }
    return true;
}

void xcontract_helper::map_create(const string& key) {
    if (m_account_context->map_create(key)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "MAP_CREATE " + key + " error"};
    }
}

string xcontract_helper::map_get(const string& key, const string& field, const std::string& addr) {
    string value{};
    if (m_account_context->map_get(key, field, value, addr)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "MAP_GET " + key + " error"};
    }
    return value;
}

int32_t xcontract_helper::map_get2(const string& key, const string& field, string& value, const std::string& addr) {
    return m_account_context->map_get(key, field, value, addr);
}

void xcontract_helper::map_set(const string& key, const string& field, const string & value, bool native) {
    if (m_account_context->map_set(key, field, value, native)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "MAP_SET " + key + " error"};
    }
}

void xcontract_helper::map_remove(const string& key, const string& field, bool native) {
    if (m_account_context->map_remove(key, field, native)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "MAP_REMOVE " + key + " error"};
    }
}

int32_t xcontract_helper::map_size(const string& key, const std::string& addr) {
    int32_t size{0};
    if (m_account_context->map_size(key, size, addr)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "MAP_SIZE " + key + " error"};
    }
    return size;
}

void xcontract_helper::map_copy_get(const std::string & key, std::map<std::string, std::string> & map, const std::string& addr) {
    if (m_account_context->map_copy_get(key, map, addr)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "MAP_COPY_GET " + key + " error"};
    }
}


bool xcontract_helper::map_field_exist(const string& key, const string& field) const {
    string value{};
    int32_t ret = m_account_context->map_get(key, field, value);
    if (xaccount_property_map_field_not_create == ret || xaccount_property_not_create == ret) {
        return false;
    } else if (xstore_success == ret) {
        return true;
    } else {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "MAP_FIELD_EXIST error:" + std::to_string(ret)};
    }
    return true;
}

bool xcontract_helper::map_key_exist(const std::string& key) {
    string field, value;
    int32_t ret = m_account_context->map_get(key, field, value);
    if (xaccount_property_not_create == ret) {
        return false;
    } else {
        return true;
    }
}

void xcontract_helper::map_clear(const std::string& key, bool native) {
    if (m_account_context->map_clear(key, native)) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "MAP_CLEAR " + key + " error"};
    }
}

void xcontract_helper::get_map_property(const std::string& key, std::map<std::string, std::string>& value, uint64_t height, const std::string& addr) {
    m_account_context->get_map_property(key, value, height, addr);
}

bool xcontract_helper::map_property_exist(const std::string& key) {
    return m_account_context->map_property_exist(key) == 0;
}

void xcontract_helper::generate_tx(common::xaccount_address_t const & target_addr, const string& func_name, const string& func_param) {
    if (m_contract_account == target_addr) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "can't send to self " + target_addr.value()};
    }
    int32_t ret = m_account_context->generate_tx(target_addr.value(), func_name, func_param);

    auto const & fork_config = chain_upgrade::xchain_fork_config_center_t::chain_fork_config();
    if (chain_upgrade::xtop_chain_fork_config_center::is_forked(fork_config.reward_fork_point, m_account_context->get_timer_height())) {
        if (ret) {
            throw xvm_error{enum_xvm_error_code::enum_vm_exception, "generate tx fail " + store::xstore_error_to_string(ret)};
        }
    }
}

std::string xcontract_helper::get_random_seed() const {
    auto random_seed = m_account_context->get_random_seed();
    if (random_seed.empty()) {
        throw xvm_error{enum_xvm_error_code::enum_vm_exception, "random_seed empty"};
    }
    return random_seed;
}

std::uint64_t
xcontract_helper::contract_height() const {
    return m_account_context->get_chain_height();
}

base::xauto_ptr<xblock_t>
xcontract_helper::get_block_by_height(const std::string & owner, uint64_t height) const {
    return base::xauto_ptr<xblock_t>(m_account_context->get_block_by_height(owner, height));
}

std::uint64_t  xcontract_helper::get_blockchain_height(const std::string & owner) const {
    return m_account_context->get_blockchain_height(owner);
}

int32_t xcontract_helper::get_gas_and_disk_usage(std::uint32_t &gas, std::uint32_t &disk) const {
    store::xtransaction_result_t result;
    m_account_context->get_transaction_result(result);

    for (auto const & tx : result.m_contract_txs) {
        uint32_t size = tx->get_transaction()->get_tx_len();
        gas += 2 * size;
        xdbg("[xcontract_helper::get_gas_and_disk_usage] size: %u, gas: %u, disk: %u\n", size, gas, disk);
    }
    return 0;
}

NS_END2

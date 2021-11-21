// Copyright (c) 2017-present Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xbase/xns_macro.h"

#include <cstdint>
#include <functional>
#include <system_error>
#include <type_traits>

NS_BEG3(top, system_contracts, error)

enum class xenum_errc {
    successful,
    serialization_error,
    deserialization_error,
    rec_registration_node_info_not_found,
    election_error,
    proposal_not_found,
    proposal_not_changed,
    unknown_proposal_type,
    invalid_proposer,
    proposal_already_voted,
};
using xerrc_t = xenum_errc;

std::error_code make_error_code(xerrc_t errc) noexcept;
std::error_condition make_error_condition(xerrc_t errc) noexcept;

std::error_category const & system_contract_category();

NS_END3

NS_BEG1(std)

#if !defined(XCXX14_OR_ABOVE)

template <>
struct hash<top::system_contracts::error::xerrc_t> final {
    size_t operator()(top::system_contracts::error::xerrc_t errc) const noexcept;
};

template <>
struct is_error_code_enum<top::system_contracts::error::xerrc_t> : std::true_type {};

template <>
struct is_error_condition_enum<top::system_contracts::error::xerrc_t> : std::true_type {};

#endif


NS_END1

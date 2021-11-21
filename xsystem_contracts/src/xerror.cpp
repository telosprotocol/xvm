// Copyright (c) 2017-present Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xvm/xsystem_contracts/xerror/xerror.h"

NS_BEG3(top, system_contracts, error)

static char const * errc_to_message(int const errc) noexcept {
    auto ec = static_cast<xerrc_t>(errc);
    switch (ec) {
    case xerrc_t::successful:
        return "successful";

    case xerrc_t::serialization_error:
        return "serialization error";

    case xerrc_t::deserialization_error:
        return "deserialization error";

    case xerrc_t::rec_registration_node_info_not_found:
        return "rec registraction constract: node info not found";

    case xerrc_t::election_error:
        return "election error";

    case xerrc_t::proposal_not_found:
        return "proposal not found";

    case xerrc_t::proposal_not_changed:
        return "proposal not changed";

    case xerrc_t::unknown_proposal_type:
        return "unknown proposal type";

    case xerrc_t::invalid_proposer:
        return "invalid proposer";

    case xerrc_t::proposal_already_voted:
        return "proposal alrady voted";

    default:
        return "unknown error";
    }
}

class xtop_system_contract_category : public std::error_category {
public:
    const char * name() const noexcept override {
        return "system_contract";
    }

    std::string message(int errc) const override {
        return errc_to_message(errc);
    }
};
using xsystem_contract_category_t = xtop_system_contract_category;

std::error_code make_error_code(xerrc_t errc) noexcept {
    return std::error_code(static_cast<int>(errc), system_contract_category());
}

std::error_condition make_error_condition(xerrc_t errc) noexcept {
    return std::error_condition(static_cast<int>(errc), system_contract_category());
}

std::error_category const & system_contract_category() {
    static xsystem_contract_category_t category;
    return category;
}

NS_END3

NS_BEG1(std)

size_t hash<top::system_contracts::error::xerrc_t>::operator()(top::system_contracts::error::xerrc_t errc) const noexcept {
    return static_cast<size_t>(static_cast<std::underlying_type<top::system_contracts::error::xerrc_t>::type>(errc));
}

NS_END1

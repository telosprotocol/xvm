// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xcommon/xip.h"
#include "xvm/xcontract/xcontract_base.h"
#include "xvm/xcontract/xcontract_exec.h"

#include <type_traits>

NS_BEG4(top, xvm, system_contracts, rec)

using namespace xvm;
using namespace xvm::xcontract;

class xtop_rec_registration_contract2 final : public xcontract_base {
    using xbase_t = xcontract_base;

public:
    XDECLARE_DELETED_COPY_DEFAULTED_MOVE_SEMANTICS(xtop_rec_registration_contract2);
    XDECLARE_DEFAULTED_OVERRIDE_DESTRUCTOR(xtop_rec_registration_contract2);

    explicit xtop_rec_registration_contract2(common::xnetwork_id_t const & network_id);

    xcontract_base * clone() override {
        return new xtop_rec_registration_contract2(network_id());
    }

    void setup();

    BEGIN_CONTRACT_WITH_PARAM(xtop_rec_registration_contract2)
    CONTRACT_FUNCTION_PARAM(xtop_rec_registration_contract2, registerNode);
    CONTRACT_FUNCTION_PARAM(xtop_rec_registration_contract2, unregisterNode);
    CONTRACT_FUNCTION_PARAM(xtop_rec_registration_contract2, updateNodeInfo);
    CONTRACT_FUNCTION_PARAM(xtop_rec_registration_contract2, updateNodeType);
    CONTRACT_FUNCTION_PARAM(xtop_rec_registration_contract2, stakeDeposit);
    CONTRACT_FUNCTION_PARAM(xtop_rec_registration_contract2, unstakeDeposit);
    CONTRACT_FUNCTION_PARAM(xtop_rec_registration_contract2, updateNodeSignKey);
    END_CONTRACT_WITH_PARAM

private:
    void registerNode(common::xnetwork_id_t const & network_id,
                      std::string const & role_type_name,
                      std::string const & signing_key
#if defined XENABLE_MOCK_ZEC_STAKE
                      ,
                      common::xaccount_address_t const & registration_account
#endif
    );

    void unregisterNode();

    void updateNodeInfo(common::xnetwork_id_t const & network_id,
                        const int updateDepositType,
                        const uint64_t deposit,
                        const std::string & registration_type,
                        const std::string & node_sign_key);

    void updateNodeType(const std::string & node_types);

    void stakeDeposit();

    void unstakeDeposit(uint64_t unstake_deposit);

    void updateNodeSignKey(const std::string & node_sign_key);
};
using xrec_registration_contract2 = xtop_rec_registration_contract2;

NS_END4

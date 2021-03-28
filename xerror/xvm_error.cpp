// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "xvm_error.h"

NS_BEG2(top, xvm)
using std::string;

xvm_error::xvm_error(const enum_xvm_error_code error_code)
    :xvm_error{make_error_code(error_code)}
{
}

xvm_error::xvm_error(const enum_xvm_error_code error_code, const string& message)
    :xvm_error{make_error_code(error_code), message}
{
}

xvm_error::xvm_error(std::error_code const & ec)
    :std::runtime_error{ec.message()},m_ec(ec)
{
}

xvm_error::xvm_error(std::error_code const & ec, const std::string& message)
    :std::runtime_error{message},m_ec(ec)
{
}

const std::error_code & xvm_error::code() const noexcept {
    return m_ec;
}

const char * xvm_error::what() const noexcept {
    return std::runtime_error::what();
}
NS_END2

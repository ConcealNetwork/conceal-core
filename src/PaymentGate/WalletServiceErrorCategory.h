// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2022 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <system_error>

namespace cn {
namespace error {

enum class WalletServiceErrorCode {
  WRONG_KEY_FORMAT = 1,
  WRONG_PAYMENT_ID_FORMAT,
  WRONG_HASH_FORMAT,
  OBJECT_NOT_FOUND,
  DUPLICATE_KEY,
  KEYS_NOT_DETERMINISTIC
};

// custom category:
class WalletServiceErrorCategory : public std::error_category {
public:
  static WalletServiceErrorCategory INSTANCE;

  const char* name() const throw() override {
    return "WalletServiceErrorCategory";
  }

  std::error_condition default_error_condition(int ev) const throw() override {
    return std::error_condition(ev, *this);
  }

  std::string message(int ev) const override {
    auto code = static_cast<WalletServiceErrorCode>(ev);

    switch (code) {
      case WalletServiceErrorCode::WRONG_KEY_FORMAT: return "Wrong key format";
      case WalletServiceErrorCode::WRONG_PAYMENT_ID_FORMAT: return "Wrong payment id format";
      case WalletServiceErrorCode::WRONG_HASH_FORMAT: return "Wrong block id format";
      case WalletServiceErrorCode::OBJECT_NOT_FOUND: return "Requested object not found";
      case WalletServiceErrorCode::DUPLICATE_KEY: return "Duplicate key";
      case WalletServiceErrorCode::KEYS_NOT_DETERMINISTIC: return "Keys are non-deterministic";
      default: return "Unknown error";
    }
  }

private:
  WalletServiceErrorCategory() = default;
};

} //namespace error
} //namespace cn

inline std::error_code make_error_code(cn::error::WalletServiceErrorCode e) {
  return std::error_code(static_cast<int>(e), cn::error::WalletServiceErrorCategory::INSTANCE);
}

namespace std {

template <>
struct is_error_code_enum<cn::error::WalletServiceErrorCode>: public true_type {};

}

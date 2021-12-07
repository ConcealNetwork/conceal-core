// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2021 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <system_error>

namespace cn {
namespace error {

enum class BlockchainExplorerErrorCodes : int {
  NOT_INITIALIZED = 1,
  ALREADY_INITIALIZED,
  INTERNAL_ERROR,
  REQUEST_ERROR
};

class BlockchainExplorerErrorCategory : public std::error_category {
public:
  static BlockchainExplorerErrorCategory INSTANCE;

  virtual const char* name() const throw() override {
    return "BlockchainExplorerErrorCategory";
  }

  virtual std::error_condition default_error_condition(int ev) const throw() override {
    return std::error_condition(ev, *this);
  }

  virtual std::string message(int ev) const override {
    switch (ev) {
      case static_cast<int>(BlockchainExplorerErrorCodes::NOT_INITIALIZED):     return "Object was not initialized";
      case static_cast<int>(BlockchainExplorerErrorCodes::ALREADY_INITIALIZED): return "Object has been already initialized";
      case static_cast<int>(BlockchainExplorerErrorCodes::INTERNAL_ERROR):      return "Internal error";
      case static_cast<int>(BlockchainExplorerErrorCodes::REQUEST_ERROR):       return "Error in request parameters";
      default:                                                                  return "Unknown error";
    }
  }

private:
  BlockchainExplorerErrorCategory() {
  }
};

} //namespace error
} //namespace cn

inline std::error_code make_error_code(cn::error::BlockchainExplorerErrorCodes e) {
  return std::error_code(static_cast<int>(e), cn::error::BlockchainExplorerErrorCategory::INSTANCE);
}


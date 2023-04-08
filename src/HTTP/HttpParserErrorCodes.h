// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <system_error>

namespace cn {
namespace error {

enum HttpParserErrorCodes {
  STREAM_NOT_GOOD = 1,
  END_OF_STREAM,
  UNEXPECTED_SYMBOL,
  EMPTY_HEADER
};

// custom category:
class HttpParserErrorCategory : public std::error_category {
public:
  static HttpParserErrorCategory INSTANCE;

  virtual const char* name() const throw() override {
    return "HttpParserErrorCategory";
  }

  virtual std::error_condition default_error_condition(int ev) const throw() override {
    return std::error_condition(ev, *this);
  }

  virtual std::string message(int ev) const override {
    switch (ev) {
      case STREAM_NOT_GOOD: return "The stream is not good";
      case END_OF_STREAM: return "The stream is ended";
      case UNEXPECTED_SYMBOL: return "Unexpected symbol";
      case EMPTY_HEADER: return "The header name is empty";
      default: return "Unknown error";
    }
  }

private:
  HttpParserErrorCategory() {
  }
};

} //namespace error
} //namespace cn

inline std::error_code make_error_code(cn::error::HttpParserErrorCodes e) {
  return std::error_code(static_cast<int>(e), cn::error::HttpParserErrorCategory::INSTANCE);
}

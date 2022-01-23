// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2022 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <system_error>

namespace cn {
namespace error {

enum class HttpParserErrorCodes : uint8_t {
  STREAM_NOT_GOOD = 1,
  END_OF_STREAM = 2,
  UNEXPECTED_SYMBOL = 3,
  EMPTY_HEADER = 4
};

// custom category:
class HttpParserErrorCategory : public std::error_category {
public:
  static HttpParserErrorCategory INSTANCE;

  const char* name() const throw() override {
    return "HttpParserErrorCategory";
  }

  std::error_condition default_error_condition(int ev) const throw() override {
    return std::error_condition(ev, *this);
  }

  std::string message(int ev) const override
  {
    switch (static_cast<HttpParserErrorCodes>(ev))
    {
      case HttpParserErrorCodes::STREAM_NOT_GOOD:
        return "The stream is not good";
      case HttpParserErrorCodes::END_OF_STREAM:
        return "The stream is ended";
      case HttpParserErrorCodes::UNEXPECTED_SYMBOL:
        return "Unexpected symbol";
      case HttpParserErrorCodes::EMPTY_HEADER:
        return "The header name is empty";
      default:
        return "Unknown error";
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

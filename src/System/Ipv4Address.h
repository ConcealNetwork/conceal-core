// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <string>

namespace platform_system {

class Ipv4Address {
public:
  explicit Ipv4Address(uint32_t value);
  explicit Ipv4Address(const std::string& dottedDecimal);
  bool operator!=(const Ipv4Address& other) const;
  bool operator==(const Ipv4Address& other) const;
  uint32_t getValue() const;
  bool isLoopback() const;
  bool isPrivate() const;
  std::string toDottedDecimal() const;

private:
  uint32_t value;
};

}

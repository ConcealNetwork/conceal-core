// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>

namespace System {

class Dispatcher;
class Ipv4Address;

class Ipv4Resolver {
public:
  Ipv4Resolver();
  explicit Ipv4Resolver(Dispatcher& dispatcher);
  Ipv4Resolver(const Ipv4Resolver&) = delete;
  Ipv4Resolver(Ipv4Resolver&& other);
  ~Ipv4Resolver();
  Ipv4Resolver& operator=(const Ipv4Resolver&) = delete;
  Ipv4Resolver& operator=(Ipv4Resolver&& other);
  Ipv4Address resolve(const std::string& host);

private:
  Dispatcher* dispatcher;
};

}

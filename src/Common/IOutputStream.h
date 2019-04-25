// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstddef>

namespace Common {

class IOutputStream {
public:
  virtual ~IOutputStream() { }
  virtual size_t writeSome(const void* data, size_t size) = 0;
};

}

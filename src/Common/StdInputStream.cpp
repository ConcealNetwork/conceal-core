// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "StdInputStream.h"

namespace Common {

StdInputStream::StdInputStream(std::istream& in) : in(in) {
}

uint64_t StdInputStream::readSome(void* data, uint64_t size) {
  in.read(static_cast<char*>(data), size);
  return in.gcount();
}

}

// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "VectorOutputStream.h"

namespace Common {

VectorOutputStream::VectorOutputStream(std::vector<uint8_t>& out) : out(out) {
}

uint64_t VectorOutputStream::writeSome(const void* data, uint64_t size) {
  out.insert(out.end(), static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
  return size;
}

}

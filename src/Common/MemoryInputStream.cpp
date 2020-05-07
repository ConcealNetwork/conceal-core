// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "MemoryInputStream.h"
#include <algorithm>
#include <cassert>
#include <cstring> // memcpy

namespace Common {

MemoryInputStream::MemoryInputStream(const void* buffer, uint64_t bufferSize) : 
buffer(static_cast<const char*>(buffer)), bufferSize(bufferSize), position(0) {}

uint64_t MemoryInputStream::getPosition() const {
  return position;
}

bool MemoryInputStream::endOfStream() const {
  return position == bufferSize;
}

uint64_t MemoryInputStream::readSome(void* data, uint64_t size) {
  assert(position <= bufferSize);
  uint64_t readSize = std::min(size, bufferSize - position);

  if (readSize > 0) {
    memcpy(data, buffer + position, readSize);
    position += readSize;
  }
  
  return readSize;
}

}

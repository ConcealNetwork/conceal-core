// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2021 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "MemoryInputStream.h"
#include <algorithm>
#include <cassert>
#include <cstring> // memcpy

namespace common {

MemoryInputStream::MemoryInputStream(const void* buffer, size_t bufferSize) : 
buffer(static_cast<const char*>(buffer)), bufferSize(bufferSize), position(0) {}

size_t MemoryInputStream::getPosition() const {
  return position;
}

bool MemoryInputStream::endOfStream() const {
  return position == bufferSize;
}

size_t MemoryInputStream::readSome(void* data, size_t size) {
  assert(position <= bufferSize);
  size_t readSize = std::min(size, bufferSize - position);

  if (readSize > 0) {
    memcpy(data, buffer + position, readSize);
    position += readSize;
  }
  
  return readSize;
}

}

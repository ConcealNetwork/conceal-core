// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2015-2016 The Bytecoin developers
// Copyright (c) 2016-2017 The TurtleCoin developers
// Copyright (c) 2017-2018 krypt0x aka krypt0chaos
// Copyright (c) 2018 The Circle Foundation
//
// This file is part of Conceal Sense Crypto Engine.
//
// Conceal is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Conceal is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Conceal.  If not, see <http://www.gnu.org/licenses/>.

#include "Serialization/SerializationOverloads.h"

#include <limits>

namespace CryptoNote {

void serializeBlockHeight(ISerializer& s, uint32_t& blockHeight, Common::StringView name) {
  if (s.type() == ISerializer::INPUT) {
    uint64_t height;
    s(height, name);

    if (height == std::numeric_limits<uint64_t>::max()) {
      blockHeight = std::numeric_limits<uint32_t>::max();
    } else if (height > std::numeric_limits<uint32_t>::max() && height < std::numeric_limits<uint64_t>::max()) {
      throw std::runtime_error("Deserialization error: wrong value");
    } else {
      blockHeight = static_cast<uint32_t>(height);
    }
  } else {
    s(blockHeight, name);
  }
}

void serializeGlobalOutputIndex(ISerializer& s, uint32_t& globalOutputIndex, Common::StringView name) {
  serializeBlockHeight(s, globalOutputIndex, name);
}

} //namespace CryptoNote

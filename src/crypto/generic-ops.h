// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstddef>
#include <cstring>
#include <functional>

#define CRYPTO_MAKE_COMPARABLE(type) \
namespace Crypto { \
  inline bool operator==(const type &_v1, const type &_v2) { \
    return std::memcmp(&_v1, &_v2, sizeof(type)) == 0; \
  } \
  inline bool operator!=(const type &_v1, const type &_v2) { \
    return std::memcmp(&_v1, &_v2, sizeof(type)) != 0; \
  } \
}

#define CRYPTO_MAKE_HASHABLE(type) \
CRYPTO_MAKE_COMPARABLE(type) \
namespace Crypto { \
  static_assert(sizeof(uint64_t) <= sizeof(type), "Size of " #type " must be at least that of uint64_t"); \
  inline uint64_t hash_value(const type &_v) { \
    return reinterpret_cast<const uint64_t &>(_v); \
  } \
} \
namespace std { \
  template<> \
  struct hash<Crypto::type> { \
    uint64_t operator()(const Crypto::type &_v) const { \
      return reinterpret_cast<const uint64_t &>(_v); \
    } \
  }; \
}

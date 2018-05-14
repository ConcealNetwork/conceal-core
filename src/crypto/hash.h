// Copyright (c) 2017-2018, The Conceal developers
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

#pragma once

#include <stddef.h>

#include <CryptoTypes.h>
#include "generic-ops.h"

namespace Crypto {

  extern "C" {
#include "hash-ops.h"
  }

  /*
    Cryptonight hash functions
  */

  inline void cn_fast_hash(const void *data, size_t length, Hash &hash) {
    cn_fast_hash(data, length, reinterpret_cast<char *>(&hash));
  }

  inline Hash cn_fast_hash(const void *data, size_t length) {
    Hash h;
    cn_fast_hash(data, length, reinterpret_cast<char *>(&h));
    return h;
  }

  class cn_context {
  public:

    cn_context();
    ~cn_context();
#if !defined(_MSC_VER) || _MSC_VER >= 1800
    cn_context(const cn_context &) = delete;
    void operator=(const cn_context &) = delete;
#endif

  private:

    void *data;
    friend inline void cn_slow_hash_v6(cn_context &, const void *, size_t, Hash &);
    friend inline void cn_slow_hash_v7(cn_context &, const void *, size_t, Hash &);
    friend inline void cn_lite_slow_hash_v0(cn_context &, const void *, size_t, Hash &);
    friend inline void cn_lite_slow_hash_v1(cn_context &, const void *, size_t, Hash &);
  };

  inline void cn_slow_hash_v6(cn_context &context, const void *data, size_t length, Hash &hash) {
    (*cn_slow_hash_f)(context.data, data, length, reinterpret_cast<void *>(&hash), 0, 0);
  }

  inline void cn_slow_hash_v7(cn_context &context, const void *data, size_t length, Hash &hash) {
    (*cn_slow_hash_f)(context.data, data, length, reinterpret_cast<void *>(&hash), 0, 1);
  }

  inline void cn_lite_slow_hash_v0(cn_context &context, const void *data, size_t length, Hash &hash) {
    (*cn_slow_hash_f)(context.data, data, length, reinterpret_cast<void *>(&hash), 1, 0);
  }

  inline void cn_lite_slow_hash_v1(cn_context &context, const void *data, size_t length, Hash &hash) {
    (*cn_slow_hash_f)(context.data, data, length, reinterpret_cast<void *>(&hash), 1, 1);
  }

  inline void tree_hash(const Hash *hashes, size_t count, Hash &root_hash) {
    tree_hash(reinterpret_cast<const char (*)[HASH_SIZE]>(hashes), count, reinterpret_cast<char *>(&root_hash));
  }

  inline void tree_branch(const Hash *hashes, size_t count, Hash *branch) {
    tree_branch(reinterpret_cast<const char (*)[HASH_SIZE]>(hashes), count, reinterpret_cast<char (*)[HASH_SIZE]>(branch));
  }

  inline void tree_hash_from_branch(const Hash *branch, size_t depth, const Hash &leaf, const void *path, Hash &root_hash) {
    tree_hash_from_branch(reinterpret_cast<const char (*)[HASH_SIZE]>(branch), depth, reinterpret_cast<const char *>(&leaf), path, reinterpret_cast<char *>(&root_hash));
  }

}

CRYPTO_MAKE_HASHABLE(Hash)

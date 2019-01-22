// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018 The Circle Foundation
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stddef.h>

#include <CryptoTypes.h>
#include "generic-ops.h"

/* Standard Cryptonight */
#define CN_PAGE_SIZE                    2097152
#define CN_SCRATCHPAD                   2097152
#define CN_ITERATIONS                   1048576

/* Cryptonight Fast */
#define CN_FAST_PAGE_SIZE               2097152
#define CN_FAST_SCRATCHPAD              2097152
#define CN_FAST_ITERATIONS              524288

/* Cryptonight Lite */
#define CN_LITE_PAGE_SIZE               2097152
#define CN_LITE_SCRATCHPAD              1048576
#define CN_LITE_ITERATIONS              524288

/* Cryptonight Dark */
#define CN_DARK_PAGE_SIZE               524288
#define CN_DARK_SCRATCHPAD              524288
#define CN_DARK_ITERATIONS              262144

/* Cryptonight Turtle */
#define CN_TURTLE_PAGE_SIZE             262144
#define CN_TURTLE_SCRATCHPAD            262144
#define CN_TURTLE_ITERATIONS            131072

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
    friend inline void cn_slow_hash(cn_context &, const void *, size_t, Hash &);
  };

  /* Standard Cryptonight */

  inline void cn_slow_hash(cn_context &context, const void *data, size_t length, Hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 0, 0, 0, CN_PAGE_SIZE, CN_SCRATCHPAD, CN_ITERATIONS);
  }

  inline void cn_slow_hash_v1(cn_context &context, const void *data, size_t length, Hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 0, 1, 0, CN_PAGE_SIZE, CN_SCRATCHPAD, CN_ITERATIONS);
  }

  inline void cn_slow_hash_v2(cn_context &context, const void *data, size_t length, Hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 0, 2, 0, CN_PAGE_SIZE, CN_SCRATCHPAD, CN_ITERATIONS);
  }

  /* Cryptonight Fast */

  inline void cn_fast_slow_hash_v0(cn_context &context, const void *data, size_t length, Hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 0, 0, 0, CN_FAST_PAGE_SIZE, CN_FAST_SCRATCHPAD, CN_FAST_ITERATIONS);
  }

  inline void cn_fast_slow_hash_v1(cn_context &context, const void *data, size_t length, Hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 0, 1, 0, CN_FAST_PAGE_SIZE, CN_FAST_SCRATCHPAD, CN_FAST_ITERATIONS);
  }

  inline void cn_fast_slow_hash_v2(cn_context &context, const void *data, size_t length, Hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 0, 2, 0, CN_FAST_PAGE_SIZE, CN_FAST_SCRATCHPAD, CN_FAST_ITERATIONS);
  }

  /* Cryptonight Lite */

  inline void cn_lite_slow_hash_v0(cn_context &context, const void *data, size_t length, Hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 1, 0, 0, CN_LITE_PAGE_SIZE, CN_LITE_SCRATCHPAD, CN_LITE_ITERATIONS);
  }

  inline void cn_lite_slow_hash_v1(cn_context &context, const void *data, size_t length, Hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 1, 1, 0, CN_LITE_PAGE_SIZE, CN_LITE_SCRATCHPAD, CN_LITE_ITERATIONS);
  }

  inline void cn_lite_slow_hash_v2(cn_context &context, const void *data, size_t length, Hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 1, 2, 0, CN_LITE_PAGE_SIZE, CN_LITE_SCRATCHPAD, CN_LITE_ITERATIONS);
  }

  /* Cryptonight Dark */

  inline void cn_dark_slow_hash_v0(cn_context &context, const void *data, size_t length, Hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 0, 0, 0, CN_DARK_PAGE_SIZE, CN_DARK_SCRATCHPAD, CN_DARK_ITERATIONS);
  }

  inline void cn_dark_slow_hash_v1(cn_context &context, const void *data, size_t length, Hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 0, 1, 0, CN_DARK_PAGE_SIZE, CN_DARK_SCRATCHPAD, CN_DARK_ITERATIONS);
  }

  inline void cn_dark_slow_hash_v2(cn_context &context, const void *data, size_t length, Hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 0, 2, 0, CN_DARK_PAGE_SIZE, CN_DARK_SCRATCHPAD, CN_DARK_ITERATIONS);
  }

  /* Cryptonight Turtle */

  inline void cn_turtle_slow_hash_v0(cn_context &context, const void *data, size_t length, Hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 0, 0, 0, CN_TURTLE_PAGE_SIZE, CN_TURTLE_SCRATCHPAD, CN_TURTLE_ITERATIONS);
  }

  inline void cn_turtle_slow_hash_v1(cn_context &context, const void *data, size_t length, Hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 0, 1, 0, CN_TURTLE_PAGE_SIZE, CN_TURTLE_SCRATCHPAD, CN_TURTLE_ITERATIONS);
  }

  inline void cn_turtle_slow_hash_v2(cn_context &context, const void *data, size_t length, Hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 0, 2, 0, CN_TURTLE_PAGE_SIZE, CN_TURTLE_SCRATCHPAD, CN_TURTLE_ITERATIONS);
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

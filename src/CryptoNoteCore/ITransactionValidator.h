// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "CryptoNoteCore/CryptoNoteBasic.h"

namespace cn {

  struct BlockInfo {
    uint32_t height;
    crypto::Hash id;

    BlockInfo() {
      clear();
    }

    void clear() {
      height = 0;
      id = cn::NULL_HASH;
    }

    bool empty() const {
      return id == cn::NULL_HASH;
    }
  };

  class ITransactionValidator {
  public:
    virtual ~ITransactionValidator() = default;

    virtual bool checkTransactionInputs(const cn::Transaction& tx, BlockInfo& maxUsedBlock) = 0;
    virtual bool checkTransactionInputs(const cn::Transaction& tx, BlockInfo& maxUsedBlock, BlockInfo& lastFailed) = 0;
    virtual bool haveSpentKeyImages(const cn::Transaction& tx) = 0;
    virtual bool checkTransactionSize(size_t blobSize) = 0;
  };

}

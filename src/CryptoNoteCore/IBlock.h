// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "CryptoNote.h"

namespace CryptoNote {
class IBlock {
public:
  virtual ~IBlock();

  virtual const Block& getBlock() const = 0;
  virtual uint64_t getTransactionCount() const = 0;
  virtual const Transaction& getTransaction(uint64_t index) const = 0;
};
}

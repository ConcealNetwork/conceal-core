// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <vector>
#include <cstddef>
#include <cstdint>

namespace CryptoNote {
class ISerializer;

class DepositIndex {
public:
  using DepositAmount = int64_t;
  using DepositInterest = uint64_t;
  using DepositHeight = uint32_t;
  DepositIndex();
  explicit DepositIndex(DepositHeight expectedHeight);
  void pushBlock(DepositAmount amount, DepositInterest interest); 
  void popBlock(); 
  void reserve(DepositHeight expectedHeight);
  uint64_t popBlocks(DepositHeight from); 
  DepositAmount depositAmountAtHeight(DepositHeight height) const;
  DepositAmount fullDepositAmount() const; 
  DepositInterest depositInterestAtHeight(DepositHeight height) const;
  DepositInterest fullInterestAmount() const; 
  DepositHeight size() const;
  void serialize(ISerializer& s);

private:
  struct DepositIndexEntry {
    DepositHeight height;
    DepositAmount amount;
    DepositInterest interest;

    void serialize(ISerializer& s);
  };

  using IndexType = std::vector<DepositIndexEntry>;
  IndexType::const_iterator upperBound(DepositHeight height) const;
  IndexType index;
  DepositHeight blockCount;
};
}

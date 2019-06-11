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

class InvestmentIndex {
public:
  using DepositAmount = int64_t;
  using DepositInterest = uint64_t;
  using DepositHeight = uint32_t;
  InvestmentIndex();
  explicit InvestmentIndex(DepositHeight expectedHeight);
  void pushBlock(DepositAmount amount, DepositInterest interest); 
  void popBlock(); 
  void reserve(DepositHeight expectedHeight);
  size_t popBlocks(DepositHeight from); 
  DepositAmount investmentAmountAtHeight(DepositHeight height) const;
  DepositAmount fullDepositAmount() const; 
  DepositInterest depositInterestAtHeight(DepositHeight height) const;
  DepositInterest fullInterestAmount() const; 
  DepositHeight size() const;
  void serialize(ISerializer& s);

private:
  struct InvestmentIndexEntry {
    DepositHeight height;
    DepositAmount amount;
    DepositInterest interest;

    void serialize(ISerializer& s);
  };

  using IndexType = std::vector<InvestmentIndexEntry>;
  IndexType::const_iterator upperBound(DepositHeight height) const;
  IndexType index;
  DepositHeight blockCount;
};
}

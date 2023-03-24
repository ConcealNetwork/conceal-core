// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <memory>
#include "ITransaction.h"

namespace cn {
  std::unique_ptr<ITransaction> createTransaction();
  std::unique_ptr<ITransaction> createTransaction(const BinaryArray& transactionBlob);
  std::unique_ptr<ITransaction> createTransaction(const Transaction& tx);

  std::unique_ptr<ITransactionReader> createTransactionPrefix(const TransactionPrefix& prefix, const crypto::Hash& transactionHash);
  std::unique_ptr<ITransactionReader> createTransactionPrefix(const Transaction& fullTransaction);
}

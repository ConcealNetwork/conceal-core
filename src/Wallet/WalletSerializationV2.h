// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include "Common/IInputStream.h"
#include "Common/IOutputStream.h"
#include "Serialization/ISerializer.h"
#include "Transfers/TransfersSynchronizer.h"
#include "Wallet/WalletIndices.h"
#include "IWallet.h"

namespace cn {

class WalletSerializerV2 {
public:
  WalletSerializerV2(
    ITransfersObserver& transfersObserver,
    crypto::PublicKey& viewPublicKey,
    crypto::SecretKey& viewSecretKey,
    uint64_t& actualBalance,
    uint64_t& pendingBalance,
    uint64_t& lockedDepositBalance,
    uint64_t& unlockedDepositBalance,
    WalletsContainer& walletsContainer,
    TransfersSyncronizer& synchronizer,
    UnlockTransactionJobs& unlockTransactions,
    WalletTransactions& transactions,
    WalletTransfers& transfers,
    WalletDeposits& deposits,
    UncommitedTransactions& uncommitedTransactions,
    std::string& extra,
    uint32_t transactionSoftLockTime
  );

  void load(common::IInputStream& source, uint8_t version);
  void save(common::IOutputStream& destination, WalletSaveLevel saveLevel);

  std::unordered_set<crypto::PublicKey>& addedKeys();
  std::unordered_set<crypto::PublicKey>& deletedKeys();

  static const uint8_t MIN_VERSION = 6;
  static const uint8_t SERIALIZATION_VERSION = 6;

private:
  void loadKeyListAndBanalces(cn::ISerializer& serializer, bool saveCache);
  void saveKeyListAndBanalces(cn::ISerializer& serializer, bool saveCache);
    
  void loadTransactions(cn::ISerializer& serializer);
  void saveTransactions(cn::ISerializer& serializer);

  void loadDeposits(cn::ISerializer& serializer);
  void saveDeposits(cn::ISerializer& serializer);

  void loadTransfers(cn::ISerializer& serializer);
  void saveTransfers(cn::ISerializer& serializer);

  void loadTransfersSynchronizer(cn::ISerializer& serializer);
  void saveTransfersSynchronizer(cn::ISerializer& serializer);

  void loadUnlockTransactionsJobs(cn::ISerializer& serializer);
  void saveUnlockTransactionsJobs(cn::ISerializer& serializer);

  ITransfersObserver& m_transfersObserver;
  uint64_t& m_actualBalance;
  uint64_t& m_pendingBalance;
  uint64_t& m_lockedDepositBalance;
  uint64_t& m_unlockedDepositBalance;
  WalletsContainer& m_walletsContainer;
  TransfersSyncronizer& m_synchronizer;
  UnlockTransactionJobs& m_unlockTransactions;
  WalletTransactions& m_transactions;
  WalletTransfers& m_transfers;
  WalletDeposits& m_deposits;
  UncommitedTransactions& m_uncommitedTransactions;
  std::string& m_extra;
  uint32_t m_transactionSoftLockTime;

  std::unordered_set<crypto::PublicKey> m_addedKeys;
  std::unordered_set<crypto::PublicKey> m_deletedKeys;
};

} //namespace cn

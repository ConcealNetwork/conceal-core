// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "IWallet.h"
#include "WalletIndices.h"
#include "Common/IInputStream.h"
#include "Common/IOutputStream.h"
#include "Transfers/TransfersSynchronizer.h"
#include "Serialization/BinaryInputStreamSerializer.h"

#include "crypto/chacha8.h"

namespace cn
{

struct CryptoContext
{
  crypto::chacha8_key key;
  crypto::chacha8_iv iv;

  void incIv();
};

class WalletSerializer
{
public:
  WalletSerializer(
      ITransfersObserver &transfersObserver,
      crypto::PublicKey &viewPublicKey,
      crypto::SecretKey &viewSecretKey,
      uint64_t &actualBalance,
      uint64_t &pendingBalance,
      WalletsContainer &walletsContainer,
      TransfersSyncronizer &synchronizer,
      UnlockTransactionJobs &unlockTransactions,
      WalletTransactions &transactions,
      WalletTransfers &transfers,
      uint32_t transactionSoftLockTime,
      UncommitedTransactions &uncommitedTransactions);

  void save(const std::string &password, common::IOutputStream &destination, bool saveDetails, bool saveCache);
  void load(const crypto::chacha8_key &key, common::IInputStream &source);

private:
  static const uint32_t SERIALIZATION_VERSION;

  void loadWallet(common::IInputStream &source, const crypto::chacha8_key &key, uint32_t version);
  void loadWalletV1(common::IInputStream &source, const crypto::chacha8_key &key);

  CryptoContext generateCryptoContext(const std::string &password);

  void saveVersion(common::IOutputStream &destination);
  void saveIv(common::IOutputStream &destination, crypto::chacha8_iv &iv);
  void saveKeys(common::IOutputStream &destination, CryptoContext &cryptoContext);
  void savePublicKey(common::IOutputStream &destination, CryptoContext &cryptoContext);
  void saveSecretKey(common::IOutputStream &destination, CryptoContext &cryptoContext);
  void saveFlags(bool saveDetails, bool saveCache, common::IOutputStream &destination, CryptoContext &cryptoContext);
  void saveWallets(common::IOutputStream &destination, bool saveCache, CryptoContext &cryptoContext);
  void saveBalances(common::IOutputStream &destination, bool saveCache, CryptoContext &cryptoContext);
  void saveTransfersSynchronizer(common::IOutputStream &destination, CryptoContext &cryptoContext);
  void saveUnlockTransactionsJobs(common::IOutputStream &destination, CryptoContext &cryptoContext);
  void saveUncommitedTransactions(common::IOutputStream &destination, CryptoContext &cryptoContext);
  void saveTransactions(common::IOutputStream &destination, CryptoContext &cryptoContext);
  void saveTransfers(common::IOutputStream &destination, CryptoContext &cryptoContext);

  uint32_t loadVersion(common::IInputStream &source);
  void loadIv(common::IInputStream &source, crypto::chacha8_iv &iv);
  void generateKey(const std::string &password, crypto::chacha8_key &key);
  void loadKeys(common::IInputStream &source, CryptoContext &cryptoContext);
  void loadPublicKey(common::IInputStream &source, CryptoContext &cryptoContext);
  void loadSecretKey(common::IInputStream &source, CryptoContext &cryptoContext);
  void checkKeys();
  void loadFlags(bool &details, bool &cache, common::IInputStream &source, CryptoContext &cryptoContext);
  void loadWallets(common::IInputStream &source, CryptoContext &cryptoContext);
  void subscribeWallets();
  void loadBalances(common::IInputStream &source, CryptoContext &cryptoContext);
  void loadTransfersSynchronizer(common::IInputStream &source, CryptoContext &cryptoContext);
  void loadObsoleteSpentOutputs(common::IInputStream &source, CryptoContext &cryptoContext);
  void loadUnlockTransactionsJobs(common::IInputStream &source, CryptoContext &cryptoContext);
  void loadObsoleteChange(common::IInputStream &source, CryptoContext &cryptoContext);
  void loadUncommitedTransactions(common::IInputStream &source, CryptoContext &cryptoContext);
  void loadTransactions(common::IInputStream &source, CryptoContext &cryptoContext);
  void loadTransfers(common::IInputStream &source, CryptoContext &cryptoContext, uint32_t version);

  void loadWalletV1Keys(cn::BinaryInputStreamSerializer &serializer);
  void loadWalletV1Details(cn::BinaryInputStreamSerializer &serializer);
  void addWalletV1Details(const std::vector<WalletLegacyTransaction> &txs, const std::vector<WalletLegacyTransfer> &trs);
  void initTransactionPool();
  void resetCachedBalance();
  void updateTransactionsBaseStatus();
  void updateTransfersSign();

  ITransfersObserver &m_transfersObserver;
  crypto::PublicKey &m_viewPublicKey;
  crypto::SecretKey &m_viewSecretKey;
  uint64_t &m_actualBalance;
  uint64_t &m_pendingBalance;
  WalletsContainer &m_walletsContainer;
  TransfersSyncronizer &m_synchronizer;
  UnlockTransactionJobs &m_unlockTransactions;
  WalletTransactions &m_transactions;
  WalletTransfers &m_transfers;
  uint32_t m_transactionSoftLockTime;
  UncommitedTransactions &uncommitedTransactions;
};

} //namespace cn

// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2021 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <list>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

#include "IWalletLegacy.h"
#include "INode.h"
#include "Wallet/WalletErrors.h"
#include "Wallet/WalletAsyncContextCounter.h"
#include "Common/ObserverManager.h"
#include "CryptoNoteCore/TransactionExtra.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/Currency.h"
#include "WalletLegacy/WalletUserTransactionsCache.h"
#include "WalletLegacy/WalletUnconfirmedTransactions.h"

#include "WalletLegacy/WalletTransactionSender.h"
#include "WalletLegacy/WalletRequest.h"

#include "Transfers/BlockchainSynchronizer.h"
#include "Transfers/TransfersSynchronizer.h"

namespace cn {

class SyncStarter;

class WalletLegacy :
  public IWalletLegacy,
  IBlockchainSynchronizerObserver,
  ITransfersObserver {

public:
  WalletLegacy(const cn::Currency& currency, INode& node, logging::ILogger& loggerGroup);
  virtual ~WalletLegacy();

  virtual void addObserver(IWalletLegacyObserver* observer) override;
  virtual void removeObserver(IWalletLegacyObserver* observer) override;

  virtual void initAndGenerate(const std::string& password) override;
  virtual void initAndLoad(std::istream& source, const std::string& password) override;
  virtual void initWithKeys(const AccountKeys& accountKeys, const std::string& password) override;
  virtual void shutdown() override;
  virtual void reset() override;
  virtual bool checkWalletPassword(std::istream& source, const std::string& password) override;

  virtual void save(std::ostream& destination, bool saveDetailed = true, bool saveCache = true) override;

  virtual std::error_code changePassword(const std::string& oldPassword, const std::string& newPassword) override;

  virtual std::string getAddress() override;

  virtual uint64_t actualBalance() override;
  virtual uint64_t pendingBalance() override;
  virtual uint64_t actualDepositBalance() override;
  virtual uint64_t actualInvestmentBalance() override;  
  virtual uint64_t pendingDepositBalance() override;
  virtual uint64_t pendingInvestmentBalance() override;  

  virtual size_t getTransactionCount() override;
  virtual size_t getTransferCount() override;
  virtual size_t getDepositCount() override;
  virtual size_t getNumUnlockedOutputs() override;
  virtual std::vector<TransactionOutputInformation> getUnspentOutputs() override;
  virtual bool isTrackingWallet();
  virtual TransactionId findTransactionByTransferId(TransferId transferId) override;
  virtual bool getTxProof(crypto::Hash& txid, cn::AccountPublicAddress& address, crypto::SecretKey& tx_key, std::string& sig_str) override;
  virtual std::string getReserveProof(const uint64_t &reserve, const std::string &message) override;
  virtual crypto::SecretKey getTxKey(crypto::Hash& txid) override;
  virtual bool get_tx_key(crypto::Hash& txid, crypto::SecretKey& txSecretKey) override;
  virtual bool getTransaction(TransactionId transactionId, WalletLegacyTransaction& transaction) override;
  virtual bool getTransfer(TransferId transferId, WalletLegacyTransfer& transfer) override;
  virtual bool getDeposit(DepositId depositId, Deposit& deposit) override;
  virtual std::vector<Payments> getTransactionsByPaymentIds(const std::vector<PaymentId>& paymentIds) const override;

  virtual TransactionId sendTransaction(crypto::SecretKey& transactionSK,
                                        const WalletLegacyTransfer& transfer,
                                        uint64_t fee,
                                        const std::string& extra = "",
                                        uint64_t mixIn = 5,
                                        uint64_t unlockTimestamp = 0,
                                        const std::vector<TransactionMessage>& messages = std::vector<TransactionMessage>(),
                                        uint64_t ttl = 0) override;
  virtual TransactionId sendTransaction(crypto::SecretKey& transactionSK,
                                        std::vector<WalletLegacyTransfer>& transfers,
                                        uint64_t fee,
                                        const std::string& extra = "",
                                        uint64_t mixIn = 5,
                                        uint64_t unlockTimestamp = 0,
                                        const std::vector<TransactionMessage>& messages = std::vector<TransactionMessage>(),
                                        uint64_t ttl = 0) override;
  virtual size_t estimateFusion(const uint64_t& threshold);
  virtual std::list<TransactionOutputInformation> selectFusionTransfersToSend(uint64_t threshold, size_t minInputCount, size_t maxInputCount);
  virtual TransactionId sendFusionTransaction(const std::list<TransactionOutputInformation>& fusionInputs, uint64_t fee, const std::string& extra = "", uint64_t mixIn = 0, uint64_t unlockTimestamp = 0);
  virtual TransactionId deposit(uint32_t term, uint64_t amount, uint64_t fee, uint64_t mixIn = 5) override;
  virtual TransactionId withdrawDeposits(const std::vector<DepositId>& depositIds, uint64_t fee) override;
  virtual std::error_code cancelTransaction(size_t transactionId) override;

  virtual void getAccountKeys(AccountKeys& keys) override;

private:

  // IBlockchainSynchronizerObserver
  virtual void synchronizationProgressUpdated(uint32_t current, uint32_t total) override;
  virtual void synchronizationCompleted(std::error_code result) override;

  // ITransfersObserver
  virtual void onTransactionUpdated(ITransfersSubscription* object, const crypto::Hash& transactionHash) override;
  virtual void onTransactionDeleted(ITransfersSubscription* object, const crypto::Hash& transactionHash) override;
  virtual void onTransfersUnlocked(ITransfersSubscription* object, const std::vector<TransactionOutputInformation>& unlockedTransfers) override;
  virtual void onTransfersLocked(ITransfersSubscription* object, const std::vector<TransactionOutputInformation>& lockedTransfers) override;

  void initSync();
  void throwIfNotInitialised();

  void doSave(std::ostream& destination, bool saveDetailed, bool saveCache);
  void doLoad(std::istream& source);

  void synchronizationCallback(WalletRequest::Callback callback, std::error_code ec);
  void sendTransactionCallback(WalletRequest::Callback callback, std::error_code ec);
  void notifyClients(std::deque<std::unique_ptr<WalletLegacyEvent> >& events);
  void notifyIfBalanceChanged();
  void notifyIfDepositBalanceChanged();
  void notifyIfInvestmentBalanceChanged();  

  std::unique_ptr<WalletLegacyEvent> getActualInvestmentBalanceChangedEvent();
  std::unique_ptr<WalletLegacyEvent> getPendingInvestmentBalanceChangedEvent();

  std::unique_ptr<WalletLegacyEvent> getActualDepositBalanceChangedEvent();
  std::unique_ptr<WalletLegacyEvent> getPendingDepositBalanceChangedEvent();

  std::unique_ptr<WalletLegacyEvent> getActualBalanceChangedEvent();
  std::unique_ptr<WalletLegacyEvent> getPendingBalanceChangedEvent();

  uint64_t calculateActualDepositBalance();
  uint64_t calculateActualInvestmentBalance();
  uint64_t calculatePendingDepositBalance();
  uint64_t calculatePendingInvestmentBalance();
  uint64_t getWalletMaximum();
  uint64_t dustBalance();

  uint64_t calculateActualBalance();
  uint64_t calculatePendingBalance();

  void pushBalanceUpdatedEvents(std::deque<std::unique_ptr<WalletLegacyEvent>>& eventsQueue);

  std::vector<TransactionId> deleteOutdatedUnconfirmedTransactions();

  std::vector<uint32_t> getTransactionHeights(std::vector<TransactionOutputInformation> transfers);


  enum WalletState
  {
    NOT_INITIALIZED = 0,
    INITIALIZED,
    LOADING,
    SAVING
  };

  WalletState m_state;
  std::mutex m_cacheMutex;
  cn::AccountBase m_account;
  std::string m_password;
  const cn::Currency& m_currency;
  INode& m_node;
  logging::ILogger& m_loggerGroup;  
  bool m_isStopping;

  std::atomic<uint64_t> m_lastNotifiedActualBalance;
  std::atomic<uint64_t> m_lastNotifiedPendingBalance;

  std::atomic<uint64_t> m_lastNotifiedActualDepositBalance;
  std::atomic<uint64_t> m_lastNotifiedPendingDepositBalance;
  std::atomic<uint64_t> m_lastNotifiedActualInvestmentBalance;
  std::atomic<uint64_t> m_lastNotifiedPendingInvestmentBalance;  

  BlockchainSynchronizer m_blockchainSync;
  TransfersSyncronizer m_transfersSync;
  ITransfersContainer* m_transferDetails;

  WalletUserTransactionsCache m_transactionsCache;
  std::unique_ptr<WalletTransactionSender> m_sender;

  WalletAsyncContextCounter m_asyncContextCounter;
  tools::ObserverManager<cn::IWalletLegacyObserver> m_observerManager;

  std::unique_ptr<SyncStarter> m_onInitSyncStarter;
};

} //namespace cn

// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "IWallet.h"

#include <queue>
#include <unordered_map>

#include "IFusionManager.h"
#include "WalletIndices.h"
#include "Common/StringOutputStream.h"
#include "Logging/LoggerRef.h"
#include <System/Dispatcher.h>
#include <System/Event.h>
#include "Transfers/TransfersSynchronizer.h"
#include "Transfers/BlockchainSynchronizer.h"

namespace cn
{

class WalletGreen : public IWallet,
                    public ITransfersObserver,
                    public IBlockchainSynchronizerObserver,
                    public ITransfersSynchronizerObserver,
                    public IFusionManager
{
public:
  WalletGreen(platform_system::Dispatcher &dispatcher, const Currency &currency, INode &node, logging::ILogger &logger, uint32_t transactionSoftLockTime = 1);
  ~WalletGreen() override;

  /* Deposit related functions */
  void createDeposit(uint64_t amount, uint64_t term, std::string sourceAddress, std::string destinationAddress, std::string &transactionHash) override;
  void withdrawDeposit(DepositId depositId, std::string &transactionHash) override;
  std::vector<MultisignatureInput> prepareMultisignatureInputs(const std::vector<TransactionOutputInformation> &selectedTransfers);

  
  void initialize(const std::string& path, const std::string& password) override;
  void initializeWithViewKey(const std::string& path, const std::string& password, const crypto::SecretKey& viewSecretKey) override;
  void load(const std::string& path, const std::string& password, std::string& extra) override;
  void load(const std::string& path, const std::string& password) override;
  void shutdown() override;


  void changePassword(const std::string &oldPassword, const std::string &newPassword) override;
  void save(WalletSaveLevel saveLevel = WalletSaveLevel::SAVE_ALL, const std::string& extra = "") override;
  void reset(const uint64_t scanHeight) override;
  void exportWallet(const std::string &path, WalletSaveLevel saveLevel, bool encrypt = true, const std::string &extra = "") override;

  size_t getAddressCount() const override;
  size_t getWalletDepositCount() const override;  
  std::string getAddress(size_t index) const override;
  KeyPair getAddressSpendKey(size_t index) const override;
  KeyPair getAddressSpendKey(const std::string &address) const override;
  KeyPair getViewKey() const override;
  std::string createAddress() override;
  std::string createAddress(const crypto::SecretKey &spendSecretKey) override;
  std::string createAddress(const crypto::PublicKey &spendPublicKey) override;
  std::vector<std::string> createAddressList(const std::vector<crypto::SecretKey> &spendSecretKeys, bool reset = true) override;

  void deleteAddress(const std::string &address) override;

  uint64_t getActualBalance() const override;
  uint64_t getActualBalance(const std::string &address) const override;
  uint64_t getPendingBalance() const override;
  uint64_t getPendingBalance(const std::string &address) const override;

  uint64_t getLockedDepositBalance() const override;
  uint64_t getLockedDepositBalance(const std::string &address) const override;
  uint64_t getUnlockedDepositBalance() const override;
  uint64_t getUnlockedDepositBalance(const std::string &address) const override;

  size_t getTransactionCount() const override;
  WalletTransaction getTransaction(size_t transactionIndex) const override;
  Deposit getDeposit(size_t depositIndex) const override;
  size_t getTransactionTransferCount(size_t transactionIndex) const override;
  WalletTransfer getTransactionTransfer(size_t transactionIndex, size_t transferIndex) const override;

  WalletTransactionWithTransfers getTransaction(const crypto::Hash &transactionHash) const override;

  std::vector<TransactionsInBlockInfo> getTransactions(const crypto::Hash &blockHash, size_t count) const override;
  std::vector<TransactionsInBlockInfo> getTransactions(uint32_t blockIndex, size_t count) const override;
  
  std::vector<DepositsInBlockInfo> getDeposits(const crypto::Hash &blockHash, size_t count) const override;
  std::vector<DepositsInBlockInfo> getDeposits(uint32_t blockIndex, size_t count) const override;
  
  std::vector<crypto::Hash> getBlockHashes(uint32_t blockIndex, size_t count) const override;
  uint32_t getBlockCount() const override;
  std::vector<WalletTransactionWithTransfers> getUnconfirmedTransactions() const override;

  std::vector<size_t> getDelayedTransactionIds() const override;

  size_t transfer(const TransactionParameters &sendingTransaction, crypto::SecretKey &transactionSK) override;

  size_t makeTransaction(const TransactionParameters &sendingTransaction) override;
  void commitTransaction(size_t) override;
  void rollbackUncommitedTransaction(size_t) override;
  void start() override;
  void stop() override;
  WalletEvent getEvent() override;

  size_t createFusionTransaction(uint64_t threshold, uint64_t mixin,
                                         const std::vector<std::string> &sourceAddresses = {}, const std::string &destinationAddress = "") override;
  bool isFusionTransaction(size_t transactionId) const override;
  IFusionManager::EstimateResult estimate(uint64_t threshold, const std::vector<std::string> &sourceAddresses = {}) const override;

  DepositId insertDeposit(const Deposit &deposit, size_t depositIndexInTransaction, const crypto::Hash &transactionHash);
  DepositId insertNewDeposit(const TransactionOutputInformation &depositOutput,
                             TransactionId creatingTransactionId,
                             const Currency &currency, uint32_t height);

protected:
  struct NewAddressData
  {
    crypto::PublicKey spendPublicKey;
    crypto::SecretKey spendSecretKey;
    uint64_t creationTimestamp;
  };

  void throwIfNotInitialized() const;
  void throwIfStopped() const;
  void throwIfTrackingMode() const;
  void doShutdown();
  void clearCaches(bool clearTransactions, bool clearCachedData);
  void clearCacheAndShutdown();
  void convertAndLoadWalletFile(const std::string &path, std::ifstream &&walletFileStream);
  size_t getTxSize(const TransactionParameters &sendingTransaction);

  static void decryptKeyPair(const EncryptedWalletRecord& cipher, crypto::PublicKey& publicKey, crypto::SecretKey& secretKey, uint64_t& creationTimestamp, const crypto::chacha8_key& key);
    crypto::chacha8_iv getNextIv() const;

  void decryptKeyPair(const EncryptedWalletRecord& cipher, crypto::PublicKey& publicKey, crypto::SecretKey& secretKey, uint64_t& creationTimestamp) const;
  static EncryptedWalletRecord encryptKeyPair(const crypto::PublicKey& publicKey, const crypto::SecretKey& secretKey, uint64_t creationTimestamp, const crypto::chacha8_key& key, const crypto::chacha8_iv& iv);
  EncryptedWalletRecord encryptKeyPair(const crypto::PublicKey& publicKey, const crypto::SecretKey& secretKey, uint64_t creationTimestamp) const;
  static void incIv(crypto::chacha8_iv& iv);
  void incNextIv();
  void initWithKeys(const std::string& path, const std::string& password, const crypto::PublicKey& viewPublicKey, const crypto::SecretKey& viewSecretKey);
  std::string doCreateAddress(const crypto::PublicKey &spendPublicKey, const crypto::SecretKey &spendSecretKey, uint64_t creationTimestamp);
  std::vector<std::string> doCreateAddressList(const std::vector<NewAddressData> &addressDataList);
  crypto::SecretKey getTransactionDeterministicSecretKey(crypto::Hash &transactionHash) const;

  uint64_t scanHeightToTimestamp(const uint32_t scanHeight);
  uint64_t getCurrentTimestampAdjusted();

  struct InputInfo
  {
    transaction_types::InputKeyInfo keyInfo;
    WalletRecord *walletRecord = nullptr;
    KeyPair ephKeys;
  };

  struct OutputToTransfer
  {
    TransactionOutputInformation out;
    WalletRecord *wallet;
  };

  struct ReceiverAmounts
  {
    cn::AccountPublicAddress receiver;
    std::vector<uint64_t> amounts;
  };

  struct WalletOuts
  {
    WalletRecord *wallet;
    std::vector<TransactionOutputInformation> outs;
  };

  using TransfersRange = std::pair<WalletTransfers::const_iterator, WalletTransfers::const_iterator>;

  struct AddressAmounts
  {
    int64_t input = 0;
    int64_t output = 0;
  };

  struct ContainerAmounts
  {
    ITransfersContainer *container;
    AddressAmounts amounts;
  };

#pragma pack(push, 1)
  struct ContainerStoragePrefix {
    uint8_t version;
    crypto::chacha8_iv nextIv;
    EncryptedWalletRecord encryptedViewKeys;
  };
#pragma pack(pop)

  using TransfersMap = std::unordered_map<std::string, AddressAmounts>;

  void onError(ITransfersSubscription *object, uint32_t height, std::error_code ec) override;

  void onTransactionUpdated(ITransfersSubscription *object, const crypto::Hash &transactionHash) override;
  void onTransactionUpdated(const crypto::PublicKey &viewPublicKey, const crypto::Hash &transactionHash,
                                    const std::vector<ITransfersContainer *> &containers) override;
  void transactionUpdated(TransactionInformation transactionInfo, const std::vector<ContainerAmounts> &containerAmountsList);

  void onTransactionDeleted(ITransfersSubscription *object, const crypto::Hash &transactionHash) override;
  void transactionDeleted(ITransfersSubscription *object, const crypto::Hash &transactionHash);

  void synchronizationProgressUpdated(uint32_t processedBlockCount, uint32_t totalBlockCount) override;
  void synchronizationCompleted(std::error_code result) override;

  void onSynchronizationProgressUpdated(uint32_t processedBlockCount, uint32_t totalBlockCount);
  void onSynchronizationCompleted();

  void onBlocksAdded(const crypto::PublicKey &viewPublicKey, const std::vector<crypto::Hash> &blockHashes) override;
  void blocksAdded(const std::vector<crypto::Hash> &blockHashes);

  void onBlockchainDetach(const crypto::PublicKey &viewPublicKey, uint32_t blockIndex) override;
  void blocksRollback(uint32_t blockIndex);

  void onTransactionDeleteBegin(const crypto::PublicKey &viewPublicKey, crypto::Hash transactionHash) override;
  void transactionDeleteBegin(crypto::Hash transactionHash);

  void onTransactionDeleteEnd(const crypto::PublicKey &viewPublicKey, crypto::Hash transactionHash) override;
  void transactionDeleteEnd(crypto::Hash transactionHash);

  std::vector<WalletOuts> pickWalletsWithMoney() const;
  WalletOuts pickWallet(const std::string &address) const;
  std::vector<WalletOuts> pickWallets(const std::vector<std::string> &addresses) const;

  void updateBalance(cn::ITransfersContainer *container);
  void unlockBalances(uint32_t height);

  const WalletRecord &getWalletRecord(const crypto::PublicKey &key) const;
  const WalletRecord &getWalletRecord(const std::string &address) const;
  const WalletRecord &getWalletRecord(cn::ITransfersContainer *container) const;

  cn::AccountPublicAddress parseAddress(const std::string &address) const;
  std::string addWallet(const crypto::PublicKey &spendPublicKey, const crypto::SecretKey &spendSecretKey, uint64_t creationTimestamp);
  AccountKeys makeAccountKeys(const WalletRecord &wallet) const;
  size_t getTransactionId(const crypto::Hash &transactionHash) const;
  size_t getDepositId(const crypto::Hash &transactionHash) const;
  void pushEvent(const WalletEvent &event);
  bool isFusionTransaction(const WalletTransaction &walletTx) const;

  struct PreparedTransaction
  {
    std::unique_ptr<ITransaction> transaction;
    std::vector<WalletTransfer> destinations;
    uint64_t neededMoney;
    uint64_t changeAmount;
  };

  void prepareTransaction(std::vector<WalletOuts> &&wallets,
                          const std::vector<WalletOrder> &orders,
                          const std::vector<WalletMessage> &messages,
                          uint64_t fee,
                          uint64_t mixIn,
                          const std::string &extra,
                          uint64_t unlockTimestamp,
                          const DonationSettings &donation,
                          const cn::AccountPublicAddress &changeDestinationAddress,
                          PreparedTransaction &preparedTransaction,
                          crypto::SecretKey &transactionSK);
  void validateAddresses(const std::vector<std::string> &addresses) const;
  void validateSourceAddresses(const std::vector<std::string> &sourceAddresses) const;
  void validateChangeDestination(const std::vector<std::string> &sourceAddresses, const std::string &changeDestination, bool isFusion) const;
  void validateOrders(const std::vector<WalletOrder> &orders) const;

  void validateTransactionParameters(const TransactionParameters &transactionParameters) const;
  size_t doTransfer(const TransactionParameters &transactionParameters, crypto::SecretKey &transactionSK);

  void requestMixinOuts(const std::vector<OutputToTransfer> &selectedTransfers,
                        uint64_t mixIn,
                        std::vector<cn::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount> &mixinResult);

  void prepareInputs(const std::vector<OutputToTransfer> &selectedTransfers,
                     std::vector<cn::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount> &mixinResult,
                     uint64_t mixIn,
                     std::vector<InputInfo> &keysInfo);

  uint64_t selectTransfers(uint64_t needeMoney,
                           uint64_t dustThreshold,
                           std::vector<WalletOuts> &&wallets,
                           std::vector<OutputToTransfer> &selectedTransfers);

  std::vector<ReceiverAmounts> splitDestinations(const std::vector<WalletTransfer> &destinations,
                                                 uint64_t dustThreshold, const Currency &currency);
  ReceiverAmounts splitAmount(uint64_t amount, const AccountPublicAddress &destination, uint64_t dustThreshold);

  std::unique_ptr<cn::ITransaction> makeTransaction(const std::vector<ReceiverAmounts> &decomposedOutputs,
                                                            std::vector<InputInfo> &keysInfo, const std::vector<WalletMessage> &messages, const std::string &extra, uint64_t unlockTimestamp, crypto::SecretKey &transactionSK);

  void sendTransaction(const cn::Transaction &cryptoNoteTransaction);
  size_t validateSaveAndSendTransaction(const ITransactionReader &transaction, const std::vector<WalletTransfer> &destinations, bool isFusion, bool send);

  size_t insertBlockchainTransaction(const TransactionInformation &info, int64_t txBalance);
  size_t insertOutgoingTransactionAndPushEvent(const crypto::Hash &transactionHash, uint64_t fee, const BinaryArray &extra, uint64_t unlockTimestamp);
  void updateTransactionStateAndPushEvent(size_t transactionId, WalletTransactionState state);
  bool updateWalletTransactionInfo(size_t transactionId, const cn::TransactionInformation &info, int64_t totalAmount);
  bool updateWalletDepositInfo(size_t depositId, const cn::Deposit &info);



  bool updateTransactionTransfers(size_t transactionId, const std::vector<ContainerAmounts> &containerAmountsList,
                                  int64_t allInputsAmount, int64_t allOutputsAmount);
  TransfersMap getKnownTransfersMap(size_t transactionId, size_t firstTransferIdx) const;
  bool updateAddressTransfers(size_t transactionId, size_t firstTransferIdx, const std::string &address, int64_t knownAmount, int64_t targetAmount);
  bool updateUnknownTransfers(size_t transactionId, size_t firstTransferIdx, const std::unordered_set<std::string> &myAddresses,
                              int64_t knownAmount, int64_t myAmount, int64_t totalAmount, bool isOutput);
  void appendTransfer(size_t transactionId, size_t firstTransferIdx, const std::string &address, int64_t amount);
  bool adjustTransfer(size_t transactionId, size_t firstTransferIdx, const std::string &address, int64_t amount);
  bool eraseTransfers(size_t transactionId, size_t firstTransferIdx, std::function<bool(bool, const std::string &)> &&predicate);
  bool eraseTransfersByAddress(size_t transactionId, size_t firstTransferIdx, const std::string &address, bool eraseOutputTransfers);
  bool eraseForeignTransfers(size_t transactionId, size_t firstTransferIdx, const std::unordered_set<std::string> &knownAddresses, bool eraseOutputTransfers);
  void pushBackOutgoingTransfers(size_t txId, const std::vector<WalletTransfer> &destinations);
  void insertUnlockTransactionJob(const crypto::Hash &transactionHash, uint32_t blockHeight, cn::ITransfersContainer *container);
  void deleteUnlockTransactionJob(const crypto::Hash &transactionHash);
  void startBlockchainSynchronizer();
  void stopBlockchainSynchronizer();
  void addUnconfirmedTransaction(const ITransactionReader &transaction);
  void removeUnconfirmedTransaction(const crypto::Hash &transactionHash);
  void initTransactionPool();
  static void loadAndDecryptContainerData(ContainerStorage& storage, const crypto::chacha8_key& key, BinaryArray& containerData);
  static void encryptAndSaveContainerData(ContainerStorage& storage, const crypto::chacha8_key& key, const void* containerData, size_t containerDataSize);
  void loadWalletCache(std::unordered_set<crypto::PublicKey>& addedKeys, std::unordered_set<crypto::PublicKey>& deletedKeys, std::string& extra);

  void copyContainerStorageKeys(ContainerStorage& src, const crypto::chacha8_key& srcKey, ContainerStorage& dst, const crypto::chacha8_key& dstKey);
  static void copyContainerStoragePrefix(ContainerStorage& src, const crypto::chacha8_key& srcKey, ContainerStorage& dst, const crypto::chacha8_key& dstKey);

  void deleteOrphanTransactions(const std::unordered_set<crypto::PublicKey> &deletedKeys);
  void saveWalletCache(ContainerStorage &storage, const crypto::chacha8_key &key, WalletSaveLevel saveLevel, const std::string &extra);
  void loadSpendKeys();
  void loadContainerStorage(const std::string &path);

  void subscribeWallets();

  std::vector<OutputToTransfer> pickRandomFusionInputs(const std::vector<std::string> &addresses,
                                                       uint64_t threshold, size_t minInputCount, size_t maxInputCount);

  static ReceiverAmounts decomposeFusionOutputs(const AccountPublicAddress &address, uint64_t inputsAmount);

  enum class WalletState
  {
    INITIALIZED,
    NOT_INITIALIZED
  };

  enum class WalletTrackingMode
  {
    TRACKING,
    NOT_TRACKING,
    NO_ADDRESSES
  };

  WalletTrackingMode getTrackingMode() const;

  TransfersRange getTransactionTransfersRange(size_t transactionIndex) const;
  std::vector<TransactionsInBlockInfo> getTransactionsInBlocks(uint32_t blockIndex, size_t count) const;
  std::vector<DepositsInBlockInfo> getDepositsInBlocks(uint32_t blockIndex, size_t count) const;
  crypto::Hash getBlockHashByIndex(uint32_t blockIndex) const;

  std::vector<WalletTransfer> getTransactionTransfers(const WalletTransaction &transaction) const;
  void filterOutTransactions(WalletTransactions &transactions, WalletTransfers &transfers, std::function<bool(const WalletTransaction &)> &&pred) const;
  void initBlockchain(const crypto::PublicKey& viewPublicKey);
  void getViewKeyKnownBlocks(const crypto::PublicKey &viewPublicKey);
  cn::AccountPublicAddress getChangeDestination(const std::string &changeDestinationAddress, const std::vector<std::string> &sourceAddresses) const;
  bool isMyAddress(const std::string &address) const;

  void deleteContainerFromUnlockTransactionJobs(const ITransfersContainer *container);
  std::vector<size_t> deleteTransfersForAddress(const std::string &address, std::vector<size_t> &deletedTransactions);
  void deleteFromUncommitedTransactions(const std::vector<size_t> &deletedTransactions);

private:
  platform_system::Dispatcher &m_dispatcher;
  const Currency &m_currency;
  INode &m_node;
  mutable logging::LoggerRef m_logger;
  bool m_stopped;
  WalletDeposits m_deposits;
  WalletsContainer m_walletsContainer;
  ContainerStorage m_containerStorage;
  UnlockTransactionJobs m_unlockTransactionsJob;
  WalletTransactions m_transactions;
  WalletTransfers m_transfers;                               //sorted
  mutable std::unordered_map<size_t, bool> m_fusionTxsCache; // txIndex -> isFusion
  UncommitedTransactions m_uncommitedTransactions;

  bool m_blockchainSynchronizerStarted;
  BlockchainSynchronizer m_blockchainSynchronizer;
  TransfersSyncronizer m_synchronizer;

  platform_system::Event m_eventOccurred;
  std::queue<WalletEvent> m_events;
  mutable platform_system::Event m_readyEvent;

  WalletState m_state;

  std::string m_password;
  crypto::chacha8_key m_key;
  std::string m_path;
  std::string m_extra; // workaround for wallet reset
  
  crypto::PublicKey m_viewPublicKey;
  crypto::SecretKey m_viewSecretKey;

  uint64_t m_actualBalance;
  uint64_t m_pendingBalance;
  uint64_t m_lockedDepositBalance;
  uint64_t m_unlockedDepositBalance;

  uint64_t m_upperTransactionSizeLimit;
  uint32_t m_transactionSoftLockTime;

  BlockHashesContainer m_blockchain;
};

} //namespace cn

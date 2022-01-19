// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <vector>
#include <unordered_map>
#include <limits>

#include "INode.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "TestBlockchainGenerator.h"
#include "Common/ObserverManager.h"
#include "Wallet/WalletAsyncContextCounter.h"


class INodeDummyStub : public cn::INode
{
public:
  virtual bool addObserver(cn::INodeObserver* observer) override;
  virtual bool removeObserver(cn::INodeObserver* observer) override;

  virtual void init(const cn::INode::Callback& callback) override {callback(std::error_code());};
  virtual bool shutdown() override { return true; };

  virtual size_t getPeerCount() const override { return 0; };
  virtual uint32_t getLastLocalBlockHeight() const override { return 0; };
  virtual uint32_t getLastKnownBlockHeight() const override { return 0; };
  virtual uint32_t getLocalBlockCount() const override { return 0; };
  virtual uint32_t getKnownBlockCount() const override { return 0; };
  virtual uint64_t getLastLocalBlockTimestamp() const override { return 0; }

  virtual void getNewBlocks(std::vector<crypto::Hash>&& knownBlockIds, std::vector<cn::block_complete_entry>& newBlocks, uint32_t& height, const Callback& callback) override { callback(std::error_code()); };

  virtual void relayTransaction(const cn::Transaction& transaction, const Callback& callback) override { callback(std::error_code()); };
  virtual void getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount, std::vector<cn::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback) override { callback(std::error_code()); };
  virtual void getTransactionOutsGlobalIndices(const crypto::Hash& transactionHash, std::vector<uint32_t>& outsGlobalIndices, const Callback& callback) override { callback(std::error_code()); };
  virtual void getPoolSymmetricDifference(std::vector<crypto::Hash>&& known_pool_tx_ids, crypto::Hash known_block_id, bool& is_bc_actual,
          std::vector<std::unique_ptr<cn::ITransactionReader>>& new_txs, std::vector<crypto::Hash>& deleted_tx_ids, const Callback& callback) override {
    is_bc_actual = true; callback(std::error_code());
  };
  virtual void queryBlocks(std::vector<crypto::Hash>&& knownBlockIds, uint64_t timestamp, std::vector<cn::BlockShortEntry>& newBlocks,
          uint32_t& startHeight, const Callback& callback) override { callback(std::error_code()); };

  virtual void getBlocks(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<cn::BlockDetails>>& blocks, const Callback& callback) override { callback(std::error_code()); };
  virtual void getBlocks(const std::vector<crypto::Hash>& blockHashes, std::vector<cn::BlockDetails>& blocks, const Callback& callback) override { callback(std::error_code()); };
  virtual void getBlocks(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<cn::BlockDetails>& blocks, uint32_t& blocksNumberWithinTimestamps, const Callback& callback) override { callback(std::error_code()); };
  virtual void getTransactions(const std::vector<crypto::Hash>& transactionHashes, std::vector<cn::TransactionDetails>& transactions, const Callback& callback) override { callback(std::error_code()); };
  virtual void getTransactionsByPaymentId(const crypto::Hash& paymentId, std::vector<cn::TransactionDetails>& transactions, const Callback& callback) override { callback(std::error_code()); };
  virtual void getPoolTransactions(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<cn::TransactionDetails>& transactions, uint64_t& transactionsNumberWithinTimestamps, const Callback& callback) override { callback(std::error_code()); };
  virtual void isSynchronized(bool& syncStatus, const Callback& callback) override { callback(std::error_code()); };
  virtual void getMultisignatureOutputByGlobalIndex(uint64_t amount, uint32_t gindex, cn::MultisignatureOutput& out, const Callback& callback) override { callback(std::error_code()); }

  void updateObservers();

  tools::ObserverManager<cn::INodeObserver> observerManager;

};

class INodeTrivialRefreshStub : public INodeDummyStub {
public:
  INodeTrivialRefreshStub(TestBlockchainGenerator& generator, bool consumerTests = false) :
    m_lastHeight(1), m_blockchainGenerator(generator), m_nextTxError(false), m_getMaxBlocks(std::numeric_limits<size_t>::max()), m_nextTxToPool(false), m_synchronized(false), consumerTests(consumerTests) {
  };

  void setGetNewBlocksLimit(size_t maxBlocks) { m_getMaxBlocks = maxBlocks; }

  virtual uint32_t getLastLocalBlockHeight() const override { return static_cast<uint32_t>(m_blockchainGenerator.getBlockchain().size() - 1); }
  virtual uint32_t getLastKnownBlockHeight() const override { return static_cast<uint32_t>(m_blockchainGenerator.getBlockchain().size() - 1); }

  virtual uint32_t getLocalBlockCount() const override { return static_cast<uint32_t>(m_blockchainGenerator.getBlockchain().size()); }
  virtual uint32_t getKnownBlockCount() const override { return static_cast<uint32_t>(m_blockchainGenerator.getBlockchain().size()); }

  virtual void getNewBlocks(std::vector<crypto::Hash>&& knownBlockIds, std::vector<cn::block_complete_entry>& newBlocks, uint32_t& startHeight, const Callback& callback) override;

  virtual void relayTransaction(const cn::Transaction& transaction, const Callback& callback) override;
  virtual void getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount, std::vector<cn::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback) override;
  virtual void getTransactionOutsGlobalIndices(const crypto::Hash& transactionHash, std::vector<uint32_t>& outsGlobalIndices, const Callback& callback) override;
  virtual void queryBlocks(std::vector<crypto::Hash>&& knownBlockIds, uint64_t timestamp, std::vector<cn::BlockShortEntry>& newBlocks, uint32_t& startHeight, const Callback& callback) override;
  virtual void getPoolSymmetricDifference(std::vector<crypto::Hash>&& known_pool_tx_ids, crypto::Hash known_block_id, bool& is_bc_actual,
          std::vector<std::unique_ptr<cn::ITransactionReader>>& new_txs, std::vector<crypto::Hash>& deleted_tx_ids, const Callback& callback) override;

  virtual void getBlocks(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<cn::BlockDetails>>& blocks, const Callback& callback) override;
  virtual void getBlocks(const std::vector<crypto::Hash>& blockHashes, std::vector<cn::BlockDetails>& blocks, const Callback& callback) override;
  virtual void getBlocks(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<cn::BlockDetails>& blocks, uint32_t& blocksNumberWithinTimestamps, const Callback& callback) override;
  virtual void getTransactions(const std::vector<crypto::Hash>& transactionHashes, std::vector<cn::TransactionDetails>& transactions, const Callback& callback) override;
  virtual void getTransactionsByPaymentId(const crypto::Hash& paymentId, std::vector<cn::TransactionDetails>& transactions, const Callback& callback)  override;
  virtual void getPoolTransactions(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<cn::TransactionDetails>& transactions, uint64_t& transactionsNumberWithinTimestamps, const Callback& callback)  override;
  virtual void isSynchronized(bool& syncStatus, const Callback& callback) override;
  virtual void getMultisignatureOutputByGlobalIndex(uint64_t amount, uint32_t gindex, cn::MultisignatureOutput& out, const Callback& callback) override;


  virtual void startAlternativeChain(uint32_t height);
  void setNextTransactionError();
  void setNextTransactionToPool();
  void cleanTransactionPool();
  void setMaxMixinCount(uint64_t maxMixin);
  void includeTransactionsFromPoolToBlock();

  void setSynchronizedStatus(bool status);

  void sendPoolChanged();
  void sendLocalBlockchainUpdated();

  std::vector<crypto::Hash> calls_getTransactionOutsGlobalIndices;

  virtual ~INodeTrivialRefreshStub();

  std::function<void(const crypto::Hash&, std::vector<uint32_t>&)> getGlobalOutsFunctor = [](const crypto::Hash&, std::vector<uint32_t>&) {};

  void waitForAsyncContexts();

protected:
  void doGetNewBlocks(std::vector<crypto::Hash> knownBlockIds, std::vector<cn::block_complete_entry>& newBlocks,
          uint32_t& startHeight, std::vector<cn::Block> blockchain, const Callback& callback);
  void doGetTransactionOutsGlobalIndices(const crypto::Hash& transactionHash, std::vector<uint32_t>& outsGlobalIndices, const Callback& callback);
  void doRelayTransaction(const cn::Transaction& transaction, const Callback& callback);
  void doGetRandomOutsByAmounts(std::vector<uint64_t> amounts, uint64_t outsCount, std::vector<cn::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback);
  void doGetPoolSymmetricDifference(std::vector<crypto::Hash>&& known_pool_tx_ids, crypto::Hash known_block_id, bool& is_bc_actual,
          std::vector<std::unique_ptr<cn::ITransactionReader>>& new_txs, std::vector<crypto::Hash>& deleted_tx_ids, const Callback& callback);

  void doGetBlocks(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<cn::BlockDetails>>& blocks, const Callback& callback);
  void doGetBlocks(const std::vector<crypto::Hash>& blockHashes, std::vector<cn::BlockDetails>& blocks, const Callback& callback);
  void doGetBlocks(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<cn::BlockDetails>& blocks, uint32_t& blocksNumberWithinTimestamps, const Callback& callback);
  void doGetTransactions(const std::vector<crypto::Hash>& transactionHashes, std::vector<cn::TransactionDetails>& transactions, const Callback& callback);
  void doGetPoolTransactions(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<cn::TransactionDetails>& transactions, uint64_t& transactionsNumberWithinTimestamps, const Callback& callback);
  void doGetTransactionsByPaymentId(const crypto::Hash& paymentId, std::vector<cn::TransactionDetails>& transactions, const Callback& callback);
  void doGetOutByMSigGIndex(uint64_t amount, uint32_t gindex, cn::MultisignatureOutput& out, const Callback& callback);


  size_t m_getMaxBlocks;
  uint32_t m_lastHeight;
  TestBlockchainGenerator& m_blockchainGenerator;
  bool m_nextTxError;
  bool m_nextTxToPool;
  std::mutex m_walletLock;
  cn::WalletAsyncContextCounter m_asyncCounter;
  uint64_t m_maxMixin = std::numeric_limits<uint64_t>::max();
  bool m_synchronized;
  bool consumerTests;
};


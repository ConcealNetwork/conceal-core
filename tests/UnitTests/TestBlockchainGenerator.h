// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <vector>
#include <unordered_map>

#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/BlockchainIndices.h"
#include "crypto/hash.h"

#include "../TestGenerator/TestGenerator.h"

class TestBlockchainGenerator
{
public:
  TestBlockchainGenerator(const cn::Currency& currency);

  //TODO: get rid of this method
  std::vector<cn::Block>& getBlockchain();
  std::vector<cn::Block> getBlockchainCopy();
  void generateEmptyBlocks(size_t count);
  bool getBlockRewardForAddress(const cn::AccountPublicAddress& address);
  bool generateTransactionsInOneBlock(const cn::AccountPublicAddress& address, size_t n);
  bool getSingleOutputTransaction(const cn::AccountPublicAddress& address, uint64_t amount);
  void addTxToBlockchain(const cn::Transaction& transaction);
  bool getTransactionByHash(const crypto::Hash& hash, cn::Transaction& tx, bool checkTxPool = false);
  const cn::AccountBase& getMinerAccount() const;
  bool generateFromBaseTx(const cn::AccountBase& address);

  void putTxToPool(const cn::Transaction& tx);
  void getPoolSymmetricDifference(std::vector<crypto::Hash>&& known_pool_tx_ids, crypto::Hash known_block_id, bool& is_bc_actual,
    std::vector<cn::Transaction>& new_txs, std::vector<crypto::Hash>& deleted_tx_ids);
  void putTxPoolToBlockchain();
  void clearTxPool();

  void cutBlockchain(uint32_t height);

  bool addOrphan(const crypto::Hash& hash, uint32_t height);
  bool getGeneratedTransactionsNumber(uint32_t height, uint64_t& generatedTransactions);
  bool getOrphanBlockIdsByHeight(uint32_t height, std::vector<crypto::Hash>& blockHashes);
  bool getBlockIdsByTimestamp(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<crypto::Hash>& hashes, uint32_t& blocksNumberWithinTimestamps);
  bool getPoolTransactionIdsByTimestamp(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<crypto::Hash>& hashes, uint64_t& transactionsNumberWithinTimestamps);
  bool getTransactionIdsByPaymentId(const crypto::Hash& paymentId, std::vector<crypto::Hash>& transactionHashes);
  uint32_t getCurrentHeight() const { return static_cast<uint32_t>(m_blockchain.size()) - 1; }

  bool getTransactionGlobalIndexesByHash(const crypto::Hash& transactionHash, std::vector<uint32_t>& globalIndexes);
  bool getMultisignatureOutputByGlobalIndex(uint64_t amount, uint32_t globalIndex, cn::MultisignatureOutput& out);
  void setMinerAccount(const cn::AccountBase& account);

private:
  struct MultisignatureOutEntry {
    crypto::Hash transactionHash;
    uint16_t indexOut;
  };

  struct KeyOutEntry {
    crypto::Hash transactionHash;
    uint16_t indexOut;
  };
  
  void addGenesisBlock();
  void addMiningBlock();

  const cn::Currency& m_currency;
  test_generator generator;
  cn::AccountBase miner_acc;
  std::vector<cn::Block> m_blockchain;
  std::unordered_map<crypto::Hash, cn::Transaction> m_txs;
  std::unordered_map<crypto::Hash, std::vector<uint32_t>> transactionGlobalOuts;
  std::unordered_map<uint64_t, std::vector<MultisignatureOutEntry>> multisignatureOutsIndex;
  std::unordered_map<uint64_t, std::vector<KeyOutEntry>> keyOutsIndex;

  std::unordered_map<crypto::Hash, cn::Transaction> m_txPool;
  mutable std::mutex m_mutex;

  cn::PaymentIdIndex m_paymentIdIndex;
  cn::TimestampTransactionsIndex m_timestampIndex;
  cn::GeneratedTransactionsIndex m_generatedTransactionsIndex;
  cn::OrphanBlocksIndex m_orthanBlocksIndex;

  void addToBlockchain(const cn::Transaction& tx);
  void addToBlockchain(const std::vector<cn::Transaction>& txs);
  void addToBlockchain(const std::vector<cn::Transaction>& txs, const cn::AccountBase& minerAddress);
  void addTx(const cn::Transaction& tx);

  bool doGenerateTransactionsInOneBlock(cn::AccountPublicAddress const &address, size_t n);
};

// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <unordered_map>

#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "CryptoNoteCore/ICore.h"
#include "CryptoNoteCore/ICoreObserver.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolDefinitions.h"
#include "Logging/ConsoleLogger.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"

class ICoreStub: public cn::ICore {
public:
  ICoreStub();
  ICoreStub(const cn::Block& genesisBlock);

  virtual const cn::Currency& currency() const override;

  virtual bool addObserver(cn::ICoreObserver* observer) override;
  virtual bool removeObserver(cn::ICoreObserver* observer) override;
  virtual void get_blockchain_top(uint32_t& height, crypto::Hash& top_id) override;
  virtual std::vector<crypto::Hash> findBlockchainSupplement(const std::vector<crypto::Hash>& remoteBlockIds, size_t maxCount,
    uint32_t& totalBlockCount, uint32_t& startBlockIndex) override;
  virtual bool get_random_outs_for_amounts(const cn::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_request& req,
      cn::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response& res) override;
  virtual bool get_tx_outputs_gindexs(const crypto::Hash& tx_id, std::vector<uint32_t>& indexs) override;
  virtual cn::i_cryptonote_protocol* get_protocol() override;
  virtual bool handle_incoming_tx(cn::BinaryArray const& tx_blob, cn::tx_verification_context& tvc, bool keeped_by_block) override;
  virtual std::vector<cn::Transaction> getPoolTransactions() override;
  virtual bool getPoolChanges(const crypto::Hash& tailBlockId, const std::vector<crypto::Hash>& knownTxsIds,
                              std::vector<cn::Transaction>& addedTxs, std::vector<crypto::Hash>& deletedTxsIds) override;
  virtual bool getPoolChangesLite(const crypto::Hash& tailBlockId, const std::vector<crypto::Hash>& knownTxsIds,
          std::vector<cn::TransactionPrefixInfo>& addedTxs, std::vector<crypto::Hash>& deletedTxsIds) override;
  virtual void getPoolChanges(const std::vector<crypto::Hash>& knownTxsIds, std::vector<cn::Transaction>& addedTxs,
                              std::vector<crypto::Hash>& deletedTxsIds) override;
  virtual bool queryBlocks(const std::vector<crypto::Hash>& block_ids, uint64_t timestamp,
    uint32_t& start_height, uint32_t& current_height, uint32_t& full_offset, std::vector<cn::BlockFullInfo>& entries) override;
  virtual bool queryBlocksLite(const std::vector<crypto::Hash>& block_ids, uint64_t timestamp,
    uint32_t& start_height, uint32_t& current_height, uint32_t& full_offset, std::vector<cn::BlockShortInfo>& entries) override;

  virtual bool have_block(const crypto::Hash& id) override;
  std::vector<crypto::Hash> buildSparseChain() override;
  std::vector<crypto::Hash> buildSparseChain(const crypto::Hash& startBlockId) override;
  virtual bool get_stat_info(cn::core_stat_info& st_inf) override { return false; }
  virtual bool on_idle() override { return false; }
  virtual void pause_mining() override {}
  virtual void update_block_template_and_resume_mining() override {}
  virtual bool handle_incoming_block_blob(const cn::BinaryArray& block_blob, cn::block_verification_context& bvc, bool control_miner, bool relay_block) override { return false; }
  virtual bool handle_get_objects(cn::NOTIFY_REQUEST_GET_OBJECTS::request& arg, cn::NOTIFY_RESPONSE_GET_OBJECTS::request& rsp) override { return false; }
  virtual void on_synchronized() override {}
  virtual bool getOutByMSigGIndex(uint64_t amount, uint64_t gindex, cn::MultisignatureOutput& out) override { return true; }
  virtual size_t addChain(const std::vector<const cn::IBlock*>& chain) override;

  virtual crypto::Hash getBlockIdByHeight(uint32_t height) override;
  virtual bool getBlockByHash(const crypto::Hash &h, cn::Block &blk) override;
  virtual bool getBlockHeight(const crypto::Hash& blockId, uint32_t& blockHeight) override;
  virtual void getTransactions(const std::vector<crypto::Hash>& txs_ids, std::list<cn::Transaction>& txs, std::list<crypto::Hash>& missed_txs, bool checkTxPool = false) override;
  virtual bool getBackwardBlocksSizes(uint32_t fromHeight, std::vector<size_t>& sizes, size_t count) override;
  virtual bool getBlockSize(const crypto::Hash& hash, size_t& size) override;
  virtual bool getAlreadyGeneratedCoins(const crypto::Hash& hash, uint64_t& generatedCoins) override;
  virtual bool getBlockReward(size_t medianSize, size_t currentBlockSize, uint64_t alreadyGeneratedCoins, uint64_t fee, uint32_t height,
      uint64_t& reward, int64_t& emissionChange) override;
  virtual bool scanOutputkeysForIndices(const cn::KeyInput& txInToKey, std::list<std::pair<crypto::Hash, size_t>>& outputReferences) override;
  virtual bool getBlockDifficulty(uint32_t height, cn::difficulty_type& difficulty) override;
  virtual bool getBlockContainingTx(const crypto::Hash& txId, crypto::Hash& blockId, uint32_t& blockHeight) override;
  virtual bool getMultisigOutputReference(const cn::MultisignatureInput& txInMultisig, std::pair<crypto::Hash, size_t>& outputReference) override;

  virtual bool getGeneratedTransactionsNumber(uint32_t height, uint64_t& generatedTransactions) override;
  virtual bool getOrphanBlocksByHeight(uint32_t height, std::vector<cn::Block>& blocks) override;
  virtual bool getBlocksByTimestamp(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<cn::Block>& blocks, uint32_t& blocksNumberWithinTimestamps) override;
  virtual bool getPoolTransactionsByTimestamp(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<cn::Transaction>& transactions, uint64_t& transactionsNumberWithinTimestamps) override;
  virtual bool getTransactionsByPaymentId(const crypto::Hash& paymentId, std::vector<cn::Transaction>& transactions) override;
  virtual std::unique_ptr<cn::IBlock> getBlock(const crypto::Hash& blockId) override;
  virtual bool handleIncomingTransaction(const cn::Transaction& tx, const crypto::Hash& txHash, size_t blobSize, cn::tx_verification_context& tvc, bool keptByBlock, uint32_t height) override;
  virtual std::error_code executeLocked(const std::function<std::error_code()>& func) override;

  virtual bool addMessageQueue(cn::MessageQueue<cn::BlockchainMessage>& messageQueuePtr) override;
  virtual bool removeMessageQueue(cn::MessageQueue<cn::BlockchainMessage>& messageQueuePtr) override;


  void set_blockchain_top(uint32_t height, const crypto::Hash& top_id);
  void set_outputs_gindexs(const std::vector<uint32_t>& indexs, bool result);
  void set_random_outs(const cn::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response& resp, bool result);

  void addBlock(const cn::Block& block);
  void addTransaction(const cn::Transaction& tx);

  void setPoolTxVerificationResult(bool result);
  void setPoolChangesResult(bool result);

private:
  logging::ConsoleLogger m_logger;
  cn::Currency m_currency;

  uint32_t topHeight;
  crypto::Hash topId;

  std::vector<uint32_t> globalIndices;
  bool globalIndicesResult;

  cn::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response randomOuts;
  bool randomOutsResult;

  std::unordered_map<crypto::Hash, cn::Block> blocks;
  std::unordered_map<uint32_t, crypto::Hash> blockHashByHeightIndex;
  std::unordered_map<crypto::Hash, crypto::Hash> blockHashByTxHashIndex;

  std::unordered_map<crypto::Hash, cn::Transaction> transactions;
  std::unordered_map<crypto::Hash, cn::Transaction> transactionPool;
  bool poolTxVerificationResult;
  bool poolChangesResult;
};

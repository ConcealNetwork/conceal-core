// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2020 Karbo developers
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <atomic>

#include <parallel_hashmap/phmap.h>

#include "Common/ObserverManager.h"
#include "Common/Util.h"
#include "CryptoNoteCore/BlockIndex.h"
#include "CryptoNoteCore/Checkpoints.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/DepositIndex.h"
#include "CryptoNoteCore/IBlockchainStorageObserver.h"
#include "CryptoNoteCore/ITransactionValidator.h"
#include "CryptoNoteCore/SwappedVector.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/TransactionPool.h"
#include "CryptoNoteCore/BlockchainIndices.h"
#include "CryptoNoteCore/UpgradeDetector.h"

#include "CryptoNoteCore/MessageQueue.h"
#include "CryptoNoteCore/BlockchainMessages.h"
#include "CryptoNoteCore/IntrusiveLinkedList.h"

#include <Logging/LoggerRef.h>

#undef ERROR
using phmap::parallel_flat_hash_map;
namespace cn
{
  struct NOTIFY_REQUEST_GET_OBJECTS_request;
  struct NOTIFY_RESPONSE_GET_OBJECTS_request;
  struct COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_request;
  struct COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response;
  struct COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_outs_for_amount;

  using cn::BlockInfo;
  class Blockchain : public cn::ITransactionValidator
  {
  public:
    Blockchain(const Currency &currency, tx_memory_pool &tx_pool, logging::ILogger &logger, bool blockchainIndexesEnabled, bool blockchainAutosaveEnabled);

    bool addObserver(IBlockchainStorageObserver *observer);
    bool removeObserver(IBlockchainStorageObserver *observer);

    void rebuildCache();
    bool rebuildBlocks();
    bool storeCache();

    // ITransactionValidator
    bool checkTransactionInputs(const cn::Transaction &tx, BlockInfo &maxUsedBlock) override;
    bool checkTransactionInputs(const cn::Transaction &tx, BlockInfo &maxUsedBlock, BlockInfo &lastFailed) override;
    bool haveSpentKeyImages(const cn::Transaction &tx) override;
    bool checkTransactionSize(size_t blobSize) override;

    bool init() { return init(tools::getDefaultDataDirectory(), true, m_testnet); }
    bool init(const std::string &config_folder, bool load_existing, bool testnet);
    bool deinit();

    bool getLowerBound(uint64_t timestamp, uint64_t startOffset, uint32_t &height);
    std::vector<crypto::Hash> getBlockIds(uint32_t startHeight, uint32_t maxCount);

    void setCheckpoints(Checkpoints &&chk_pts) { m_checkpoints = std::move(chk_pts); }
    bool getBlocks(uint32_t start_offset, uint32_t count, std::list<Block> &blocks, std::list<Transaction> &txs);
    bool getBlocks(uint32_t start_offset, uint32_t count, std::list<Block> &blocks);
    bool getAlternativeBlocks(std::list<Block> &blocks);
    bool getTransactionsWithOutputGlobalIndexes(const std::vector<crypto::Hash>& txs_ids, std::list<crypto::Hash>& missed_txs, std::vector<std::pair<Transaction, std::vector<uint32_t>>>& txs);
    uint32_t getAlternativeBlocksCount();
    crypto::Hash getBlockIdByHeight(uint32_t height);
    bool getBlockByHash(const crypto::Hash &h, Block &blk);
    bool getBlockHeight(const crypto::Hash &blockId, uint32_t &blockHeight);

    template <class archive_t>
    void serialize(archive_t &ar, const unsigned int version);

    bool haveTransaction(const crypto::Hash &id);
    bool haveTransactionKeyImagesAsSpent(const Transaction &tx);

    uint32_t getCurrentBlockchainHeight(); // TODO rename to getCurrentBlockchainSize
    crypto::Hash getTailId();
    crypto::Hash getTailId(uint32_t &height);
    difficulty_type getDifficultyForNextBlock();
    uint64_t getBlockTimestamp(uint32_t height); // k0x001
    uint64_t getCoinsInCirculation();
    uint8_t get_block_major_version_for_height(uint64_t height) const;
    bool addNewBlock(const Block &bl_, block_verification_context &bvc);
    bool resetAndSetGenesisBlock(const Block &b);
    bool haveBlock(const crypto::Hash &id);
    size_t getTotalTransactions();
    std::vector<crypto::Hash> buildSparseChain();
    std::vector<crypto::Hash> buildSparseChain(const crypto::Hash &startBlockId);
    uint32_t findBlockchainSupplement(const std::vector<crypto::Hash> &qblock_ids); // !!!!
    std::vector<crypto::Hash> findBlockchainSupplement(const std::vector<crypto::Hash> &remoteBlockIds, size_t maxCount,
                                                       uint32_t &totalBlockCount, uint32_t &startBlockIndex);
    uint8_t getBlockMajorVersionForHeight(uint32_t height) const;
    uint8_t blockMajorVersion;
    bool handleGetObjects(NOTIFY_REQUEST_GET_OBJECTS_request &arg, NOTIFY_RESPONSE_GET_OBJECTS_request &rsp); //Deprecated. Should be removed with CryptoNoteProtocolHandler.
    bool getRandomOutsByAmount(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_request &req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response &res);
    bool getBackwardBlocksSize(size_t from_height, std::vector<size_t> &sz, size_t count);
    bool getTransactionOutputGlobalIndexes(const crypto::Hash &tx_id, std::vector<uint32_t> &indexs);
    bool get_out_by_msig_gindex(uint64_t amount, uint64_t gindex, MultisignatureOutput &out);
    bool checkTransactionInputs(const Transaction &tx, uint32_t &pmax_used_block_height, crypto::Hash &max_used_block_id, BlockInfo *tail = nullptr);
    uint64_t getCurrentCumulativeBlocksizeLimit() const;
    uint64_t blockDifficulty(size_t i);
    bool getBlockContainingTransaction(const crypto::Hash &txId, crypto::Hash &blockId, uint32_t &blockHeight);
    bool getAlreadyGeneratedCoins(const crypto::Hash &hash, uint64_t &generatedCoins);
    bool getBlockSize(const crypto::Hash &hash, size_t &size);
    bool getMultisigOutputReference(const MultisignatureInput &txInMultisig, std::pair<crypto::Hash, size_t> &outputReference);
    bool getGeneratedTransactionsNumber(uint32_t height, uint64_t &generatedTransactions);
    bool getOrphanBlockIdsByHeight(uint32_t height, std::vector<crypto::Hash> &blockHashes);
    bool getBlockIdsByTimestamp(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<crypto::Hash> &hashes, uint32_t &blocksNumberWithinTimestamps);
    bool getTransactionIdsByPaymentId(const crypto::Hash &paymentId, std::vector<crypto::Hash> &transactionHashes);
    bool isBlockInMainChain(const crypto::Hash &blockId) const;
    uint64_t fullDepositAmount() const;
    uint64_t depositAmountAtHeight(size_t height) const;
    uint64_t depositInterestAtHeight(size_t height) const;
    uint64_t coinsEmittedAtHeight(uint64_t height);
    uint64_t difficultyAtHeight(uint64_t height);
    bool isInCheckpointZone(const uint32_t height) const;

    template <class visitor_t>
    bool scanOutputKeysForIndexes(const KeyInput &tx_in_to_key, visitor_t &vis, uint32_t *pmax_related_block_height = nullptr);

    bool addMessageQueue(MessageQueue<BlockchainMessage> &messageQueue);
    bool removeMessageQueue(MessageQueue<BlockchainMessage> &messageQueue);

    bool check_tx_outputs(const Transaction &tx, uint32_t height) const;

    template <class t_ids_container, class t_blocks_container, class t_missed_container>
    bool getBlocks(const t_ids_container &block_ids, t_blocks_container &blocks, t_missed_container &missed_bs)
    {
      std::lock_guard<std::recursive_mutex> lk(m_blockchain_lock);

      for (const auto &bl_id : block_ids)
      {
        uint32_t height = 0;
        if (!m_blockIndex.getBlockHeight(bl_id, height))
        {
          missed_bs.push_back(bl_id);
        }
        else
        {
          if (!(height < m_blocks.size()))
          {
            logger(logging::ERROR, logging::BRIGHT_RED) << "Internal error: bl_id=" << common::podToHex(bl_id)
                                                        << " have index record with offset=" << height << ", bigger then m_blocks.size()=" << m_blocks.size();
            return false;
          }
          blocks.push_back(m_blocks[height].bl);
        }
      }

      return true;
    }

    template <class t_ids_container, class t_tx_container, class t_missed_container>
    void getBlockchainTransactions(const t_ids_container &txs_ids, t_tx_container &txs, t_missed_container &missed_txs)
    {
      std::lock_guard<decltype(m_blockchain_lock)> bcLock(m_blockchain_lock);

      for (const auto &tx_id : txs_ids)
      {
        auto it = m_transactionMap.find(tx_id);
        if (it == m_transactionMap.end())
        {
          missed_txs.push_back(tx_id);
        }
        else
        {
          txs.push_back(transactionByIndex(it->second).tx);
        }
      }
    }

    template <class t_ids_container, class t_tx_container, class t_missed_container>
    void getTransactions(const t_ids_container &txs_ids, t_tx_container &txs, t_missed_container &missed_txs, bool checkTxPool = false)
    {
      if (checkTxPool)
      {
        std::lock_guard<decltype(m_tx_pool)> txLock(m_tx_pool);

        getBlockchainTransactions(txs_ids, txs, missed_txs);

        auto poolTxIds = std::move(missed_txs);
        missed_txs.clear();
        m_tx_pool.getTransactions(poolTxIds, txs, missed_txs);
      }
      else
      {
        getBlockchainTransactions(txs_ids, txs, missed_txs);
      }
    }

    //debug functions
    void print_blockchain(uint64_t start_index, uint64_t end_index);
    void print_blockchain_index(bool print_all);
    void print_blockchain_outs(const std::string &file);

    struct TransactionIndex
    {
      uint32_t block;
      uint16_t transaction;

      void serialize(ISerializer &s)
      {
        s(block, "block");
        s(transaction, "tx");
      }
    };

    bool rollbackBlockchainTo(uint32_t height);
    bool have_tx_keyimg_as_spent(const crypto::KeyImage &key_im);

  private:
    bool m_testnet = false;
    struct MultisignatureOutputUsage
    {
      TransactionIndex transactionIndex;
      uint16_t outputIndex;
      bool isUsed;

      void serialize(ISerializer &s)
      {
        s(transactionIndex, "txindex");
        s(outputIndex, "outindex");
        s(isUsed, "used");
      }
    };

    struct TransactionEntry
    {
      Transaction tx;
      std::vector<uint32_t> m_global_output_indexes;

      void serialize(ISerializer &s)
      {
        s(tx, "tx");
        s(m_global_output_indexes, "indexes");
      }
    };

    struct BlockEntry
    {
      Block bl;
      uint32_t height;
      uint64_t block_cumulative_size;
      difficulty_type cumulative_difficulty;
      uint64_t already_generated_coins;
      std::vector<TransactionEntry> transactions;

      void serialize(ISerializer &s)
      {
        s(bl, "block");
        s(height, "height");
        s(block_cumulative_size, "block_cumulative_size");
        s(cumulative_difficulty, "cumulative_difficulty");
        s(already_generated_coins, "already_generated_coins");
        s(transactions, "transactions");
      }
    };

    using key_images_container = parallel_flat_hash_map<crypto::KeyImage, uint32_t>;
    using blocks_ext_by_hash = parallel_flat_hash_map<crypto::Hash, BlockEntry>;
    using outputs_container = parallel_flat_hash_map<uint64_t, std::vector<std::pair<TransactionIndex, uint16_t>>>; //crypto::Hash - tx hash, size_t - index of out in transaction
    using MultisignatureOutputsContainer = parallel_flat_hash_map<uint64_t, std::vector<MultisignatureOutputUsage>>;

    const Currency &m_currency;
    tx_memory_pool &m_tx_pool;
    mutable std::recursive_mutex m_blockchain_lock; // TODO: add here reader/writer lock
    crypto::cn_context m_cn_context;
    tools::ObserverManager<IBlockchainStorageObserver> m_observerManager;

    key_images_container m_spent_keys;
    size_t m_current_block_cumul_sz_limit = 0;
    blocks_ext_by_hash m_alternative_chains; // crypto::Hash -> block_extended_info
    outputs_container m_outputs;

    std::string m_config_folder;
    Checkpoints m_checkpoints;
    std::atomic<bool> m_is_in_checkpoint_zone;

    using Blocks = SwappedVector<BlockEntry>;
    using BlockMap = parallel_flat_hash_map<crypto::Hash, uint32_t>;
    using TransactionMap = parallel_flat_hash_map<crypto::Hash, TransactionIndex>;
    using UpgradeDetector = BasicUpgradeDetector<Blocks>;

    friend class BlockCacheSerializer;
    friend class BlockchainIndicesSerializer;

    Blocks m_blocks;
    cn::BlockIndex m_blockIndex;
    cn::DepositIndex m_depositIndex;
    TransactionMap m_transactionMap;
    MultisignatureOutputsContainer m_multisignatureOutputs;
    UpgradeDetector m_upgradeDetectorV2;
    UpgradeDetector m_upgradeDetectorV3;
    UpgradeDetector m_upgradeDetectorV4;
    UpgradeDetector m_upgradeDetectorV7;
    UpgradeDetector m_upgradeDetectorV8;

    bool m_blockchainIndexesEnabled;
    bool m_blockchainAutosaveEnabled;
    PaymentIdIndex m_paymentIdIndex;
    TimestampBlocksIndex m_timestampIndex;
    GeneratedTransactionsIndex m_generatedTransactionsIndex;
    OrphanBlocksIndex m_orthanBlocksIndex;

    IntrusiveLinkedList<MessageQueue<BlockchainMessage>> m_messageQueueList;

    logging::LoggerRef logger;


    bool switch_to_alternative_blockchain(const std::list<crypto::Hash> &alt_chain, bool discard_disconnected_chain);
    bool handle_alternative_block(const Block &b, const crypto::Hash &id, block_verification_context &bvc, bool sendNewAlternativeBlockMessage = true);
    difficulty_type get_next_difficulty_for_alternative_chain(const std::list<crypto::Hash> &alt_chain, const BlockEntry &bei);
    void pushToDepositIndex(const BlockEntry &block, uint64_t interest);
    bool prevalidate_miner_transaction(const Block &b, uint32_t height) const;
    bool validate_miner_transaction(const Block &b, uint32_t height, size_t cumulativeBlockSize, uint64_t alreadyGeneratedCoins, uint64_t fee, uint64_t &reward, int64_t &emissionChange);
    bool rollback_blockchain_switching(const std::list<Block> &original_chain, size_t rollback_height);
    bool get_last_n_blocks_sizes(std::vector<size_t> &sz, size_t count);
    bool add_out_to_get_random_outs(std::vector<std::pair<TransactionIndex, uint16_t>> &amount_outs, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_outs_for_amount &result_outs, uint64_t amount, size_t i);
    bool is_tx_spendtime_unlocked(uint64_t unlock_time);
    size_t find_end_of_allowed_index(const std::vector<std::pair<TransactionIndex, uint16_t>> &amount_outs);
    bool check_block_timestamp_main(const Block &b);
    bool check_block_timestamp(std::vector<uint64_t> timestamps, const Block &b);
    uint64_t get_adjusted_time() const;
    bool complete_timestamps_vector(uint64_t start_height, std::vector<uint64_t> &timestamps);
    bool checkBlockVersion(const Block &b, const crypto::Hash &blockHash);
    bool checkCumulativeBlockSize(const crypto::Hash &blockId, size_t cumulativeBlockSize, uint64_t height);
    std::vector<crypto::Hash> doBuildSparseChain(const crypto::Hash &startBlockId) const;
    bool getBlockCumulativeSize(const Block &block, size_t &cumulativeSize);
    bool update_next_comulative_size_limit();
    bool check_tx_input(const KeyInput &txin, const crypto::Hash &tx_prefix_hash, const std::vector<crypto::Signature> &sig, uint32_t *pmax_related_block_height = nullptr);
    bool checkTransactionInputs(const Transaction &tx, const crypto::Hash &tx_prefix_hash, uint32_t *pmax_used_block_height = nullptr);
    bool checkTransactionInputs(const Transaction &tx, uint32_t *pmax_used_block_height = nullptr);

    const TransactionEntry &transactionByIndex(TransactionIndex index);
    bool pushBlock(const Block &blockData, const crypto::Hash &id, block_verification_context &bvc, uint32_t height);
    bool pushBlock(const Block &blockData, const std::vector<Transaction> &transactions, const crypto::Hash &id, block_verification_context &bvc);
    bool pushBlock(const BlockEntry &block);
    void popBlock(const crypto::Hash &blockHash);
    bool pushTransaction(BlockEntry &block, const crypto::Hash &transactionHash, TransactionIndex transactionIndex);
    void popTransaction(const Transaction &transaction, const crypto::Hash &transactionHash);
    void popTransactions(const BlockEntry &block, const crypto::Hash &minerTransactionHash);
    bool validateInput(const MultisignatureInput &input, const crypto::Hash &transactionHash, const crypto::Hash &transactionPrefixHash, const std::vector<crypto::Signature> &transactionSignatures);
    bool removeLastBlock();
    bool checkCheckpoints(uint32_t &lastValidCheckpointHeight);
    bool storeBlockchainIndices();
    bool loadBlockchainIndices();

    bool loadTransactions(const Block &block, std::vector<Transaction> &transactions, uint32_t height);
    void saveTransactions(const std::vector<Transaction> &transactions, uint32_t height);

    void sendMessage(const BlockchainMessage &message);

    friend class LockedBlockchainStorage;
  };

  class LockedBlockchainStorage : private boost::noncopyable
  {
  public:
    explicit LockedBlockchainStorage(Blockchain &bc)
        : m_bc(bc), m_lock(bc.m_blockchain_lock) {}

    Blockchain *operator->()
    {
      return &m_bc;
    }

  private:
    Blockchain &m_bc;
    std::lock_guard<std::recursive_mutex> m_lock;
  };

  template <class visitor_t>
  bool Blockchain::scanOutputKeysForIndexes(const KeyInput &tx_in_to_key, visitor_t &vis, uint32_t *pmax_related_block_height)
  {
    std::lock_guard<std::recursive_mutex> lk(m_blockchain_lock);
    auto it = m_outputs.find(tx_in_to_key.amount);
    if (it == m_outputs.end() || !tx_in_to_key.outputIndexes.size())
      return false;

    std::vector<uint32_t> absolute_offsets = relative_output_offsets_to_absolute(tx_in_to_key.outputIndexes);
    std::vector<std::pair<TransactionIndex, uint16_t>> &amount_outs_vec = it->second;
    size_t count = 0;
    for (uint64_t i : absolute_offsets)
    {
      if (i >= amount_outs_vec.size())
      {
        logger(logging::INFO) << "Wrong index in transaction inputs: " << i << ", expected maximum " << amount_outs_vec.size() - 1;
        return false;
      }

      const TransactionEntry &tx = transactionByIndex(amount_outs_vec[i].first);

      if (!(amount_outs_vec[i].second < tx.tx.outputs.size()))
      {
        logger(logging::ERROR, logging::BRIGHT_RED)
            << "Wrong index in transaction outputs: "
            << amount_outs_vec[i].second << ", expected less then "
            << tx.tx.outputs.size();
        return false;
      }

      if (!vis.handle_output(tx.tx, tx.tx.outputs[amount_outs_vec[i].second], amount_outs_vec[i].second))
      {
        logger(logging::INFO) << "Failed to handle_output for output no = " << count << ", with absolute offset " << i;
        return false;
      }

      if (count++ == absolute_offsets.size() - 1 && pmax_related_block_height && *pmax_related_block_height < amount_outs_vec[i].first.block)
      {
        *pmax_related_block_height = amount_outs_vec[i].first.block;
      }
    }

    return true;
  }

  class check_tx_outputs_visitor : public boost::static_visitor<bool>
  {
  private:
    const Transaction &m_tx;
    uint32_t m_height;
    uint64_t m_amount;
    const Currency &m_currency;
    std::string &m_error;

  public:
    check_tx_outputs_visitor(const Transaction &tx, uint32_t height, uint64_t amount, const Currency &currency, std::string &error) : m_tx(tx), m_height(height), m_amount(amount), m_currency(currency), m_error(error) {}
    bool operator()(const KeyOutput &out) const
    {
      if (m_amount == 0)
      {
        m_error = "zero amount output";
        return false;
      }

      if (!check_key(out.key))
      {
        m_error = "output with invalid key";
        return false;
      }

      return true;
    }
    bool operator()(const MultisignatureOutput &out) const
    {
      if (m_tx.version < TRANSACTION_VERSION_2)
      {
        m_error = "contains multisignature output but have version " + m_tx.version;
        return false;
      }

      if (!m_currency.validateOutput(m_amount, out, m_height))
      {
        m_error = "contains invalid multisignature output";
        return false;
      }

      if (out.requiredSignatureCount > out.keys.size())
      {
        m_error = "contains multisignature with invalid required signature count";
        return false;
      }

      if (std::any_of(out.keys.begin(), out.keys.end(), [](const crypto::PublicKey &key)
                      { return !check_key(key); }))
      {
        m_error = "contains multisignature output with invalid public key";
        return false;
      }

      return true;
    }
  };
} // namespace cn
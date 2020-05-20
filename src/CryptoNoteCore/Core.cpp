// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "Core.h"

#include <sstream>
#include <unordered_set>
#include "../CryptoNoteConfig.h"
#include "../Common/CommandLine.h"
#include "../Common/Util.h"
#include "../Common/Math.h"
#include "../Common/StringTools.h"
#include "../crypto/crypto.h"
#include "../CryptoNoteProtocol/CryptoNoteProtocolDefinitions.h"
#include "../Logging/LoggerRef.h"
#include "../Rpc/CoreRpcServerCommandsDefinitions.h"
#include "CryptoNoteFormatUtils.h"

#include "CryptoNoteTools.h"
#include "CryptoNoteStatInfo.h"
#include "Miner.h"
#include "TransactionExtra.h"
#include "IBlock.h"

#undef ERROR

using namespace Logging;

#include "CryptoNoteCore/CoreConfig.h"

using namespace  Common;

namespace CryptoNote {

class BlockWithTransactions : public IBlock {
public:
  virtual const Block& getBlock() const override {
    return block;
  }

  virtual size_t getTransactionCount() const override {
    return transactions.size();
  }

  virtual const Transaction& getTransaction(size_t index) const override {
    assert(index < transactions.size());
    return transactions[index];
  }

private:
  Block block;
  std::vector<Transaction> transactions;

  friend class core;
};

core::core(const Currency& currency, i_cryptonote_protocol* pprotocol, Logging::ILogger& logger, bool blockchainIndexesEnabled) :
  m_currency(currency),
  logger(logger, "core"),
  m_mempool(currency, m_blockchain, m_timeProvider, logger),
  m_blockchain(currency, m_mempool, logger, blockchainIndexesEnabled),
  m_miner(new miner(currency, *this, logger)),
  m_starter_message_showed(false) {

  set_cryptonote_protocol(pprotocol);
  m_blockchain.addObserver(this);
  m_mempool.addObserver(this);
  }
  //-----------------------------------------------------------------------------------------------
  core::~core() {
  m_blockchain.removeObserver(this);
}

void core::set_cryptonote_protocol(i_cryptonote_protocol* pprotocol) {
  if (pprotocol) {
    m_pprotocol = pprotocol;
  } else {
    m_pprotocol = &m_protocol_stub;
  }
}
//-----------------------------------------------------------------------------------
void core::set_checkpoints(Checkpoints&& chk_pts) {
  m_blockchain.setCheckpoints(std::move(chk_pts));
}
//-----------------------------------------------------------------------------------
void core::init_options(boost::program_options::options_description& /*desc*/) {
}

bool core::handle_command_line(const boost::program_options::variables_map& vm) {
  m_config_folder = command_line::get_arg(vm, command_line::arg_data_dir);
  return true;
}

uint32_t core::get_current_blockchain_height() {
  return m_blockchain.getCurrentBlockchainHeight();
}

void core::get_blockchain_top(uint32_t& height, Crypto::Hash& top_id) {
  assert(m_blockchain.getCurrentBlockchainHeight() > 0);
  top_id = m_blockchain.getTailId(height);
}

bool core::rollback_chain_to(uint32_t height) {
  return m_blockchain.rollbackBlockchainTo(height);
}

bool core::get_blocks(uint32_t start_offset, uint32_t count, std::list<Block>& blocks, std::list<Transaction>& txs) {
  return m_blockchain.getBlocks(start_offset, count, blocks, txs);
}


bool core::get_blocks(uint32_t start_offset, uint32_t count, std::list<Block>& blocks) {
  return m_blockchain.getBlocks(start_offset, count, blocks);
}
void core::getTransactions(const std::vector<Crypto::Hash>& txs_ids, std::list<Transaction>& txs, std::list<Crypto::Hash>& missed_txs, bool checkTxPool) {
  m_blockchain.getTransactions(txs_ids, txs, missed_txs, checkTxPool);
}

bool core::get_alternative_blocks(std::list<Block>& blocks) {
  return m_blockchain.getAlternativeBlocks(blocks);
}

size_t core::get_alternative_blocks_count() {
  return m_blockchain.getAlternativeBlocksCount();
}
//-----------------------------------------------------------------------------------------------
bool core::init(const CoreConfig& config, const MinerConfig& minerConfig, bool load_existing) {
  m_config_folder = config.configFolder;
  bool r = m_mempool.init(m_config_folder);

  if (!(r)) {
    logger(ERROR, BRIGHT_RED) << "<< Core.cpp << " << "Failed to initialize memory pool";
    return false;
  }

  r = m_blockchain.init(m_config_folder, load_existing);
  if (!(r)) {
    logger(ERROR, BRIGHT_RED) << "<< Core.cpp << " << "Failed to initialize blockchain storage";
    return false;
  }

  r = m_miner->init(minerConfig);
  if (!(r)) {
    logger(ERROR, BRIGHT_RED) << "<< Core.cpp << " << "Failed to initialize blockchain storage";
    return false;
  }

  return load_state_data();
}

bool core::set_genesis_block(const Block& b) {
  return m_blockchain.resetAndSetGenesisBlock(b);
}

bool core::load_state_data() {
  // may be some code later
  return true;
}

bool core::deinit() {
  m_miner->stop();
  m_mempool.deinit();
  m_blockchain.deinit();
  return true;
}

size_t core::addChain(const std::vector<const IBlock*>& chain) {
  size_t blocksCounter = 0;

  for (const IBlock* block : chain) {
    bool allTransactionsAdded = true;
    for (size_t txNumber = 0; txNumber < block->getTransactionCount(); ++txNumber) {
      const Transaction& tx = block->getTransaction(txNumber);

      Crypto::Hash txHash = NULL_HASH;
      size_t blobSize = 0;
      getObjectHash(tx, txHash, blobSize);
      tx_verification_context tvc = boost::value_initialized<tx_verification_context>();

      if (!handleIncomingTransaction(tx, txHash, blobSize, tvc, true, get_block_height(block->getBlock()))) {
        logger(ERROR, BRIGHT_RED) << "<< Core.cpp << " << "core::addChain() failed to handle transaction " << txHash << " from block " << blocksCounter << "/" << chain.size();
        allTransactionsAdded = false;
        break;
      }
    }

    if (!allTransactionsAdded) {
      break;
    }

    block_verification_context bvc = boost::value_initialized<block_verification_context>();
    m_blockchain.addNewBlock(block->getBlock(), bvc);
    if (bvc.m_marked_as_orphaned || bvc.m_verification_failed) {
      logger(ERROR, BRIGHT_RED) << "<< Core.cpp << " << "core::addChain() failed to handle incoming block " << get_block_hash(block->getBlock()) <<
        ", " << blocksCounter << "/" << chain.size();

      break;
    }

    ++blocksCounter;
    // TODO m_dispatcher.yield()?
  }

  return blocksCounter;
}

bool core::handle_incoming_tx(const BinaryArray& tx_blob, tx_verification_context& tvc, bool keeped_by_block) { //Deprecated. Should be removed with CryptoNoteProtocolHandler.
  tvc = boost::value_initialized<tx_verification_context>();
  //want to process all transactions sequentially

  if (tx_blob.size() > m_currency.maxTxSize()) {
    logger(INFO) << "<< Core.cpp << " << "WRONG TRANSACTION BLOB, too big size " << tx_blob.size() << "<< Core.cpp << " << ", rejected";
    tvc.m_verification_failed = true;
    return false;
  }

  Crypto::Hash tx_hash = NULL_HASH;
  Crypto::Hash tx_prefixt_hash = NULL_HASH;
  Transaction tx;

  if (!parse_tx_from_blob(tx, tx_hash, tx_prefixt_hash, tx_blob)) {
    logger(INFO) << "<< Core.cpp << " << "WRONG TRANSACTION BLOB, Failed to parse, rejected";
    tvc.m_verification_failed = true;
    return false;
  }
  //std::cout << "!"<< tx.inputs.size() << std::endl;

  Crypto::Hash blockId;
  uint32_t blockHeight;
  bool ok = getBlockContainingTx(tx_hash, blockId, blockHeight);
  if (!ok) blockHeight = this->get_current_blockchain_height(); //this assumption fails for withdrawals
  return handleIncomingTransaction(tx, tx_hash, tx_blob.size(), tvc, keeped_by_block, blockHeight);
}

bool core::get_stat_info(core_stat_info& st_inf) {
  st_inf.mining_speed = m_miner->get_speed();
  st_inf.alternative_blocks = m_blockchain.getAlternativeBlocksCount();
  st_inf.blockchain_height = m_blockchain.getCurrentBlockchainHeight();
  st_inf.tx_pool_size = m_mempool.get_transactions_count();
  st_inf.top_block_id_str = Common::podToHex(m_blockchain.getTailId());
  return true;
}

bool core::check_tx_semantic(const Transaction& tx, bool keeped_by_block, uint32_t &height) {
  if (!tx.inputs.size()) {
    logger(ERROR) << "<< Core.cpp << " << "tx with empty inputs, rejected for tx id= " << getObjectHash(tx);
    return false;
  }

  if (!check_inputs_types_supported(tx)) {
    logger(ERROR) << "<< Core.cpp << " << "unsupported input types for tx id= " << getObjectHash(tx);
    return false;
  }

  std::string errmsg;
  if (!check_outs_valid(tx, &errmsg)) {
    logger(ERROR) << "<< Core.cpp << " << "tx with invalid outputs, rejected for tx id= " << getObjectHash(tx) << "<< Core.cpp << " << ": " << errmsg;
    return false;
  }

  if (!check_money_overflow(tx)) {
    logger(ERROR) << "<< Core.cpp << " << "tx have money overflow, rejected for tx id= " << getObjectHash(tx);
    return false;
  }

  uint64_t amount_in = m_currency.getTransactionAllInputsAmount(tx, height);
  uint64_t amount_out = get_outs_money_amount(tx);

  if (amount_in < amount_out){
	  //correct check for unknown deposit creation height
	  uint32_t testHeight = height > parameters::END_MULTIPLIER_BLOCK ? 0 : (uint32_t)(-1); //try other mode
	  amount_in = m_currency.getTransactionAllInputsAmount(tx, testHeight);
	  if (amount_in < amount_out) {
		logger(ERROR) << "<< Core.cpp << " << "tx with wrong amounts: ins " << amount_in << ", outs " << amount_out << ", rejected for tx id= " << getObjectHash(tx);
		return false;
	  } else {
		  height = testHeight;
	  }
  }

  //check if tx use different key images
  if (!check_tx_inputs_keyimages_diff(tx)) {
    logger(ERROR) << "<< Core.cpp << " << "tx has a few inputs with identical keyimages";
    return false;
  }

  if (!checkMultisignatureInputsDiff(tx)) {
    logger(ERROR) << "<< Core.cpp << " << "tx has a few multisignature inputs with identical output indexes";
    return false;
  }

  return true;
}

bool core::check_tx_inputs_keyimages_diff(const Transaction& tx) {
  std::unordered_set<Crypto::KeyImage> ki;
  for (const auto& in : tx.inputs) {
    if (in.type() == typeid(KeyInput)) {
      if (!ki.insert(boost::get<KeyInput>(in).keyImage).second)
        return false;
    }
  }
  return true;
}

size_t core::get_blockchain_total_transactions() {
  return m_blockchain.getTotalTransactions();
}

//bool core::get_outs(uint64_t amount, std::list<Crypto::PublicKey>& pkeys)
//{
//  return m_blockchain.get_outs(amount, pkeys);
//}

bool core::add_new_tx(const Transaction& tx, const Crypto::Hash& tx_hash, size_t blob_size, tx_verification_context& tvc, bool keeped_by_block, uint32_t height) {
  //Locking on m_mempool and m_blockchain closes possibility to add tx to memory pool which is already in blockchain
  std::lock_guard<decltype(m_mempool)> lk(m_mempool);
  LockedBlockchainStorage lbs(m_blockchain);

  if (m_blockchain.haveTransaction(tx_hash)) {
    logger(TRACE) << "<< Core.cpp << " << "tx " << tx_hash << " is already in blockchain";
    return true;
  }

  if (m_mempool.have_tx(tx_hash)) {
    logger(TRACE) << "<< Core.cpp << " << "tx " << tx_hash << " is already in transaction pool";
    return true;
  }
  return m_mempool.add_tx(tx, tx_hash, blob_size, tvc, keeped_by_block, height);
}

bool core::get_block_template(Block& b, const AccountPublicAddress& adr, difficulty_type& diffic, uint32_t& height, const BinaryArray& ex_nonce) {
  size_t median_size;
  uint64_t already_generated_coins;

  {
    LockedBlockchainStorage blockchainLock(m_blockchain);
    height = m_blockchain.getCurrentBlockchainHeight();
    diffic = m_blockchain.getDifficultyForNextBlock();
    if (!(diffic)) {
      logger(ERROR, BRIGHT_RED) << "<< Core.cpp << " << "difficulty overhead.";
      return false;
    }

    b = boost::value_initialized<Block>();
    b.majorVersion = m_blockchain.get_block_major_version_for_height(height);

	if (b.majorVersion < BLOCK_MAJOR_VERSION_7) {
      if (b.majorVersion == BLOCK_MAJOR_VERSION_1) {
        b.minorVersion = m_currency.upgradeHeight(BLOCK_MAJOR_VERSION_2) == UpgradeDetectorBase::UNDEF_HEIGHT ? BLOCK_MINOR_VERSION_1 : BLOCK_MINOR_VERSION_0;
      } else {
        b.minorVersion = m_currency.upgradeHeight(BLOCK_MAJOR_VERSION_3) == UpgradeDetectorBase::UNDEF_HEIGHT ? BLOCK_MINOR_VERSION_1 : BLOCK_MINOR_VERSION_0;
      }
    } else {
		b.minorVersion = BLOCK_MINOR_VERSION_0;
	}

    b.previousBlockHash = get_tail_id();
    b.timestamp = time(NULL);
// k0x001
// Don't generate a block template with invalid timestamp
// Fix by Jagerman
    if(height >= m_currency.timestampCheckWindow()) {
      std::vector<uint64_t> timestamps;
      for(uint32_t offset = height - static_cast<uint32_t>(m_currency.timestampCheckWindow()); offset < height; ++offset) {
        timestamps.push_back(m_blockchain.getBlockTimestamp(offset));
      }
      uint64_t median_ts = Common::medianValue(timestamps);
      if (b.timestamp < median_ts) {
          b.timestamp = median_ts;
      }
    }
//	
    median_size = m_blockchain.getCurrentCumulativeBlocksizeLimit() / 2;
    already_generated_coins = m_blockchain.getCoinsInCirculation();
  }

  size_t txs_size;
  uint64_t fee;
  if (!m_mempool.fill_block_template(b, median_size, m_currency.maxBlockCumulativeSize(height), already_generated_coins, txs_size, fee, height)) {
    return false;
  }

  /*
     two-phase miner transaction generation: we don't know exact block size until we prepare block, but we don't know reward until we know
     block size, so first miner transaction generated with fake amount of money, and with phase we know think we know expected block size
     */
  //make blocks coin-base tx looks close to real coinbase tx to get truthful blob size
  bool r = m_currency.constructMinerTx(height, median_size, already_generated_coins, txs_size, fee, adr, b.baseTransaction, ex_nonce, 11);
  if (!r) {
    logger(ERROR, BRIGHT_RED) << "<< Core.cpp << " << "Failed to construct miner tx, first chance";
    return false;
  }

  size_t cumulative_size = txs_size + getObjectBinarySize(b.baseTransaction);
  for (size_t try_count = 0; try_count != 10; ++try_count) {
    r = m_currency.constructMinerTx(height, median_size, already_generated_coins, cumulative_size, fee, adr, b.baseTransaction, ex_nonce, 11);

    if (!(r)) { logger(ERROR, BRIGHT_RED) << "<< Core.cpp << " << "Failed to construct miner tx, second chance"; return false; }
    size_t coinbase_blob_size = getObjectBinarySize(b.baseTransaction);
    if (coinbase_blob_size > cumulative_size - txs_size) {
      cumulative_size = txs_size + coinbase_blob_size;
      continue;
    }

    if (coinbase_blob_size < cumulative_size - txs_size) {
      size_t delta = cumulative_size - txs_size - coinbase_blob_size;
      b.baseTransaction.extra.insert(b.baseTransaction.extra.end(), delta, 0);
      //here  could be 1 byte difference, because of extra field counter is varint, and it can become from 1-byte len to 2-bytes len.
      if (cumulative_size != txs_size + getObjectBinarySize(b.baseTransaction)) {
        if (!(cumulative_size + 1 == txs_size + getObjectBinarySize(b.baseTransaction))) { logger(ERROR, BRIGHT_RED) << "<< Core.cpp << " << "unexpected case: cumulative_size=" << cumulative_size << " + 1 is not equal txs_cumulative_size=" << txs_size << " + get_object_blobsize(b.baseTransaction)=" << getObjectBinarySize(b.baseTransaction); return false; }
          b.baseTransaction.extra.resize(b.baseTransaction.extra.size() - 1);
          if (cumulative_size != txs_size + getObjectBinarySize(b.baseTransaction)) {
            //fuck, not lucky, -1 makes varint-counter size smaller, in that case we continue to grow with cumulative_size
            logger(TRACE, BRIGHT_RED) <<
              "Miner tx creation have no luck with delta_extra size = " << delta << " and " << delta - 1;

            cumulative_size += delta - 1;
            continue;
        }
        logger(DEBUGGING, BRIGHT_GREEN) <<
          "Setting extra for block: " << b.baseTransaction.extra.size() << "<< Core.cpp << " << ", try_count=" << try_count;
      }
    }

    if (!(cumulative_size == txs_size + getObjectBinarySize(b.baseTransaction))) {
      logger(ERROR, BRIGHT_RED) << "<< Core.cpp << " << "unexpected case: cumulative_size=" << cumulative_size << " is not equal txs_cumulative_size=" << txs_size << " + get_object_blobsize(b.baseTransaction)=" << getObjectBinarySize(b.baseTransaction);
      return false;
    }

    return true;
  }

  logger(ERROR, BRIGHT_RED) <<
    "Failed to create_block_template with " << 10 << " tries";

  return false;
}

std::vector<Crypto::Hash> core::findBlockchainSupplement(const std::vector<Crypto::Hash>& remoteBlockIds, size_t maxCount,
  uint32_t& totalBlockCount, uint32_t& startBlockIndex) {

  assert(!remoteBlockIds.empty());
  assert(remoteBlockIds.back() == m_blockchain.getBlockIdByHeight(0));

  return m_blockchain.findBlockchainSupplement(remoteBlockIds, maxCount, totalBlockCount, startBlockIndex);
}

void core::print_blockchain(uint32_t start_index, uint32_t end_index) {
  m_blockchain.print_blockchain(start_index, end_index);
}

void core::print_blockchain_index() {
  m_blockchain.print_blockchain_index();
}

void core::print_blockchain_outs(const std::string& file) {
  m_blockchain.print_blockchain_outs(file);
}

bool core::get_random_outs_for_amounts(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request& req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response& res) {
  return m_blockchain.getRandomOutsByAmount(req, res);
}

bool core::get_tx_outputs_gindexs(const Crypto::Hash& tx_id, std::vector<uint32_t>& indexs) {
  return m_blockchain.getTransactionOutputGlobalIndexes(tx_id, indexs);
}

bool core::getOutByMSigGIndex(uint64_t amount, uint64_t gindex, MultisignatureOutput& out) {
  return m_blockchain.get_out_by_msig_gindex(amount, gindex, out);
}

void core::pause_mining() {
  m_miner->pause();
}

void core::update_block_template_and_resume_mining() {
  update_miner_block_template();
  m_miner->resume();
}

bool core::handle_block_found(Block& b) {
  block_verification_context bvc = boost::value_initialized<block_verification_context>();
  handle_incoming_block(b, bvc, true, true);

  if (bvc.m_verification_failed) {
    logger(ERROR) << "<< Core.cpp << " << "mined block failed verification";
  }

  return bvc.m_added_to_main_chain;
}

void core::on_synchronized() {
  m_miner->on_synchronized();
}

bool core::getPoolChanges(const Crypto::Hash& tailBlockId, const std::vector<Crypto::Hash>& knownTxsIds,
                          std::vector<Transaction>& addedTxs, std::vector<Crypto::Hash>& deletedTxsIds) {

  getPoolChanges(knownTxsIds, addedTxs, deletedTxsIds);
  return tailBlockId == m_blockchain.getTailId();
}

bool core::getPoolChangesLite(const Crypto::Hash& tailBlockId, const std::vector<Crypto::Hash>& knownTxsIds,
        std::vector<TransactionPrefixInfo>& addedTxs, std::vector<Crypto::Hash>& deletedTxsIds) {

  std::vector<Transaction> added;
  bool returnStatus = getPoolChanges(tailBlockId, knownTxsIds, added, deletedTxsIds);

  for (const auto& tx: added) {
    TransactionPrefixInfo tpi;
    tpi.txPrefix = tx;
    tpi.txHash = getObjectHash(tx);

    addedTxs.push_back(std::move(tpi));
  }

  return returnStatus;
}

void core::getPoolChanges(const std::vector<Crypto::Hash>& knownTxsIds, std::vector<Transaction>& addedTxs,
                          std::vector<Crypto::Hash>& deletedTxsIds) {

  std::vector<Crypto::Hash> addedTxsIds;
  auto guard = m_mempool.obtainGuard();
  m_mempool.get_difference(knownTxsIds, addedTxsIds, deletedTxsIds);
  std::vector<Crypto::Hash> misses;
  m_mempool.getTransactions(addedTxsIds, addedTxs, misses);
  assert(misses.empty());
}

bool core::handle_incoming_block_blob(const BinaryArray& block_blob, block_verification_context& bvc, bool control_miner, bool relay_block) {
  if (block_blob.size() > m_currency.maxBlockBlobSize()) {
    logger(INFO) << "<< Core.cpp << " << "WRONG BLOCK BLOB, too big size " << block_blob.size() << "<< Core.cpp << " << ", rejected";
    bvc.m_verification_failed = true;
    return false;
  }

  Block b;
  if (!fromBinaryArray(b, block_blob)) {
    logger(INFO) << "<< Core.cpp << " << "Failed to parse and validate new block";
    bvc.m_verification_failed = true;
    return false;
  }

  return handle_incoming_block(b, bvc, control_miner, relay_block);
}

bool core::handle_incoming_block(const Block& b, block_verification_context& bvc, bool control_miner, bool relay_block) {
  if (control_miner) {
    pause_mining();
  }

  m_blockchain.addNewBlock(b, bvc);

  if (control_miner) {
    update_block_template_and_resume_mining();
  }

  if (relay_block && bvc.m_added_to_main_chain) {
    std::list<Crypto::Hash> missed_txs;
    std::list<Transaction> txs;
    m_blockchain.getTransactions(b.transactionHashes, txs, missed_txs);
    if (!missed_txs.empty() && getBlockIdByHeight(get_block_height(b)) != get_block_hash(b)) {
      logger(INFO) << "<< Core.cpp << " << "Block added, but it seems that reorganize just happened after that, do not relay this block";
    } else {
      if (!(txs.size() == b.transactionHashes.size() && missed_txs.empty())) {
        logger(ERROR, BRIGHT_RED) << "<< Core.cpp << " << "can't find some transactions in found block:" <<
          get_block_hash(b) << "<< Core.cpp << " << " txs.size()=" << txs.size() << "<< Core.cpp << " << ", b.transactionHashes.size()=" << b.transactionHashes.size() << "<< Core.cpp << " << ", missed_txs.size()" << missed_txs.size(); return false;
      }

      NOTIFY_NEW_BLOCK::request arg;
      arg.hop = 0;
      arg.current_blockchain_height = m_blockchain.getCurrentBlockchainHeight();
      BinaryArray blockBa;
      bool r = toBinaryArray(b, blockBa);
      if (!(r)) { logger(ERROR, BRIGHT_RED) << "<< Core.cpp << " << "failed to serialize block"; return false; }
      arg.b.block = asString(blockBa);
      for (auto& tx : txs) {
        arg.b.txs.push_back(asString(toBinaryArray(tx)));
      }

      m_pprotocol->relay_block(arg);
    }
  }

  return true;
}

Crypto::Hash core::get_tail_id() {
  return m_blockchain.getTailId();
}

size_t core::get_pool_transactions_count() {
  return m_mempool.get_transactions_count();
}

bool core::have_block(const Crypto::Hash& id) {
  return m_blockchain.haveBlock(id);
}

bool core::parse_tx_from_blob(Transaction& tx, Crypto::Hash& tx_hash, Crypto::Hash& tx_prefix_hash, const BinaryArray& blob) {
  return parseAndValidateTransactionFromBinaryArray(blob, tx, tx_hash, tx_prefix_hash);
}

bool core::check_tx_syntax(const Transaction& tx) {
  return true;
}

std::vector<Transaction> core::getPoolTransactions() {
  std::list<Transaction> txs;
  m_mempool.get_transactions(txs);

  std::vector<Transaction> result;
  for (auto& tx : txs) {
    result.emplace_back(std::move(tx));
  }

  return result;
}

std::vector<Crypto::Hash> core::buildSparseChain() {
  assert(m_blockchain.getCurrentBlockchainHeight() != 0);
  return m_blockchain.buildSparseChain();
}

std::vector<Crypto::Hash> core::buildSparseChain(const Crypto::Hash& startBlockId) {
  LockedBlockchainStorage lbs(m_blockchain);
  assert(m_blockchain.haveBlock(startBlockId));
  return m_blockchain.buildSparseChain(startBlockId);
}

bool core::handle_get_objects(NOTIFY_REQUEST_GET_OBJECTS::request& arg, NOTIFY_RESPONSE_GET_OBJECTS::request& rsp) { //Deprecated. Should be removed with CryptoNoteProtocolHandler.
  return m_blockchain.handleGetObjects(arg, rsp);
}

Crypto::Hash core::getBlockIdByHeight(uint32_t height) {
  LockedBlockchainStorage lbs(m_blockchain);
  if (height < m_blockchain.getCurrentBlockchainHeight()) {
    return m_blockchain.getBlockIdByHeight(height);
  } else {
    return NULL_HASH;
  }
}

bool core::getBlockByHash(const Crypto::Hash &h, Block &blk) {
  return m_blockchain.getBlockByHash(h, blk);
}

bool core::getBlockHeight(const Crypto::Hash& blockId, uint32_t& blockHeight) {
  return m_blockchain.getBlockHeight(blockId, blockHeight);
}

uint64_t core::coinsEmittedAtHeight(uint64_t height) {
  return m_blockchain.coinsEmittedAtHeight(height);
}

uint64_t core::difficultyAtHeight(uint64_t height) {
  return m_blockchain.difficultyAtHeight(height);
}

//void core::get_all_known_block_ids(std::list<Crypto::Hash> &main, std::list<Crypto::Hash> &alt, std::list<Crypto::Hash> &invalid) {
//  m_blockchain.get_all_known_block_ids(main, alt, invalid);
//}

std::string core::print_pool(bool short_format) {
  return m_mempool.print_pool(short_format);
}

bool core::update_miner_block_template() {
  m_miner->on_block_chain_update();
  return true;
}

bool core::on_idle() {
  if (!m_starter_message_showed) {
    logger(INFO) << ENDL << "**********************************************************************" << ENDL
      << "The daemon will continue synchronizing with the network. If this is a new installation it may take up to several hours." << ENDL
      << "You can set the level of process detailization* through \"set_log <level>\" command*, where <level> is between 0 (no details) and 4 (very verbose)." << ENDL
      << "Use \"help\" command to see the list of available commands." << ENDL
      << "Note: in case you need to interrupt the process, use \"exit\" command. Otherwise, the current progress won't be saved." << ENDL
      << "**********************************************************************";
    m_starter_message_showed = true;
  }

  m_miner->on_idle();
  m_mempool.on_idle();
  return true;
}

bool core::addObserver(ICoreObserver* observer) {
  return m_observerManager.add(observer);
}

bool core::removeObserver(ICoreObserver* observer) {
  return m_observerManager.remove(observer);
}

void core::blockchainUpdated() {
  m_observerManager.notify(&ICoreObserver::blockchainUpdated);
}

void core::txDeletedFromPool() {
  poolUpdated();
}

void core::poolUpdated() {
  m_observerManager.notify(&ICoreObserver::poolUpdated);
}

bool core::queryBlocks(const std::vector<Crypto::Hash>& knownBlockIds, uint64_t timestamp,
  uint32_t& resStartHeight, uint32_t& resCurrentHeight, uint32_t& resFullOffset, std::vector<BlockFullInfo>& entries) {

  LockedBlockchainStorage lbs(m_blockchain);

  uint32_t currentHeight = lbs->getCurrentBlockchainHeight();
  uint32_t startOffset = 0;
  uint32_t startFullOffset = 0;

  if (!findStartAndFullOffsets(knownBlockIds, timestamp, startOffset, startFullOffset)) {
    return false;
  }

  resFullOffset = startFullOffset;
  std::vector<Crypto::Hash> blockIds = findIdsForShortBlocks(startOffset, startFullOffset);
  entries.reserve(blockIds.size());

  for (const auto& id : blockIds) {
    entries.push_back(BlockFullInfo());
    entries.back().block_id = id;
  }

  resCurrentHeight = currentHeight;
  resStartHeight = startOffset;

  uint32_t blocksLeft = static_cast<uint32_t>(std::min(BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT - entries.size(), size_t(BLOCKS_SYNCHRONIZING_DEFAULT_COUNT)));

  if (blocksLeft == 0) {
    return true;
  }

  std::list<Block> blocks;
  lbs->getBlocks(startFullOffset, blocksLeft, blocks);

  for (auto& b : blocks) {
    BlockFullInfo item;

    item.block_id = get_block_hash(b);

    if (b.timestamp >= timestamp) {
      // query transactions
      std::list<Transaction> txs;
      std::list<Crypto::Hash> missedTxs;
      lbs->getTransactions(b.transactionHashes, txs, missedTxs);

      // fill data
      block_complete_entry& completeEntry = item;
      completeEntry.block = asString(toBinaryArray(b));
      for (auto& tx : txs) {
        completeEntry.txs.push_back(asString(toBinaryArray(tx)));
      }
    }

    entries.push_back(std::move(item));
  }

  return true;
}

bool core::findStartAndFullOffsets(const std::vector<Crypto::Hash>& knownBlockIds, uint64_t timestamp, uint32_t& startOffset, uint32_t& startFullOffset) {
  LockedBlockchainStorage lbs(m_blockchain);

  if (knownBlockIds.empty()) {
    logger(ERROR, BRIGHT_RED) << "<< Core.cpp << " << "knownBlockIds is empty";
    return false;
  }

  if (knownBlockIds.back() != m_blockchain.getBlockIdByHeight(0)) {
    logger(ERROR, BRIGHT_RED) << "<< Core.cpp << " << "knownBlockIds doesn't end with genesis block hash: " << knownBlockIds.back();
    return false;
  }

  startOffset = lbs->findBlockchainSupplement(knownBlockIds);
  if (!lbs->getLowerBound(timestamp, startOffset, startFullOffset)) {
    startFullOffset = startOffset;
  }

  return true;
}

std::vector<Crypto::Hash> core::findIdsForShortBlocks(uint32_t startOffset, uint32_t startFullOffset) {
  assert(startOffset <= startFullOffset);

  LockedBlockchainStorage lbs(m_blockchain);

  std::vector<Crypto::Hash> result;
  if (startOffset < startFullOffset) {
    result = lbs->getBlockIds(startOffset, std::min(static_cast<uint32_t>(BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT), startFullOffset - startOffset));
  }

  return result;
}

bool core::queryBlocksLite(const std::vector<Crypto::Hash>& knownBlockIds, uint64_t timestamp, uint32_t& resStartHeight,
  uint32_t& resCurrentHeight, uint32_t& resFullOffset, std::vector<BlockShortInfo>& entries) {
  LockedBlockchainStorage lbs(m_blockchain);

  resCurrentHeight = lbs->getCurrentBlockchainHeight();
  resStartHeight = 0;
  resFullOffset = 0;

  if (!findStartAndFullOffsets(knownBlockIds, timestamp, resStartHeight, resFullOffset)) {
    return false;
  }

  std::vector<Crypto::Hash> blockIds = findIdsForShortBlocks(resStartHeight, resFullOffset);
  entries.reserve(blockIds.size());

  for (const auto& id : blockIds) {
    entries.push_back(BlockShortInfo());
    entries.back().blockId = id;
  }

  uint32_t blocksLeft = static_cast<uint32_t>(std::min(BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT - entries.size(), size_t(BLOCKS_SYNCHRONIZING_DEFAULT_COUNT)));

  if (blocksLeft == 0) {
    return true;
  }

  std::list<Block> blocks;
  lbs->getBlocks(resFullOffset, blocksLeft, blocks);

  for (auto& b : blocks) {
    BlockShortInfo item;

    item.blockId = get_block_hash(b);

    if (b.timestamp >= timestamp) {
      std::list<Transaction> txs;
      std::list<Crypto::Hash> missedTxs;
      lbs->getTransactions(b.transactionHashes, txs, missedTxs);

      item.block = asString(toBinaryArray(b));

      for (const auto& tx: txs) {
        TransactionPrefixInfo info;
        info.txPrefix = tx;
        info.txHash = getObjectHash(tx);

        item.txPrefixes.push_back(std::move(info));
      }
    }

    entries.push_back(std::move(item));
  }

  return true;
}

bool core::getBackwardBlocksSizes(uint32_t fromHeight, std::vector<size_t>& sizes, size_t count) {
  return m_blockchain.getBackwardBlocksSize(fromHeight, sizes, count);
}

bool core::getBlockSize(const Crypto::Hash& hash, size_t& size) {
  return m_blockchain.getBlockSize(hash, size);
}

bool core::getAlreadyGeneratedCoins(const Crypto::Hash& hash, uint64_t& generatedCoins) {
  return m_blockchain.getAlreadyGeneratedCoins(hash, generatedCoins);
}

bool core::getBlockReward(size_t medianSize, size_t currentBlockSize, uint64_t alreadyGeneratedCoins, uint64_t fee, uint32_t height,
                          uint64_t& reward, int64_t& emissionChange) {
  return m_currency.getBlockReward(medianSize, currentBlockSize, alreadyGeneratedCoins, fee, height, reward, emissionChange);
}

bool core::scanOutputkeysForIndices(const KeyInput& txInToKey, std::list<std::pair<Crypto::Hash, size_t>>& outputReferences) {
  struct outputs_visitor
  {
    std::list<std::pair<Crypto::Hash, size_t>>& m_resultsCollector;
    outputs_visitor(std::list<std::pair<Crypto::Hash, size_t>>& resultsCollector):m_resultsCollector(resultsCollector){}
    bool handle_output(const Transaction& tx, const TransactionOutput& out, size_t transactionOutputIndex)
    {
      m_resultsCollector.push_back(std::make_pair(getObjectHash(tx), transactionOutputIndex));
      return true;
    }
  };

  outputs_visitor vi(outputReferences);

  return m_blockchain.scanOutputKeysForIndexes(txInToKey, vi);
}

bool core::getBlockDifficulty(uint32_t height, difficulty_type& difficulty) {
  difficulty = m_blockchain.blockDifficulty(height);
  return true;
}

bool core::getBlockContainingTx(const Crypto::Hash& txId, Crypto::Hash& blockId, uint32_t& blockHeight) {
  return m_blockchain.getBlockContainingTransaction(txId, blockId, blockHeight);
}

bool core::getMultisigOutputReference(const MultisignatureInput& txInMultisig, std::pair<Crypto::Hash, size_t>& outputReference) {
  return m_blockchain.getMultisigOutputReference(txInMultisig, outputReference);
}

bool core::getGeneratedTransactionsNumber(uint32_t height, uint64_t& generatedTransactions) {
  return m_blockchain.getGeneratedTransactionsNumber(height, generatedTransactions);
}

bool core::getOrphanBlocksByHeight(uint32_t height, std::vector<Block>& blocks) {
  std::vector<Crypto::Hash> blockHashes;
  if (!m_blockchain.getOrphanBlockIdsByHeight(height, blockHashes)) {
    return false;
  }

  for (const Crypto::Hash& hash : blockHashes) {
    Block blk;
    if (!getBlockByHash(hash, blk)) {
      return false;
    }
    blocks.push_back(std::move(blk));
  }
  return true;
}

bool core::getBlocksByTimestamp(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<Block>& blocks, uint32_t& blocksNumberWithinTimestamps) {
  std::vector<Crypto::Hash> blockHashes;
  if (!m_blockchain.getBlockIdsByTimestamp(timestampBegin, timestampEnd, blocksNumberLimit, blockHashes, blocksNumberWithinTimestamps)) {
    return false;
  }
  for (const Crypto::Hash& hash : blockHashes) {
    Block blk;
    if (!getBlockByHash(hash, blk)) {
      return false;
    }
    blocks.push_back(std::move(blk));
  }
  return true;
}

bool core::getPoolTransactionsByTimestamp(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<Transaction>& transactions, uint64_t& transactionsNumberWithinTimestamps) {
  std::vector<Crypto::Hash> poolTransactionHashes;
  if (!m_mempool.getTransactionIdsByTimestamp(timestampBegin, timestampEnd, transactionsNumberLimit, poolTransactionHashes, transactionsNumberWithinTimestamps)) {
    return false;
  }

  std::list<Transaction> txs;
  std::list<Crypto::Hash> missed_txs;

  getTransactions(poolTransactionHashes, txs, missed_txs, true);
  if (missed_txs.size() > 0) {
    return false;
  }

  transactions.insert(transactions.end(), txs.begin(), txs.end());
  return true;
}

bool core::getTransactionsByPaymentId(const Crypto::Hash& paymentId, std::vector<Transaction>& transactions) {
  std::vector<Crypto::Hash> blockchainTransactionHashes;
  if (!m_blockchain.getTransactionIdsByPaymentId(paymentId, blockchainTransactionHashes)) {
    return false;
  }

  std::vector<Crypto::Hash> poolTransactionHashes;
  if (!m_mempool.getTransactionIdsByPaymentId(paymentId, poolTransactionHashes)) {
    return false;
  }

  std::list<Transaction> txs;
  std::list<Crypto::Hash> missed_txs;
  blockchainTransactionHashes.insert(blockchainTransactionHashes.end(), poolTransactionHashes.begin(), poolTransactionHashes.end());

  getTransactions(blockchainTransactionHashes, txs, missed_txs, true);
  if (missed_txs.size() > 0) {
    return false;
  }

  transactions.insert(transactions.end(), txs.begin(), txs.end());
  return true;
}

std::error_code core::executeLocked(const std::function<std::error_code()>& func) {
  std::lock_guard<decltype(m_mempool)> lk(m_mempool);
  LockedBlockchainStorage lbs(m_blockchain);

  return func();
}

uint64_t core::getNextBlockDifficulty() {
  return m_blockchain.getDifficultyForNextBlock();
}

uint64_t core::getTotalGeneratedAmount() {
  return m_blockchain.getCoinsInCirculation();
}

uint64_t core::fullDepositAmount() const {
  return m_blockchain.fullDepositAmount();
}

uint64_t core::depositAmountAtHeight(size_t height) const {
  return m_blockchain.depositAmountAtHeight(height);
}

uint64_t core::depositInterestAtHeight(size_t height) const {
  return m_blockchain.depositInterestAtHeight(height);
}

bool core::handleIncomingTransaction(const Transaction& tx, const Crypto::Hash& txHash, size_t blobSize, tx_verification_context& tvc, bool keptByBlock, uint32_t height) {
  if (!check_tx_syntax(tx)) {
    logger(INFO) << "<< Core.cpp << " << "WRONG TRANSACTION BLOB, Failed to check tx " << txHash << " syntax, rejected";
    tvc.m_verification_failed = true;
    return false;
  }

  if (!check_tx_semantic(tx, keptByBlock, height)) {
    logger(INFO) << "<< Core.cpp << " << "WRONG TRANSACTION BLOB, Failed to check tx " << txHash << " semantic, rejected";
    tvc.m_verification_failed = true;
    return false;
  }

  bool r = add_new_tx(tx, txHash, blobSize, tvc, keptByBlock, height);
  if (tvc.m_verification_failed) {
    if (!tvc.m_tx_fee_too_small) {
      logger(ERROR) << "<< Core.cpp << " << "Transaction verification failed: " << txHash;
    } else {
      logger(INFO) << "<< Core.cpp << " << "Transaction verification failed: " << txHash;
    }
  } else if (tvc.m_verification_impossible) {
    logger(ERROR) << "<< Core.cpp << " << "Transaction verification impossible: " << txHash;
  }

  if (tvc.m_added_to_pool) {
    logger(DEBUGGING) << "<< Core.cpp << " << "tx added: " << txHash;
    poolUpdated();
  }

  return r;
}

std::unique_ptr<IBlock> core::getBlock(const Crypto::Hash& blockId) {
  std::lock_guard<decltype(m_mempool)> lk(m_mempool);
  LockedBlockchainStorage lbs(m_blockchain);

  std::unique_ptr<BlockWithTransactions> blockPtr(new BlockWithTransactions());
  if (!lbs->getBlockByHash(blockId, blockPtr->block)) {
    logger(DEBUGGING) << "<< Core.cpp << " << "Can't find block: " << blockId;
    return std::unique_ptr<BlockWithTransactions>(nullptr);
  }

  blockPtr->transactions.reserve(blockPtr->block.transactionHashes.size());
  std::vector<Crypto::Hash> missedTxs;
  lbs->getTransactions(blockPtr->block.transactionHashes, blockPtr->transactions, missedTxs, true);
  assert(missedTxs.empty() || !lbs->isBlockInMainChain(blockId)); //if can't find transaction for blockchain block -> error

  if (!missedTxs.empty()) {
    logger(DEBUGGING) << "<< Core.cpp << " << "Can't find transactions for block: " << blockId;
    return std::unique_ptr<BlockWithTransactions>(nullptr);
  }

  return std::move(blockPtr);
}

bool core::is_key_image_spent(const Crypto::KeyImage& key_im) {
  return m_blockchain.have_tx_keyimg_as_spent(key_im);
}

bool core::addMessageQueue(MessageQueue<BlockchainMessage>& messageQueue) {
  return m_blockchain.addMessageQueue(messageQueue);
}

bool core::removeMessageQueue(MessageQueue<BlockchainMessage>& messageQueue) {
  return m_blockchain.removeMessageQueue(messageQueue);
}

}

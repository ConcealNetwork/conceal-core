// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "RpcServer.h"

#include <future>
#include <unordered_map>

// cn
#include "BlockchainExplorerData.h"
#include "Common/StringTools.h"
#include "Common/Base58.h"
#include "CryptoNoteCore/TransactionUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/Core.h"
#include "CryptoNoteCore/IBlock.h"
#include "CryptoNoteCore/Miner.h"
#include "CryptoNoteCore/TransactionExtra.h"
#include "CryptoNoteProtocol/ICryptoNoteProtocolQuery.h"

#include "P2p/NetNode.h"

#include "CoreRpcServerErrorCodes.h"
#include "JsonRpc.h"
#include "version.h"

#undef ERROR

using namespace logging;
using namespace crypto;
using namespace common;

const uint64_t BLOCK_LIST_MAX_COUNT = 1000;

namespace cn {

namespace {

template <typename Command>
RpcServer::HandlerFunction binMethod(bool (RpcServer::*handler)(typename Command::request const&, typename Command::response&)) {
  return [handler](RpcServer* obj, const HttpRequest& request, HttpResponse& response) {

    boost::value_initialized<typename Command::request> req;
    boost::value_initialized<typename Command::response> res;

    if (!loadFromBinaryKeyValue(static_cast<typename Command::request&>(req), request.getBody())) {
      return false;
    }

    bool result = (obj->*handler)(req, res);
    response.setBody(storeToBinaryKeyValue(res.data()));
    return result;
  };
}

template <typename Command>
RpcServer::HandlerFunction jsonMethod(bool (RpcServer::*handler)(typename Command::request const&, typename Command::response&)) {
  return [handler](RpcServer* obj, const HttpRequest& request, HttpResponse& response) {

    boost::value_initialized<typename Command::request> req;
    boost::value_initialized<typename Command::response> res;

    if (!loadFromJson(static_cast<typename Command::request&>(req), request.getBody())) {
      return false;
    }

    bool result = (obj->*handler)(req, res);

    std::string cors_domain = obj->getCorsDomain();
    if (!cors_domain.empty()) {
      response.addHeader("Access-Control-Allow-Origin", cors_domain);
      response.addHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
      response.addHeader("Access-Control-Allow-Methods", "POST, GET");
    }
    response.addHeader("Content-Type", "application/json");

    response.setBody(storeToJson(res.data()));
    return result;
  };
}

}

std::unordered_map<std::string, RpcServer::RpcHandler<RpcServer::HandlerFunction>> RpcServer::s_handlers = {

  // binary handlers
  { "/getblocks.bin", { binMethod<COMMAND_RPC_GET_BLOCKS_FAST>(&RpcServer::on_get_blocks), false } },
  { "/queryblocks.bin", { binMethod<COMMAND_RPC_QUERY_BLOCKS>(&RpcServer::on_query_blocks), false } },
  { "/queryblockslite.bin", { binMethod<COMMAND_RPC_QUERY_BLOCKS_LITE>(&RpcServer::on_query_blocks_lite), false } },
  { "/get_o_indexes.bin", { binMethod<COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES>(&RpcServer::on_get_indexes), false } },
  { "/getrandom_outs.bin", { binMethod<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS>(&RpcServer::on_get_random_outs_bin), false } },
  { "/get_pool_changes.bin", { binMethod<COMMAND_RPC_GET_POOL_CHANGES>(&RpcServer::onGetPoolChanges), false } },
  { "/get_pool_changes_lite.bin", { binMethod<COMMAND_RPC_GET_POOL_CHANGES_LITE>(&RpcServer::onGetPoolChangesLite), false } },

  // json handlers
  { "/getinfo", { jsonMethod<COMMAND_RPC_GET_INFO>(&RpcServer::on_get_info), true } },
  { "/getheight", { jsonMethod<COMMAND_RPC_GET_HEIGHT>(&RpcServer::on_get_height), true } },
  { "/gettransactions", { jsonMethod<COMMAND_RPC_GET_TRANSACTIONS>(&RpcServer::on_get_transactions), false } },
  { "/sendrawtransaction", { jsonMethod<COMMAND_RPC_SEND_RAW_TX>(&RpcServer::on_send_raw_tx), false } },
  { "/feeaddress", { jsonMethod<COMMAND_RPC_GET_FEE_ADDRESS>(&RpcServer::on_get_fee_address), true } },
  { "/peers", { jsonMethod<COMMAND_RPC_GET_PEER_LIST>(&RpcServer::on_get_peer_list), true } },
  { "/getpeers", { jsonMethod<COMMAND_RPC_GET_PEER_LIST>(&RpcServer::on_get_peer_list), true } },
  { "/get_raw_transactions_by_heights", { jsonMethod<COMMAND_RPC_GET_TRANSACTIONS_WITH_OUTPUT_GLOBAL_INDEXES>(&RpcServer::on_get_txs_with_output_global_indexes), true } },
  { "/getrawtransactionspool", { jsonMethod<COMMAND_RPC_GET_RAW_TRANSACTIONS_POOL>(&RpcServer::on_get_transactions_pool_raw), true } },
  { "/getrandom_outs", { jsonMethod<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_JSON>(&RpcServer::on_get_random_outs_json), false } },

  // json rpc
  { "/json_rpc", { std::bind(&RpcServer::processJsonRpcRequest, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), true } }
};

RpcServer::RpcServer(platform_system::Dispatcher& dispatcher, logging::ILogger& log, core& c, NodeServer& p2p, const ICryptoNoteProtocolQuery& protocolQuery) :
  HttpServer(dispatcher, log), logger(log, "RpcServer"), m_core(c), m_p2p(p2p), m_protocolQuery(protocolQuery) {
}

void RpcServer::processRequest(const HttpRequest& request, HttpResponse& response) {
  auto url = request.getUrl();

  auto it = s_handlers.find(url);
  if (it == s_handlers.end()) {
    response.setStatus(HttpResponse::STATUS_404);
    return;
  }

  if (!it->second.allowBusyCore && !isCoreReady()) {
    response.setStatus(HttpResponse::STATUS_500);
    response.setBody("Core is busy");
    return;
  }

  it->second.handler(this, request, response);
}

bool RpcServer::processJsonRpcRequest(const HttpRequest& request, HttpResponse& response) {

  using namespace JsonRpc;

  response.addHeader("Content-Type", "application/json");
  if (!m_cors_domain.empty()) {
    response.addHeader("Access-Control-Allow-Origin", m_cors_domain);
    response.addHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
    response.addHeader("Access-Control-Allow-Methods", "POST, GET");
  }  

  JsonRpcRequest jsonRequest;
  JsonRpcResponse jsonResponse;

  try {
    logger(TRACE) << "JSON-RPC request: " << request.getBody();
    jsonRequest.parseRequest(request.getBody());
    jsonResponse.setId(jsonRequest.getId()); // copy id

    static std::unordered_map<std::string, RpcServer::RpcHandler<JsonMemberMethod>> jsonRpcHandlers = {
        {"getaltblockslist", {makeMemberMethod(&RpcServer::on_alt_blocks_list_json), true}},
        {"f_blocks_list_json", {makeMemberMethod(&RpcServer::f_on_blocks_list_json), false}},
        {"f_block_json", {makeMemberMethod(&RpcServer::f_on_block_json), false}},
        {"f_transaction_json", {makeMemberMethod(&RpcServer::f_on_transaction_json), false}},
        {"f_on_transactions_pool_json", {makeMemberMethod(&RpcServer::f_on_transactions_pool_json), false}},
        {"check_tx_proof", {makeMemberMethod(&RpcServer::k_on_check_tx_proof), false}},
        {"check_reserve_proof", {makeMemberMethod(&RpcServer::k_on_check_reserve_proof), false}},
        {"getblockcount", {makeMemberMethod(&RpcServer::on_getblockcount), true}},
        {"getblockhash", {makeMemberMethod(&RpcServer::on_getblockhash), true}},
        {"getblockbyheight", {makeMemberMethod(&RpcServer::on_get_block_details_by_height), true}},
        {"on_getblockhash", {makeMemberMethod(&RpcServer::on_getblockhash), false}},
        {"getblocktemplate", {makeMemberMethod(&RpcServer::on_getblocktemplate), false}},
        {"getcurrencyid", {makeMemberMethod(&RpcServer::on_get_currency_id), true}},
        {"submitblock", {makeMemberMethod(&RpcServer::on_submitblock), false}},
        {"getlastblockheader", {makeMemberMethod(&RpcServer::on_get_last_block_header), false}},
        {"getblockheaderbyhash", {makeMemberMethod(&RpcServer::on_get_block_header_by_hash), false}},
        {"getblocktimestamp", {makeMemberMethod(&RpcServer::on_get_block_timestamp_by_height), true}},
        {"getblockheaderbyheight", {makeMemberMethod(&RpcServer::on_get_block_header_by_height), false}},
        {"getrawtransactionspool", {makeMemberMethod(&RpcServer::on_get_transactions_pool_raw), true}},
        {"getrawtransactionsbyheights", {makeMemberMethod(&RpcServer::on_get_txs_with_output_global_indexes), true}}
    };

    auto it = jsonRpcHandlers.find(jsonRequest.getMethod());
    if (it == jsonRpcHandlers.end()) {
      throw JsonRpcError(JsonRpc::errMethodNotFound);
    }

    if (!it->second.allowBusyCore && !isCoreReady()) {
      throw JsonRpcError(CORE_RPC_ERROR_CODE_CORE_BUSY, "Core is busy");
    }

    it->second.handler(this, jsonRequest, jsonResponse);

  } catch (const JsonRpcError& err) {
    jsonResponse.setError(err);
  } catch (const std::exception& e) {
    jsonResponse.setError(JsonRpcError(JsonRpc::errInternalError, e.what()));
  }

  response.setBody(jsonResponse.getBody());
  logger(TRACE) << "JSON-RPC response: " << jsonResponse.getBody();
  return true;
}

bool RpcServer::isCoreReady() {
  return m_core.currency().isTestnet() || m_p2p.get_payload_object().isSynchronized();
}

bool RpcServer::enableCors(const std::string domain) {
  m_cors_domain = domain;
  return true;
}

std::string RpcServer::getCorsDomain() {
  return m_cors_domain;
}

//
// Binary handlers
//

bool RpcServer::on_get_blocks(const COMMAND_RPC_GET_BLOCKS_FAST::request& req, COMMAND_RPC_GET_BLOCKS_FAST::response& res) {
  // TODO code duplication see InProcessNode::doGetNewBlocks()
  if (req.block_ids.empty()) {
    res.status = "Failed";
    return false;
  }

  if (req.block_ids.back() != m_core.getBlockIdByHeight(0)) {
    res.status = "Failed";
    return false;
  }

  uint32_t totalBlockCount;
  uint32_t startBlockIndex;
  std::vector<crypto::Hash> supplement = m_core.findBlockchainSupplement(req.block_ids, COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT, totalBlockCount, startBlockIndex);

  res.current_height = totalBlockCount;
  res.start_height = startBlockIndex;

  for (const auto& blockId : supplement) {
    assert(m_core.have_block(blockId));
    auto completeBlock = m_core.getBlock(blockId);
    assert(completeBlock != nullptr);

    res.blocks.resize(res.blocks.size() + 1);
    res.blocks.back().block = asString(toBinaryArray(completeBlock->getBlock()));

    res.blocks.back().txs.reserve(completeBlock->getTransactionCount());
    for (size_t i = 0; i < completeBlock->getTransactionCount(); ++i) {
      res.blocks.back().txs.push_back(asString(toBinaryArray(completeBlock->getTransaction(i))));
    }
  }

  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_get_block_timestamp_by_height(const COMMAND_RPC_GET_BLOCK_TIMESTAMP_BY_HEIGHT::request &req, COMMAND_RPC_GET_BLOCK_TIMESTAMP_BY_HEIGHT::response &res)
{
  if (m_core.get_current_blockchain_height() <= req.height)
  {
    throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT,
                                std::string("To big height: ") + std::to_string(req.height) + ", current blockchain height = " + std::to_string(m_core.get_current_blockchain_height())};
  }

  res.status = CORE_RPC_STATUS_OK;

  m_core.getBlockTimestamp(req.height, res.timestamp);

  return true;
}

bool RpcServer::on_getblockhash(const COMMAND_RPC_GETBLOCKHASH::request &req, COMMAND_RPC_GETBLOCKHASH::response &res)
{
  if (req.size() != 1)
  {
    throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM, "Wrong parameters, expected height"};
  }

  uint32_t h = static_cast<uint32_t>(req[0]);
  crypto::Hash blockId = m_core.getBlockIdByHeight(h);
  if (blockId == NULL_HASH)
  {
    throw JsonRpc::JsonRpcError{
      CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT,
          std::string("To big height: ") + std::to_string(h) + ", current blockchain height = " + std::to_string(m_core.get_current_blockchain_height())
    };
  }

  res = common::podToHex(blockId);
  return true;
}

bool RpcServer::fill_f_block_details_response(const crypto::Hash &hash, f_block_details_response &block)
{
  Block blk;
  if (!m_core.getBlockByHash(hash, blk))
  {
    throw JsonRpc::JsonRpcError{
        CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
        "Internal error: can't get block by hash."};
  }

  if (blk.baseTransaction.inputs.front().type() != typeid(BaseInput))
  {
    throw JsonRpc::JsonRpcError{
        CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
        "Internal error: coinbase transaction in the block has the wrong type"};
  }

  block_header_response block_header;

  uint32_t block_height = boost::get<BaseInput>(blk.baseTransaction.inputs.front()).blockIndex;
  block.height = block_height;
  crypto::Hash tmp_hash = m_core.getBlockIdByHeight(block_height);
  bool is_orphaned = hash != tmp_hash;

  fill_block_header_response(blk, is_orphaned, block_height, hash, block_header);

  block.major_version = block_header.major_version;
  block.minor_version = block_header.minor_version;
  block.timestamp = block_header.timestamp;
  block.prev_hash = block_header.prev_hash;
  block.nonce = block_header.nonce;
  block.hash = common::podToHex(hash);
  block.orphan_status = is_orphaned;
  block.depth = m_core.get_current_blockchain_height() - block.height - 1;
  m_core.getBlockDifficulty(static_cast<uint32_t>(block.height), block.difficulty);

  block.reward = block_header.reward;

  std::vector<size_t> blocksSizes;
  if (!m_core.getBackwardBlocksSizes(block.height, blocksSizes, parameters::CRYPTONOTE_REWARD_BLOCKS_WINDOW))
  {
    return false;
  }
  block.sizeMedian = common::medianValue(blocksSizes);

  size_t blockSize = 0;
  if (!m_core.getBlockSize(hash, blockSize))
  {
    return false;
  }
  block.transactionsCumulativeSize = blockSize;

  size_t blokBlobSize = getObjectBinarySize(blk);
  size_t minerTxBlobSize = getObjectBinarySize(blk.baseTransaction);
  block.blockSize = blokBlobSize + block.transactionsCumulativeSize - minerTxBlobSize;

  uint64_t alreadyGeneratedCoins;
  if (!m_core.getAlreadyGeneratedCoins(hash, alreadyGeneratedCoins))
  {
    return false;
  }
  block.alreadyGeneratedCoins = std::to_string(alreadyGeneratedCoins);

  if (!m_core.getGeneratedTransactionsNumber(block.height, block.alreadyGeneratedTransactions))
  {
    return false;
  }

  uint64_t prevBlockGeneratedCoins = 0;
  if (block.height > 0)
  {
    if (!m_core.getAlreadyGeneratedCoins(blk.previousBlockHash, prevBlockGeneratedCoins))
    {
      return false;
    }
  }
  uint64_t maxReward = 0;
  uint64_t currentReward = 0;
  int64_t emissionChange = 0;
  block.effectiveSizeMedian = std::max(block.sizeMedian, m_core.currency().blockGrantedFullRewardZone());

  if (!m_core.getBlockReward(block.sizeMedian, 0, prevBlockGeneratedCoins, 0, block.height, maxReward, emissionChange))
  {
    return false;
  }
  if (!m_core.getBlockReward(block.sizeMedian, block.transactionsCumulativeSize, prevBlockGeneratedCoins, 0, block.height, currentReward, emissionChange))
  {
    return false;
  }

  block.baseReward = maxReward;
  if (maxReward == 0 && currentReward == 0)
  {
    block.penalty = static_cast<double>(0);
  }
  else
  {
    if (maxReward < currentReward)
    {
      return false;
    }
    block.penalty = static_cast<double>(maxReward - currentReward) / static_cast<double>(maxReward);
  }

  // Base transaction adding
  f_transaction_short_response base_transaction;
  base_transaction.hash = common::podToHex(getObjectHash(blk.baseTransaction));
  base_transaction.fee = 0;
  base_transaction.amount_out = get_outs_money_amount(blk.baseTransaction);
  base_transaction.size = getObjectBinarySize(blk.baseTransaction);
  block.transactions.push_back(base_transaction);

  std::list<crypto::Hash> missed_txs;
  std::list<Transaction> txs;
  m_core.getTransactions(blk.transactionHashes, txs, missed_txs);

  block.totalFeeAmount = 0;

  for (const Transaction &tx : txs)
  {
    f_transaction_short_response transaction_short;
    uint64_t amount_in = 0;
    get_inputs_money_amount(tx, amount_in);
    uint64_t amount_out = get_outs_money_amount(tx);

    transaction_short.hash = common::podToHex(getObjectHash(tx));
    transaction_short.fee = m_core.currency().getTransactionFee(tx, static_cast<uint32_t>(block.height));
    transaction_short.amount_out = amount_out;
    transaction_short.size = getObjectBinarySize(tx);
    block.transactions.push_back(transaction_short);

    block.totalFeeAmount += transaction_short.fee;
  }
  return true;
}

bool RpcServer::on_get_block_details_by_height(const COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT::request &req, COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT::response &res)
{
  f_block_details_response blockDetails;
  if (m_core.get_current_blockchain_height() <= req.height)
  {
    throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT,
                                std::string("Too big height: ") + std::to_string(req.height) + ", current blockchain height = " + std::to_string(m_core.get_current_blockchain_height() - 1)};
  }
  crypto::Hash hash = m_core.getBlockIdByHeight(req.height);

  if (!fill_f_block_details_response(hash, blockDetails))
  {
    throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
                                std::string("Unable to fill block details.")};
  }

  res.block = blockDetails;
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_get_txs_with_output_global_indexes(const COMMAND_RPC_GET_TRANSACTIONS_WITH_OUTPUT_GLOBAL_INDEXES::request& req, COMMAND_RPC_GET_TRANSACTIONS_WITH_OUTPUT_GLOBAL_INDEXES::response& rsp) {
  try {
    std::vector<uint32_t> heights;
    
    if (req.range) {
      if (req.heights.size() != 2) {
        throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_WRONG_PARAM,
          std::string("The range is set to true but heights size is not equal to 2") };
      }
      std::vector<uint32_t> range = req.heights;

      if (range.back() < range.front())
      {
        throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM,
                                    std::string("Invalid heights range: ") + std::to_string(range.front()) + " must be < " + std::to_string(range.back())};
      }

      if (range.back() - range.front() > BLOCK_LIST_MAX_COUNT) {
        throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_WRONG_PARAM,
          std::string("Requested blocks count: ") + std::to_string(range.back() - range.front()) + " exceeded max limit of " + std::to_string(BLOCK_LIST_MAX_COUNT) };
      }

      uint32_t upperBound = std::min(range[1], m_core.get_current_blockchain_height());
      for (size_t i = 0; i < (upperBound - range[0]); i++) {
        heights.push_back(range[0] + i);
      }
    }
    else {
      if (req.heights.size() > BLOCK_LIST_MAX_COUNT) {
        throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_WRONG_PARAM,
          std::string("Requested blocks count: ") + std::to_string(req.heights.size()) + " exceeded max limit of " + std::to_string(BLOCK_LIST_MAX_COUNT) };
      }

      heights = req.heights;
    }

    for (const uint32_t& height : heights) {
      if (m_core.get_current_blockchain_height() <= height) {
        throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT,
          std::string("To big height: ") + std::to_string(height) + ", current blockchain height = " + std::to_string(m_core.get_current_blockchain_height() - 1) };
      }

      crypto::Hash block_hash = m_core.getBlockIdByHeight(height);
      Block blk;
      if (!m_core.getBlockByHash(block_hash, blk)) {
        throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Internal error: can't get block by height " + std::to_string(height) + '.' };
      }

      std::vector<crypto::Hash> txs_ids;

      if (req.include_miner_txs) {
        txs_ids.reserve(blk.transactionHashes.size() + 1);
        txs_ids.push_back(getObjectHash(blk.baseTransaction));
      }
      else {
        txs_ids.reserve(blk.transactionHashes.size());
      }
      if (!blk.transactionHashes.empty()) {
        txs_ids.insert(txs_ids.end(), blk.transactionHashes.begin(), blk.transactionHashes.end());
      }

      std::vector<crypto::Hash>::const_iterator ti = txs_ids.begin();

      std::vector<std::pair<Transaction, std::vector<uint32_t>>> txs;
      std::list<crypto::Hash> missed;

      if (!txs_ids.empty()) {
        if (!m_core.getTransactionsWithOutputGlobalIndexes(txs_ids, missed, txs)) {
          throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Error getting transactions with output global indexes" };
        }

        for (const auto &txi : txs) {
          rsp.transactions.push_back(tx_with_output_global_indexes());
          tx_with_output_global_indexes &e = rsp.transactions.back();

          e.hash = *ti++;
          e.block_hash = block_hash;
          e.height = height;
          e.timestamp = blk.timestamp;
          e.transaction = *static_cast<const TransactionPrefix*>(&txi.first);
          e.output_indexes = txi.second;
          e.fee = m_core.currency().getTransactionFee(txi.first, height);
        }
      }

      for (const auto& miss_tx : missed) {
        rsp.missed_txs.push_back(common::podToHex(miss_tx));
      }
    }
  }
  catch (std::system_error& e) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR, e.what() };
    return false;
  }
  catch (std::exception& e) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Error: " + std::string(e.what()) };
    return false;
  }
  rsp.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_get_transactions_pool_raw(const COMMAND_RPC_GET_RAW_TRANSACTIONS_POOL::request& req, COMMAND_RPC_GET_RAW_TRANSACTIONS_POOL::response& res) {
  auto pool = m_core.getMemoryPool();

  for (const auto& txd : pool) {
    res.transactions.push_back(tx_with_output_global_indexes());
    tx_with_output_global_indexes &e = res.transactions.back();

    e.hash = txd.id;
    e.height = boost::value_initialized<uint32_t>();
    e.block_hash = boost::value_initialized<crypto::Hash>();
    e.timestamp = txd.receiveTime;
    e.transaction = *static_cast<const TransactionPrefix*>(&txd.tx);
    e.fee = txd.fee;
  }
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::k_on_check_tx_proof(const K_COMMAND_RPC_CHECK_TX_PROOF::request& req, K_COMMAND_RPC_CHECK_TX_PROOF::response& res) {
	// parse txid
	crypto::Hash txid;
	if (!parse_hash256(req.tx_id, txid)) {
		throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_WRONG_PARAM, "Failed to parse txid" };
	}
	// parse address
	cn::AccountPublicAddress address;
	if (!m_core.currency().parseAccountAddressString(req.dest_address, address)) {
		throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_WRONG_PARAM, "Failed to parse address " + req.dest_address + '.' };
	}
	// parse pubkey r*A & signature
  const std::string prefix = "ProofV1";
	const size_t header_len = prefix.size();
	if (req.signature.size() < header_len || req.signature.substr(0, header_len) != prefix) {
		throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_WRONG_PARAM, "Signature header check error" };
	}
	crypto::PublicKey rA;
	crypto::Signature sig;
	const size_t rA_len = tools::base_58::encode(std::string((const char *)&rA, sizeof(crypto::PublicKey))).size();
	const size_t sig_len = tools::base_58::encode(std::string((const char *)&sig, sizeof(crypto::Signature))).size();
	std::string rA_decoded;
	std::string sig_decoded;
	if (!tools::base_58::decode(req.signature.substr(header_len, rA_len), rA_decoded)) {
		throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_WRONG_PARAM, "Signature decoding error" };
	}
	if (!tools::base_58::decode(req.signature.substr(header_len + rA_len, sig_len), sig_decoded)) {
		throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_WRONG_PARAM, "Signature decoding error" };
	}
	if (sizeof(crypto::PublicKey) != rA_decoded.size() || sizeof(crypto::Signature) != sig_decoded.size()) {
		throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_WRONG_PARAM, "Signature decoding error" };
	}
	memcpy(&rA, rA_decoded.data(), sizeof(crypto::PublicKey));
	memcpy(&sig, sig_decoded.data(), sizeof(crypto::Signature));

	// fetch tx pubkey
	Transaction tx;

	std::vector<uint32_t> out;
	std::vector<crypto::Hash> tx_ids;
	tx_ids.push_back(txid);
	std::list<crypto::Hash> missed_txs;
	std::list<Transaction> txs;
	m_core.getTransactions(tx_ids, txs, missed_txs, true);

	if (1 == txs.size()) {
		tx = txs.front();
	}
	else {
		throw JsonRpc::JsonRpcError{
			CORE_RPC_ERROR_CODE_WRONG_PARAM,
			"transaction wasn't found. Hash = " + req.tx_id + '.' };
	}
	cn::TransactionPrefix transaction = *static_cast<const TransactionPrefix*>(&tx);

	crypto::PublicKey R = getTransactionPublicKeyFromExtra(transaction.extra);
	if (R == NULL_PUBLIC_KEY)
	{
		throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Tx pubkey was not found" };
	}

	// check signature
	bool r = crypto::check_tx_proof(txid, R, address.viewPublicKey, rA, sig);
	res.signature_valid = r;

	if (r) {

		// obtain key derivation by multiplying scalar 1 to the pubkey r*A included in the signature
		crypto::KeyDerivation derivation;
    if (!crypto::generate_key_derivation(rA, crypto::EllipticCurveScalar2SecretKey(crypto::I), derivation))
    {
      throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Failed to generate key derivation" };
    }

		// look for outputs
		uint64_t received(0);
		size_t keyIndex(0);
		std::vector<TransactionOutput> outputs;
		try {
			for (const TransactionOutput& o : transaction.outputs) {
				if (o.target.type() == typeid(KeyOutput)) {
					const KeyOutput out_key = boost::get<KeyOutput>(o.target);
					crypto::PublicKey pubkey;
					derive_public_key(derivation, keyIndex, address.spendPublicKey, pubkey);
					if (pubkey == out_key.key) {
						received += o.amount;
						outputs.push_back(o);
					}
				}
				++keyIndex;
			}
		}
		catch (...)
		{
			throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Unknown error" };
		}
		res.received_amount = received;
		res.outputs = outputs;

		crypto::Hash blockHash;
		uint32_t blockHeight;
		if (m_core.getBlockContainingTx(txid, blockHash, blockHeight)) {
			res.confirmations = m_protocolQuery.getObservedHeight() - blockHeight;
		}
	}
	else {
		res.received_amount = 0;
	}

	res.status = CORE_RPC_STATUS_OK;
	return true;
}

bool RpcServer::k_on_check_reserve_proof(const K_COMMAND_RPC_CHECK_RESERVE_PROOF::request& req, K_COMMAND_RPC_CHECK_RESERVE_PROOF::response& res) {

  // parse address
  cn::AccountPublicAddress address;
  if (!m_core.currency().parseAccountAddressString(req.address, address))
  {
    throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_WRONG_PARAM, "Failed to parse address " + req.address + '.'};
  }

  // parse signature
  const std::string header = "ReserveProofV1";
  const size_t header_len = header.size();
  if (req.signature.size() < header_len || req.signature.substr(0, header_len) != header)
  {
    throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Signature header check error"};
  }

  std::string sig_decoded;
  if (!tools::base_58::decode(req.signature.substr(header_len), sig_decoded))
  {
    throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Signature decoding error"};
  }

  BinaryArray ba;
  if (!common::fromHex(sig_decoded, ba))
  {
    throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Proof decoding error"};
  }
  reserve_proof proof_decoded;
  if (!fromBinaryArray(proof_decoded, ba))
  {
    throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "BinaryArray decoding error"};
  }

  std::vector<reserve_proof_entry> &proofs = proof_decoded.proofs;

  // compute signature prefix hash
  std::string prefix_data = req.message;
  prefix_data.append((const char *)&address, sizeof(cn::AccountPublicAddress));
  for (const auto &proof : proofs)
  {
    prefix_data.append((const char *)&proof.key_image, sizeof(crypto::PublicKey));
  }
  crypto::Hash prefix_hash;
  crypto::cn_fast_hash(prefix_data.data(), prefix_data.size(), prefix_hash);

  // fetch txes
  std::vector<crypto::Hash> transactionHashes;
  for (const auto &proof : proofs)
  {
    transactionHashes.push_back(proof.txid);
  }
  std::list<Hash> missed_txs;
  std::list<Transaction> txs;
  m_core.getTransactions(transactionHashes, txs, missed_txs);
  if (!missed_txs.empty())
  {
    throw JsonRpc::JsonRpcError(CORE_RPC_ERROR_CODE_WRONG_PARAM, std::string("Couldn't find some transactions of reserve proof"));
  }
  std::vector<Transaction> transactions;
  std::copy(txs.begin(), txs.end(), std::inserter(transactions, transactions.end()));

  // check spent status
  res.total = 0;
  res.spent = 0;
  for (size_t i = 0; i < proofs.size(); ++i)
  {
    const reserve_proof_entry &proof = proofs[i];

    cn::TransactionPrefix tx = *static_cast<const TransactionPrefix *>(&transactions[i]);

    if (proof.index_in_tx >= tx.outputs.size())
    {
      throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "index_in_tx is out of bound"};
    }

    const KeyOutput out_key = boost::get<KeyOutput>(tx.outputs[proof.index_in_tx].target);

    // get tx pub key
    crypto::PublicKey txPubKey = getTransactionPublicKeyFromExtra(tx.extra);

    // check singature for shared secret
    if (!crypto::check_tx_proof(prefix_hash, address.viewPublicKey, txPubKey, proof.shared_secret, proof.shared_secret_sig))
    {
      res.good = false;
      return true;
    }

    // check signature for key image
    const std::vector<const crypto::PublicKey *> &pubs = {&out_key.key};
    if (!crypto::check_ring_signature(prefix_hash, proof.key_image, &pubs[0], 1, &proof.key_image_sig))
    {
      res.good = false;
      return true;
    }

    // check if the address really received the fund
    crypto::KeyDerivation derivation;
    if (!crypto::generate_key_derivation(proof.shared_secret, crypto::EllipticCurveScalar2SecretKey(crypto::I), derivation))
    {
      throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Failed to generate key derivation"};
    }
    try
    {
      crypto::PublicKey pubkey;
      derive_public_key(derivation, proof.index_in_tx, address.spendPublicKey, pubkey);
      if (pubkey == out_key.key)
      {
        uint64_t amount = tx.outputs[proof.index_in_tx].amount;
        res.total += amount;

        if (m_core.is_key_image_spent(proof.key_image))
        {
          res.spent += amount;
        }
      }
    }
    catch (...)
    {
      throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Unknown error"};
    }
  }

  // check signature for address spend keys
  crypto::Signature sig = proof_decoded.signature;
  if (!crypto::check_signature(prefix_hash, address.spendPublicKey, sig))
  {
    res.good = false;
    return true;
  }

  res.good = true;

  return true;
}

bool RpcServer::on_query_blocks(const COMMAND_RPC_QUERY_BLOCKS::request& req, COMMAND_RPC_QUERY_BLOCKS::response& res) {
  uint32_t startHeight;
  uint32_t currentHeight;
  uint32_t fullOffset;

  if (!m_core.queryBlocks(req.block_ids, req.timestamp, startHeight, currentHeight, fullOffset, res.items)) {
    res.status = "Failed to perform query";
    return false;
  }

  res.start_height = startHeight;
  res.current_height = currentHeight;
  res.full_offset = fullOffset;
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_query_blocks_lite(const COMMAND_RPC_QUERY_BLOCKS_LITE::request& req, COMMAND_RPC_QUERY_BLOCKS_LITE::response& res) {
  uint32_t startHeight;
  uint32_t currentHeight;
  uint32_t fullOffset;
  if (!m_core.queryBlocksLite(req.blockIds, req.timestamp, startHeight, currentHeight, fullOffset, res.items)) {
    res.status = "Failed to perform query";
    return false;
  }

  res.startHeight = startHeight;
  res.currentHeight = currentHeight;
  res.fullOffset = fullOffset;
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::setFeeAddress(const std::string& fee_address, const AccountPublicAddress& fee_acc) {
  m_fee_address = fee_address;
  m_fee_acc = fee_acc;
  return true;
}

bool RpcServer::setViewKey(const std::string& view_key) {
  crypto::Hash private_view_key_hash;
  size_t size;
  if (!common::fromHex(view_key, &private_view_key_hash, sizeof(private_view_key_hash), size) || size != sizeof(private_view_key_hash)) {
    logger(INFO) << "Could not parse private view key";
    return false;
  }
  m_view_key = *(struct crypto::SecretKey *) &private_view_key_hash;
  return true;
}

bool RpcServer::on_get_fee_address(const COMMAND_RPC_GET_FEE_ADDRESS::request& req, COMMAND_RPC_GET_FEE_ADDRESS::response& res) {
  if (m_fee_address.empty()) {
	  res.status = CORE_RPC_STATUS_OK;
	  return false; 
  }
  res.fee_address = m_fee_address;
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_get_indexes(const COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::request& req, COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::response& res) {
  std::vector<uint32_t> outputIndexes;
  if (!m_core.get_tx_outputs_gindexs(req.txid, outputIndexes)) {
    res.status = "Failed";
    return true;
  }

  res.o_indexes.assign(outputIndexes.begin(), outputIndexes.end());
  res.status = CORE_RPC_STATUS_OK;
  logger(TRACE) << "COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES: [" << res.o_indexes.size() << "]";
  return true;
}

bool RpcServer::on_get_random_outs_bin(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request& req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response& res) {
  res.status = "Failed";
  if (!m_core.get_random_outs_for_amounts(req, res)) {
    return true;
  }

  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_get_random_outs_json(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_JSON::request& req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_JSON::response& res) {
  res.status = "Failed";
  
  COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response bin;

  if (!m_core.get_random_outs_for_amounts(req, bin)) {
    return true;
  }

  std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount> &outs = bin.outs;
  res.outs.reserve(outs.size());
  for (size_t i = 0; i < outs.size(); ++i) {
    COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_JSON::outs_for_amount out;
    out.amount = bin.outs[i].amount;
    for (auto& o : outs[i].outs) {
      out.outs.push_back(static_cast<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_JSON::out_entry&>(o));
    }
    res.outs.push_back(out);
  }

  res.status = CORE_RPC_STATUS_OK;

  return true;
}

bool RpcServer::onGetPoolChanges(const COMMAND_RPC_GET_POOL_CHANGES::request& req, COMMAND_RPC_GET_POOL_CHANGES::response& rsp) {
  rsp.status = CORE_RPC_STATUS_OK;
  std::vector<cn::Transaction> addedTransactions;
  rsp.isTailBlockActual = m_core.getPoolChanges(req.tailBlockId, req.knownTxsIds, addedTransactions, rsp.deletedTxsIds);
  for (auto& tx : addedTransactions) {
    BinaryArray txBlob;
    if (!toBinaryArray(tx, txBlob)) {
      rsp.status = "Internal error";
      break;;
    }

    rsp.addedTxs.emplace_back(std::move(txBlob));
  }
  return true;
}


bool RpcServer::onGetPoolChangesLite(const COMMAND_RPC_GET_POOL_CHANGES_LITE::request& req, COMMAND_RPC_GET_POOL_CHANGES_LITE::response& rsp) {
  rsp.status = CORE_RPC_STATUS_OK;
  rsp.isTailBlockActual = m_core.getPoolChangesLite(req.tailBlockId, req.knownTxsIds, rsp.addedTxs, rsp.deletedTxsIds);

  return true;
}

//
// JSON handlers
//


bool RpcServer::on_get_peer_list(const COMMAND_RPC_GET_PEER_LIST::request& req, COMMAND_RPC_GET_PEER_LIST::response& res) {
	std::list<PeerlistEntry> pl_wite;
	std::list<PeerlistEntry> pl_gray;
	m_p2p.getPeerlistManager().get_peerlist_full(pl_gray, pl_wite);
	for (const auto& pe : pl_wite) {
		std::stringstream ss;
		ss << pe.adr;
		res.peers.push_back(ss.str());
	}
	res.status = CORE_RPC_STATUS_OK;
	return true;
}

bool RpcServer::on_get_info(const COMMAND_RPC_GET_INFO::request& req, COMMAND_RPC_GET_INFO::response& res) {
  res.height = m_core.get_current_blockchain_height();
  res.difficulty = m_core.getNextBlockDifficulty();
  res.tx_count = m_core.get_blockchain_total_transactions() - res.height; //without coinbase
  res.tx_pool_size = m_core.get_pool_transactions_count();
  res.alt_blocks_count = m_core.get_alternative_blocks_count();
  res.fee_address = m_fee_address.empty() ? std::string() : m_fee_address;
  uint64_t total_conn = m_p2p.get_connections_count();
  res.outgoing_connections_count = m_p2p.get_outgoing_connections_count();
  res.incoming_connections_count = total_conn - res.outgoing_connections_count;
  res.white_peerlist_size = m_p2p.getPeerlistManager().get_white_peers_count();
  res.grey_peerlist_size = m_p2p.getPeerlistManager().get_gray_peers_count();
  res.last_known_block_index = std::max(static_cast<uint32_t>(1), m_protocolQuery.getObservedHeight()) - 1;
  res.full_deposit_amount = m_core.fullDepositAmount();
  res.status = CORE_RPC_STATUS_OK;
  crypto::Hash last_block_hash = m_core.getBlockIdByHeight(m_core.get_current_blockchain_height() - 1);
  res.top_block_hash = common::podToHex(last_block_hash);
  res.version = PROJECT_VERSION;

  Block blk;
  if (!m_core.getBlockByHash(last_block_hash, blk)) {
	  throw JsonRpc::JsonRpcError{
		CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
		"Internal error: can't get last block by hash." };
  }

  if (blk.baseTransaction.inputs.front().type() != typeid(BaseInput)) {
	  throw JsonRpc::JsonRpcError{
		CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
		"Internal error: coinbase transaction in the block has the wrong type" };
  }

  block_header_response block_header;
  uint32_t last_block_height = boost::get<BaseInput>(blk.baseTransaction.inputs.front()).blockIndex;

  crypto::Hash tmp_hash = m_core.getBlockIdByHeight(last_block_height);
  bool is_orphaned = last_block_hash != tmp_hash;
  fill_block_header_response(blk, is_orphaned, last_block_height, last_block_hash, block_header);

  res.block_major_version = block_header.major_version;
  res.block_minor_version = block_header.minor_version;
  res.last_block_timestamp = block_header.timestamp;
  res.last_block_reward = block_header.reward;
  m_core.getBlockDifficulty(static_cast<uint32_t>(last_block_height), res.last_block_difficulty);

  res.connections = m_p2p.get_payload_object().all_connections();
  return true;
}

bool RpcServer::on_get_height(const COMMAND_RPC_GET_HEIGHT::request& req, COMMAND_RPC_GET_HEIGHT::response& res) {
  res.height = m_core.get_current_blockchain_height();
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_get_transactions(const COMMAND_RPC_GET_TRANSACTIONS::request& req, COMMAND_RPC_GET_TRANSACTIONS::response& res) {
  std::vector<Hash> vh;
  for (const auto& tx_hex_str : req.txs_hashes) {
    BinaryArray b;
    if (!fromHex(tx_hex_str, b))
    {
      res.status = "Failed to parse hex representation of transaction hash";
      return true;
    }
    if (b.size() != sizeof(Hash))
    {
      res.status = "Failed, size of data mismatch";
    }
    vh.push_back(*reinterpret_cast<const Hash*>(b.data()));
  }
  std::list<Hash> missed_txs;
  std::list<Transaction> txs;
  m_core.getTransactions(vh, txs, missed_txs);

  for (auto& tx : txs) {
    res.txs_as_hex.push_back(toHex(toBinaryArray(tx)));
  }

  for (const auto& miss_tx : missed_txs) {
    res.missed_tx.push_back(common::podToHex(miss_tx));
  }

  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_send_raw_tx(const COMMAND_RPC_SEND_RAW_TX::request& req, COMMAND_RPC_SEND_RAW_TX::response& res) {
  BinaryArray tx_blob;
  if (!fromHex(req.tx_as_hex, tx_blob))
  {
    logger(INFO) << "[on_send_raw_tx]: Failed to parse tx from hexbuff: " << req.tx_as_hex;
    res.status = "Failed";
    return true;
  }

  if (tx_blob.size() > m_core.currency().transactionMaxSize())
  {
    logger(INFO) << "[on_send_raw_tx]: Too big size: " << tx_blob.size();
    res.status = "Too big";
    return true;
  }

  tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
  if (!m_core.handle_incoming_tx(tx_blob, tvc, false))
  {
    logger(INFO) << "[on_send_raw_tx]: Failed to process tx";
    res.status = "Failed";
    return true;
  }

  if (tvc.m_verification_failed)
  {
    logger(INFO) << "[on_send_raw_tx]: tx verification failed";
    res.status = "Failed";
    return true;
  }

  if (!tvc.m_should_be_relayed)
  {
    logger(INFO) << "[on_send_raw_tx]: tx accepted, but not relayed";
    res.status = "Not relayed";
    return true;
  }

  /* check tx for node fee

  if (!m_fee_address.empty() && m_view_key != NULL_SECRET_KEY) {
    if (!remotenode_check_incoming_tx(tx_blob)) {
      logger(INFO) << "Transaction not relayed due to lack of remote node fee";		
      res.status = "Not relayed due to lack of node fee";
      return true;
    }
  }

  */

  NOTIFY_NEW_TRANSACTIONS::request r;
  r.txs.push_back(asString(tx_blob));
  m_core.get_protocol()->relay_transactions(r);
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_start_mining(const COMMAND_RPC_START_MINING::request& req, COMMAND_RPC_START_MINING::response& res) {
  AccountPublicAddress adr;
  if (!m_core.currency().parseAccountAddressString(req.miner_address, adr)) {
    res.status = "Failed, wrong address";
    return true;
  }

  if (!m_core.get_miner().start(adr, static_cast<size_t>(req.threads_count))) {
    res.status = "Failed, mining not started";
    return true;
  }

  res.status = CORE_RPC_STATUS_OK;
  return true;
}

/*

bool RpcServer::remotenode_check_incoming_tx(const BinaryArray& tx_blob) {
	crypto::Hash tx_hash = NULL_HASH;
	crypto::Hash tx_prefixt_hash = NULL_HASH;
	Transaction tx;
	if (!parseAndValidateTransactionFromBinaryArray(tx_blob, tx, tx_hash, tx_prefixt_hash)) {
		logger(INFO) << "Could not parse tx from blob";
		return false;
	}
	cn::TransactionPrefix transaction = *static_cast<const TransactionPrefix*>(&tx);

	std::vector<uint32_t> out;
	uint64_t amount;

	if (!cn::findOutputsToAccount(transaction, m_fee_acc, m_view_key, out, amount)) {
		logger(INFO) << "Could not find outputs to remote node fee address";
		return false;
	}

	if (amount != 0) {
		logger(INFO) << "Node received relayed transaction fee: " << m_core.currency().formatAmount(amount) << " CCX";
		return true;
	}
	return false;
}

*/

bool RpcServer::on_stop_mining(const COMMAND_RPC_STOP_MINING::request& req, COMMAND_RPC_STOP_MINING::response& res) {
  if (!m_core.get_miner().stop()) {
    res.status = "Failed, mining not stopped";
    return true;
  }
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_stop_daemon(const COMMAND_RPC_STOP_DAEMON::request& req, COMMAND_RPC_STOP_DAEMON::response& res) {
  if (m_core.currency().isTestnet()) {
    m_p2p.sendStopSignal();
    res.status = CORE_RPC_STATUS_OK;
  } else {
    res.status = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
    return false;
  }
  return true;
}

//------------------------------------------------------------------------------------------------------------------------------
// JSON RPC methods
//------------------------------------------------------------------------------------------------------------------------------
bool RpcServer::f_on_blocks_list_json(const F_COMMAND_RPC_GET_BLOCKS_LIST::request& req, F_COMMAND_RPC_GET_BLOCKS_LIST::response& res) {
  if (m_core.get_current_blockchain_height() <= req.height) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT,
      std::string("To big height: ") + std::to_string(req.height) + ", current blockchain height = " + std::to_string(m_core.get_current_blockchain_height()) };
  }

  uint32_t print_blocks_count = 30;
  uint32_t last_height = req.height - print_blocks_count;
  if (req.height <= print_blocks_count)  {
    last_height = 0;
  }

  for (uint32_t i = req.height; i >= last_height; i--) {
    Hash block_hash = m_core.getBlockIdByHeight(static_cast<uint32_t>(i));
    Block blk;
    if (!m_core.getBlockByHash(block_hash, blk)) {
      throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
        "Internal error: can't get block by height. Height = " + std::to_string(i) + '.' };
    }

    size_t tx_cumulative_block_size;
    m_core.getBlockSize(block_hash, tx_cumulative_block_size);
    size_t blokBlobSize = getObjectBinarySize(blk);
    size_t minerTxBlobSize = getObjectBinarySize(blk.baseTransaction);

    f_block_short_response block_short;
    block_short.cumul_size = blokBlobSize + tx_cumulative_block_size - minerTxBlobSize;
    block_short.timestamp = blk.timestamp;
    block_short.height = i;
    m_core.getBlockDifficulty(static_cast<uint32_t>(block_short.height), block_short.difficulty);
    block_short.hash = common::podToHex(block_hash);
    block_short.tx_count = blk.transactionHashes.size() + 1;

    res.blocks.push_back(block_short);

    if (i == 0)
      break;
  }

  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::f_on_block_json(const F_COMMAND_RPC_GET_BLOCK_DETAILS::request &req, F_COMMAND_RPC_GET_BLOCK_DETAILS::response &res)
{
  Hash hash;
  f_block_details_response blockDetails;
  if (!parse_hash256(req.hash, hash))
  {
    throw JsonRpc::JsonRpcError{
        CORE_RPC_ERROR_CODE_WRONG_PARAM,
        "Failed to parse hex representation of block hash. Hex = " + req.hash + '.'};
  }

  if (!fill_f_block_details_response(hash, blockDetails))
  {
    throw JsonRpc::JsonRpcError{CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
                                std::string("Unable to fill block details.")};
  }

  res.block = blockDetails;

  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::f_on_transaction_json(const F_COMMAND_RPC_GET_TRANSACTION_DETAILS::request& req, F_COMMAND_RPC_GET_TRANSACTION_DETAILS::response& res) {
  Hash hash;

  if (!parse_hash256(req.hash, hash)) {
    throw JsonRpc::JsonRpcError{
      CORE_RPC_ERROR_CODE_WRONG_PARAM,
      "Failed to parse hex representation of transaction hash. Hex = " + req.hash + '.' };
  }

  std::vector<crypto::Hash> tx_ids;
  tx_ids.push_back(hash);

  std::list<crypto::Hash> missed_txs;
  std::list<Transaction> txs;
  m_core.getTransactions(tx_ids, txs, missed_txs);

  if (1 == txs.size()) {
    res.tx = txs.front();
  } else {
    throw JsonRpc::JsonRpcError{
      CORE_RPC_ERROR_CODE_WRONG_PARAM,
      "transaction wasn't found. Hash = " + req.hash + '.' };
  }

  crypto::Hash blockHash;
  uint32_t blockHeight;
  if (m_core.getBlockContainingTx(hash, blockHash, blockHeight)) {
    Block blk;
    if (m_core.getBlockByHash(blockHash, blk)) {
      size_t tx_cumulative_block_size;
      m_core.getBlockSize(blockHash, tx_cumulative_block_size);
      size_t blokBlobSize = getObjectBinarySize(blk);
      size_t minerTxBlobSize = getObjectBinarySize(blk.baseTransaction);
      difficulty_type blockDiff;
      m_core.getBlockDifficulty(blockHeight, blockDiff);
      f_block_short_response block_short;

      block_short.cumul_size = blokBlobSize + tx_cumulative_block_size - minerTxBlobSize;
      block_short.timestamp = blk.timestamp;
      block_short.height = blockHeight;
      block_short.hash = common::podToHex(blockHash);
      block_short.difficulty = blockDiff;
      block_short.tx_count = blk.transactionHashes.size() + 1;
      res.block = block_short;
    }
  }

  uint64_t amount_in = 0;
  get_inputs_money_amount(res.tx, amount_in);
  uint64_t amount_out = get_outs_money_amount(res.tx);

  res.txDetails.hash = common::podToHex(getObjectHash(res.tx));
  res.txDetails.fee = m_core.currency().getTransactionFee(res.tx, res.block.height);
  res.txDetails.amount_out = amount_out;
  res.txDetails.size = getObjectBinarySize(res.tx);

  uint64_t mixin;
  if (!f_getMixin(res.tx, mixin)) {
    return false;
  }
  res.txDetails.mixin = mixin;

  crypto::Hash paymentId;
  if (cn::getPaymentIdFromTxExtra(res.tx.extra, paymentId)) {
    res.txDetails.paymentId = common::podToHex(paymentId);
  } else {
    res.txDetails.paymentId = "";
  }

  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::f_getMixin(const Transaction& transaction, uint64_t& mixin) {
  mixin = 0;
  for (const TransactionInput& txin : transaction.inputs) {
    if (txin.type() != typeid(KeyInput)) {
      continue;
    }
    uint64_t currentMixin = boost::get<KeyInput>(txin).outputIndexes.size();
    if (currentMixin > mixin) {
      mixin = currentMixin;
    }
  }
  return true;
}

bool RpcServer::f_on_transactions_pool_json(const F_COMMAND_RPC_GET_POOL::request& req, F_COMMAND_RPC_GET_POOL::response& res) {
    auto pool = m_core.getPoolTransactions();
    auto height = m_core.get_current_blockchain_height();
    for (const Transaction tx : pool) {
        f_transaction_short_response transaction_short;
        uint64_t amount_out = getOutputAmount(tx);

        transaction_short.hash = common::podToHex(getObjectHash(tx));
        transaction_short.fee = m_core.currency().getTransactionFee(tx, height);
        transaction_short.amount_out = amount_out;
        transaction_short.size = getObjectBinarySize(tx);
        res.transactions.push_back(transaction_short);
    }

    res.status = CORE_RPC_STATUS_OK;
    return true;
}

bool RpcServer::on_getblockcount(const COMMAND_RPC_GETBLOCKCOUNT::request& req, COMMAND_RPC_GETBLOCKCOUNT::response& res) {
  res.count = m_core.get_current_blockchain_height();
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

namespace {
  uint64_t slow_memmem(void* start_buff, size_t buflen, void* pat, size_t patlen)
  {
    void* buf = start_buff;
    void* end = (char*)buf + buflen - patlen;
    while ((buf = memchr(buf, ((char*)pat)[0], buflen)))
    {
      if (buf>end)
        return 0;
      if (memcmp(buf, pat, patlen) == 0)
        return (char*)buf - (char*)start_buff;
      buf = (char*)buf + 1;
    }
    return 0;
  }
}

bool RpcServer::on_getblocktemplate(const COMMAND_RPC_GETBLOCKTEMPLATE::request& req, COMMAND_RPC_GETBLOCKTEMPLATE::response& res) {
  if (req.reserve_size > TX_EXTRA_NONCE_MAX_COUNT) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_TOO_BIG_RESERVE_SIZE, "To big reserved size, maximum 255" };
  }

  AccountPublicAddress acc = boost::value_initialized<AccountPublicAddress>();

  if (!req.wallet_address.size() || !m_core.currency().parseAccountAddressString(req.wallet_address, acc)) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_WRONG_WALLET_ADDRESS, "Failed to parse wallet address" };
  }

  Block b = boost::value_initialized<Block>();
  cn::BinaryArray blob_reserve;
  blob_reserve.resize(req.reserve_size, 0);
  if (!m_core.get_block_template(b, acc, res.difficulty, res.height, blob_reserve)) {
    logger(ERROR) << "Failed to create block template";
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Internal error: failed to create block template" };
  }

  BinaryArray block_blob = toBinaryArray(b);
  PublicKey tx_pub_key = cn::getTransactionPublicKeyFromExtra(b.baseTransaction.extra);
  if (tx_pub_key == NULL_PUBLIC_KEY) {
    logger(ERROR) << "Failed to find tx pub key in coinbase extra";
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Internal error: failed to find tx pub key in coinbase extra" };
  }

  if (0 < req.reserve_size) {
    res.reserved_offset = slow_memmem((void*)block_blob.data(), block_blob.size(), &tx_pub_key, sizeof(tx_pub_key));
    if (!res.reserved_offset) {
      logger(ERROR) << "Failed to find tx pub key in blockblob";
      throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Internal error: failed to create block template" };
    }
    res.reserved_offset += sizeof(tx_pub_key) + 3; //3 bytes: tag for TX_EXTRA_TAG_PUBKEY(1 byte), tag for TX_EXTRA_NONCE(1 byte), counter in TX_EXTRA_NONCE(1 byte)
    if (res.reserved_offset + req.reserve_size > block_blob.size()) {
      logger(ERROR) << "Failed to calculate offset for reserved bytes";
      throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Internal error: failed to create block template" };
    }
  } else {
    res.reserved_offset = 0;
  }

  res.blocktemplate_blob = toHex(block_blob);
  res.status = CORE_RPC_STATUS_OK;

  return true;
}

bool RpcServer::on_get_currency_id(const COMMAND_RPC_GET_CURRENCY_ID::request& /*req*/, COMMAND_RPC_GET_CURRENCY_ID::response& res) {
  Hash currencyId = m_core.currency().genesisBlockHash();
  res.currency_id_blob = common::podToHex(currencyId);
  return true;
}

bool RpcServer::on_submitblock(const COMMAND_RPC_SUBMITBLOCK::request& req, COMMAND_RPC_SUBMITBLOCK::response& res) {
  if (req.size() != 1) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_WRONG_PARAM, "Wrong param" };
  }

  BinaryArray blockblob;
  if (!fromHex(req[0], blockblob)) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_WRONG_BLOCKBLOB, "Wrong block blob" };
  }

  block_verification_context bvc = boost::value_initialized<block_verification_context>();

  m_core.handle_incoming_block_blob(blockblob, bvc, true, true);

  if (!bvc.m_added_to_main_chain) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_BLOCK_NOT_ACCEPTED, "Block not accepted" };
  }

  res.status = CORE_RPC_STATUS_OK;
  return true;
}


namespace {
  uint64_t get_block_reward(const Block& blk) {
    uint64_t reward = 0;
    for (const TransactionOutput& out : blk.baseTransaction.outputs) {
      reward += out.amount;
    }
    return reward;
  }
}

bool RpcServer::on_alt_blocks_list_json(const COMMAND_RPC_GET_ALT_BLOCKS_LIST::request &req, COMMAND_RPC_GET_ALT_BLOCKS_LIST::response &res)
{
  std::list<Block> alt_blocks;

  if (m_core.get_alternative_blocks(alt_blocks) && !alt_blocks.empty())
  {
    for (const auto &b : alt_blocks)
    {
      crypto::Hash block_hash = get_block_hash(b);
      uint32_t block_height = boost::get<BaseInput>(b.baseTransaction.inputs.front()).blockIndex;
      size_t tx_cumulative_block_size;
      m_core.getBlockSize(block_hash, tx_cumulative_block_size);
      size_t blokBlobSize = getObjectBinarySize(b);
      size_t minerTxBlobSize = getObjectBinarySize(b.baseTransaction);
      difficulty_type blockDiff;
      m_core.getBlockDifficulty(static_cast<uint32_t>(block_height), blockDiff);

      block_short_response block_short;
      block_short.timestamp = b.timestamp;
      block_short.height = block_height;
      block_short.hash = common::podToHex(block_hash);
      block_short.cumulative_size = blokBlobSize + tx_cumulative_block_size - minerTxBlobSize;
      block_short.transactions_count = b.transactionHashes.size() + 1;
      block_short.difficulty = blockDiff;

      res.alt_blocks.push_back(block_short);
    }
  }

  res.status = CORE_RPC_STATUS_OK;
  return true;
}

void RpcServer::fill_block_header_response(const Block& blk, bool orphan_status, uint64_t height, const Hash& hash, block_header_response& responce) {
  responce.major_version = blk.majorVersion;
  responce.minor_version = blk.minorVersion;
  responce.timestamp = blk.timestamp;
  responce.prev_hash = common::podToHex(blk.previousBlockHash);
  responce.nonce = blk.nonce;
  responce.orphan_status = orphan_status;
  responce.height = height;
  responce.deposits = m_core.depositAmountAtHeight(height);
  responce.depth = m_core.get_current_blockchain_height() - height - 1;
  responce.hash = common::podToHex(hash);
  m_core.getBlockDifficulty(static_cast<uint32_t>(height), responce.difficulty);
  responce.reward = get_block_reward(blk);
}

bool RpcServer::on_get_last_block_header(const COMMAND_RPC_GET_LAST_BLOCK_HEADER::request& req, COMMAND_RPC_GET_LAST_BLOCK_HEADER::response& res) {
  uint32_t last_block_height;
  Hash last_block_hash;

  m_core.get_blockchain_top(last_block_height, last_block_hash);

  Block last_block;
  if (!m_core.getBlockByHash(last_block_hash, last_block)) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Internal error: can't get last block hash." };
  }

  crypto::Hash tmp_hash = m_core.getBlockIdByHeight(last_block_height);
  bool is_orphaned = last_block_hash != tmp_hash;
  fill_block_header_response(last_block, is_orphaned, last_block_height, last_block_hash, res.block_header);
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_get_block_header_by_hash(const COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::request& req, COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::response& res) {
  Hash block_hash;

  if (!parse_hash256(req.hash, block_hash)) {
    throw JsonRpc::JsonRpcError{
      CORE_RPC_ERROR_CODE_WRONG_PARAM,
      "Failed to parse hex representation of block hash. Hex = " + req.hash + '.' };
  }

  Block blk;
  if (!m_core.getBlockByHash(block_hash, blk)) {
    throw JsonRpc::JsonRpcError{
      CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
      "Internal error: can't get block by hash. Hash = " + req.hash + '.' };
  }

  if (blk.baseTransaction.inputs.front().type() != typeid(BaseInput)) {
    throw JsonRpc::JsonRpcError{
      CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
      "Internal error: coinbase transaction in the block has the wrong type" };
  }

  uint32_t block_height = boost::get<BaseInput>(blk.baseTransaction.inputs.front()).blockIndex;
  crypto::Hash tmp_hash = m_core.getBlockIdByHeight(block_height);
  bool is_orphaned = block_hash != tmp_hash;

  fill_block_header_response(blk, is_orphaned, block_height, block_hash, res.block_header);
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_get_block_header_by_height(const COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::request& req, COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::response& res) {
  if (m_core.get_current_blockchain_height() <= req.height) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT,
      std::string("To big height: ") + std::to_string(req.height) + ", current blockchain height = " + std::to_string(m_core.get_current_blockchain_height()) };
  }

  Hash block_hash = m_core.getBlockIdByHeight(static_cast<uint32_t>(req.height));
  Block blk;
  if (!m_core.getBlockByHash(block_hash, blk)) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
      "Internal error: can't get block by height. Height = " + std::to_string(req.height) + '.' };
  }

  crypto::Hash tmp_hash = m_core.getBlockIdByHeight(req.height);
  bool is_orphaned = block_hash != tmp_hash;
  fill_block_header_response(blk, is_orphaned, req.height, block_hash, res.block_header);
  res.status = CORE_RPC_STATUS_OK;
  return true;
}


}

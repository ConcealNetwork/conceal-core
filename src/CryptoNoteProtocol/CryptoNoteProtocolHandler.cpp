// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 The TurtleCoin developers
// Copyright (c) 2016-2020 The Karbo developers
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "CryptoNoteProtocolHandler.h"

#include <future>
#include <thread>
#include <chrono>
#include <map>
#include <mutex>
#include <algorithm>
#include <climits>
#include <boost/scope_exit.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <System/Dispatcher.h>
#include <boost/optional.hpp>
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/VerificationContext.h"
#include "P2p/LevinProtocol.h"
#include <cstdint>  // for UINT64_MAX
#include "CryptoNoteCore/CheckpointList.h"

using namespace logging;
using namespace common;

namespace cn
{

namespace
{

template <class t_parametr>
bool post_notify(IP2pEndpoint &p2p, typename t_parametr::request &arg, const CryptoNoteConnectionContext &context)
{
  return p2p.invoke_notify_to_peer(t_parametr::ID, LevinProtocol::encode(arg), context);
}

template <class t_parametr>
void relay_post_notify(IP2pEndpoint &p2p, typename t_parametr::request &arg, const net_connection_id *excludeConnection = nullptr)
{
  p2p.relay_notify_to_all(t_parametr::ID, LevinProtocol::encode(arg), excludeConnection);
}

} // namespace

CryptoNoteProtocolHandler::CryptoNoteProtocolHandler(const Currency &currency, platform_system::Dispatcher &dispatcher, ICore &rcore, IP2pEndpoint *p_net_layout, logging::ILogger &log) :
  m_currency(currency),
  m_p2p(p_net_layout),
  m_core(rcore),
  m_synchronized(false),
  m_stop(false),
  m_last_checkpoint_req(0),
  m_observedHeight(0),
  m_peersCount(0),
  logger(log, "protocol"),
  m_dispatcher(dispatcher),
  m_maxObjectCount(cn::COMMAND_RPC_GET_OBJECTS_MAX_COUNT),
  m_current_validating_chunk_index(UINT32_MAX),
  m_last_chunk_validation_attempt(0)
  {
    if (!m_p2p)
      m_p2p = &m_p2p_stub;
  }

size_t CryptoNoteProtocolHandler::getPeerCount() const
{
  return m_peersCount;
}

void CryptoNoteProtocolHandler::set_p2p_endpoint(IP2pEndpoint *p2p)
{
  if (p2p)
    m_p2p = p2p;
  else
    m_p2p = &m_p2p_stub;
}

void CryptoNoteProtocolHandler::onConnectionOpened(CryptoNoteConnectionContext &context)
{
}

void CryptoNoteProtocolHandler::onConnectionClosed(CryptoNoteConnectionContext &context)
{
  bool updated = false;
  {
    std::lock_guard<std::mutex> lock(m_observedHeightMutex);
    uint64_t prevHeight = m_observedHeight;
    recalculateMaxObservedHeight(context);
    if (prevHeight != m_observedHeight)
    {
      updated = true;
    }
  }

  if (updated)
  {
    logger(TRACE) << "Observed height updated: " << m_observedHeight;
    m_observerManager.notify(&ICryptoNoteProtocolObserver::lastKnownBlockHeightUpdated, m_observedHeight);
  }

  if (context.m_state != CryptoNoteConnectionContext::state_befor_handshake)
  {
    m_peersCount--;
    m_observerManager.notify(&ICryptoNoteProtocolObserver::peerCountUpdated, m_peersCount.load());
  }
}

void CryptoNoteProtocolHandler::stop()
{
  m_stop = true;
}

bool CryptoNoteProtocolHandler::start_sync(CryptoNoteConnectionContext &context)
{
  logger(logging::TRACE) << context << "Starting synchronization";

  if (context.m_state == CryptoNoteConnectionContext::state_synchronizing)
  {
    assert(context.m_needed_objects.empty());
    assert(context.m_requested_objects.empty());

    NOTIFY_REQUEST_CHAIN::request r = boost::value_initialized<NOTIFY_REQUEST_CHAIN::request>();
    r.block_ids = m_core.buildSparseChain();
    logger(logging::TRACE) << context << "-->>NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << r.block_ids.size();
    post_notify<NOTIFY_REQUEST_CHAIN>(*m_p2p, r, context);
  }

  return true;
}

bool CryptoNoteProtocolHandler::get_stat_info(core_stat_info &stat_inf)
{
  return m_core.get_stat_info(stat_inf);
}

void CryptoNoteProtocolHandler::log_connections()
{
  std::stringstream ss;

  ss << std::setw(30) << std::left << "Remote Host"
     << std::setw(20) << std::left << "Peer id"
     << std::setw(25) << std::left << "State"
     << std::setw(20) << std::left << "Lifetime(seconds)" << std::endl;

  m_p2p->for_each_connection([&ss](const CryptoNoteConnectionContext &cntxt, PeerIdType peer_id)
                             { ss << std::setw(30) << std::left << std::string(cntxt.m_is_income ? "[INC]" : "[OUT]") + common::ipAddressToString(cntxt.m_remote_ip) + ":" + std::to_string(cntxt.m_remote_port)
                                  << std::setw(20) << std::left << std::hex << peer_id
                                  << std::setw(25) << std::left << get_protocol_state_string(cntxt.m_state)
                                  << std::setw(20) << std::left << std::to_string(time(nullptr) - cntxt.m_started) << std::endl; });
  logger(INFO) << "Connections: \n"
               << ss.str();
}

/* Get a list of daemons connected to this node */
std::vector<std::string> CryptoNoteProtocolHandler::all_connections()
{
  std::vector<std::string> connections;
  connections.clear();
  m_p2p->for_each_connection([&connections](const CryptoNoteConnectionContext &cntxt, PeerIdType peer_id)
                             {
                               (void)peer_id;
                               std::string ipAddress = common::ipAddressToString(cntxt.m_remote_ip);
                               connections.push_back(ipAddress);
                             });
  return connections;
}

uint32_t CryptoNoteProtocolHandler::get_current_blockchain_height()
{
  uint32_t height;
  crypto::Hash blockId;
  m_core.get_blockchain_top(height, blockId);
  return height;
}

bool CryptoNoteProtocolHandler::process_payload_sync_data(const CORE_SYNC_DATA &hshd, CryptoNoteConnectionContext &context, bool is_inital)
{
  if (context.m_state == CryptoNoteConnectionContext::state_befor_handshake && !is_inital)
    return true;

  if (context.m_state == CryptoNoteConnectionContext::state_synchronizing)
  {
  }
  else if (m_core.have_block(hshd.top_id))
  {
    if (is_inital)
    {
      on_connection_synchronized();
      context.m_state = CryptoNoteConnectionContext::state_pool_sync_required;
    }
    else
    {
      context.m_state = CryptoNoteConnectionContext::state_normal;
    }
  }
  else
  {
    int64_t diff = static_cast<int64_t>(hshd.current_height) - static_cast<int64_t>(get_current_blockchain_height());

    logger(diff >= 0 ? (is_inital ? logging::INFO : DEBUGGING) : logging::TRACE)  << context 
              << "Unknown top block: " << get_current_blockchain_height() << " -> " << hshd.current_height
              << std::endl
              << "Synchronization started";

    logger(DEBUGGING) << "Remote top block height: " << hshd.current_height << ", id: " << hshd.top_id;
    //let the socket to send response to handshake, but request callback, to let send request data after response
    logger(logging::TRACE) << context << "requesting synchronization";
    context.m_state = CryptoNoteConnectionContext::state_sync_required;
  }

  updateObservedHeight(hshd.current_height, context);
  context.m_remote_blockchain_height = hshd.current_height;

  if (is_inital)
  {
    m_peersCount++;
    m_observerManager.notify(&ICryptoNoteProtocolObserver::peerCountUpdated, m_peersCount.load());
  }

  if (context.version < cn::P2P_CHECKPOINT_LIST_VERSION || context.m_checkpoints_not_in_sync || context.m_active_checkpoint_req)
    return true;

  // Only request checkpoints if we can answer them (have confirmed chunks)
  // This prevents nodes without confirmed chunks from requesting from peers that also can't answer
  if (!m_core.getCheckpointList().can_answer_checkpoint_requests())
    return true;

  uint64_t time_now = time(nullptr);
  uint64_t last_request = time_now - m_last_checkpoint_req.load();
  if (last_request > P2P_CHECKPOINT_LIST_RE_REQUEST)
    return true;

  auto cl_status = m_core.getCheckpointList().get_incomplete_checkpoint_target();
  if ( cl_status.target_hash != NULL_HASH )
  {
    NOTIFY_REQUEST_CHECKPOINT_LIST::request req = boost::value_initialized<NOTIFY_REQUEST_CHECKPOINT_LIST::request>();
    req.target_hash = cl_status.target_hash;
    req.start_height = cl_status.start_height;
    req.end_height = cl_status.end_height;
  
    if (!post_notify<NOTIFY_REQUEST_CHECKPOINT_LIST>(*m_p2p, req, context))
      return false;

    m_last_checkpoint_req = time_now;
    context.m_active_checkpoint_req = true;
  }

  return true;
}

bool CryptoNoteProtocolHandler::get_payload_sync_data(CORE_SYNC_DATA &hshd)
{
  uint32_t current_height;
  m_core.get_blockchain_top(current_height, hshd.top_id);
  hshd.current_height = current_height;
  hshd.current_height += 1;
  return true;
}

template <typename Command, typename Handler>
int notifyAdaptor(const BinaryArray &reqBuf, CryptoNoteConnectionContext &ctx, Handler handler)
{

  typedef typename Command::request Request;
  int command = Command::ID;

  Request req = boost::value_initialized<Request>();
  if (!LevinProtocol::decode(reqBuf, req))
  {
    throw std::runtime_error("Failed to load_from_binary in command " + std::to_string(command));
  }

  return handler(command, req, ctx);
}

#define HANDLE_NOTIFY(CMD, Handler)                                                                                                   \
  case CMD::ID:                                                                                                                       \
  {                                                                                                                                   \
    ret = notifyAdaptor<CMD>(in, ctx, std::bind(Handler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)); \
    break;                                                                                                                            \
  }

int CryptoNoteProtocolHandler::handleCommand(bool is_notify, int command, const BinaryArray &in, BinaryArray &out, CryptoNoteConnectionContext &ctx, bool &handled)
{
  int ret = 0;
  handled = true;

  // Debug logging for chunk hash messages (BC_COMMANDS_POOL_BASE + 13 = REQUEST, + 14 = RESPONSE)
  const int CHUNK_HASH_REQUEST_ID = BC_COMMANDS_POOL_BASE + 13;
  const int CHUNK_HASH_RESPONSE_ID = BC_COMMANDS_POOL_BASE + 14;
  if (command == CHUNK_HASH_REQUEST_ID || command == CHUNK_HASH_RESPONSE_ID)
  {
    logger(INFO) << ctx << " handleCommand: received " << (is_notify ? "NOTIFY" : "REQUEST") 
                 << " command " << command << " (chunk hash message)";
  }

  switch (command)
  {
    HANDLE_NOTIFY(NOTIFY_NEW_BLOCK, &CryptoNoteProtocolHandler::handle_notify_new_block)
    HANDLE_NOTIFY(NOTIFY_NEW_TRANSACTIONS, &CryptoNoteProtocolHandler::handle_notify_new_transactions)
    HANDLE_NOTIFY(NOTIFY_REQUEST_GET_OBJECTS, &CryptoNoteProtocolHandler::handle_request_get_objects)
    HANDLE_NOTIFY(NOTIFY_RESPONSE_GET_OBJECTS, &CryptoNoteProtocolHandler::handle_response_get_objects)
    HANDLE_NOTIFY(NOTIFY_REQUEST_CHECKPOINT_LIST, &CryptoNoteProtocolHandler::handle_request_checkpoint_list)
    HANDLE_NOTIFY(NOTIFY_RESPONSE_CHECKPOINT_LIST, &CryptoNoteProtocolHandler::handle_response_checkpoint_list)
    HANDLE_NOTIFY(NOTIFY_REQUEST_CHUNK_HASH, &CryptoNoteProtocolHandler::handle_request_chunk_hash)
    HANDLE_NOTIFY(NOTIFY_RESPONSE_CHUNK_HASH, &CryptoNoteProtocolHandler::handle_response_chunk_hash)
    HANDLE_NOTIFY(NOTIFY_REQUEST_CHAIN, &CryptoNoteProtocolHandler::handle_request_chain)
    HANDLE_NOTIFY(NOTIFY_RESPONSE_CHAIN_ENTRY, &CryptoNoteProtocolHandler::handle_response_chain_entry)
    HANDLE_NOTIFY(NOTIFY_REQUEST_TX_POOL, &CryptoNoteProtocolHandler::handle_request_tx_pool)
    HANDLE_NOTIFY(NOTIFY_NEW_LITE_BLOCK, &CryptoNoteProtocolHandler::handle_notify_new_lite_block)
    HANDLE_NOTIFY(NOTIFY_MISSING_TXS, &CryptoNoteProtocolHandler::handle_notify_missing_txs)

  default:
    handled = false;
    // Debug logging for unhandled commands (especially chunk hash related)
    const int CHUNK_HASH_REQUEST_ID = BC_COMMANDS_POOL_BASE + 13;
    const int CHUNK_HASH_RESPONSE_ID = BC_COMMANDS_POOL_BASE + 14;
    if (command == CHUNK_HASH_REQUEST_ID || command == CHUNK_HASH_RESPONSE_ID)
    {
      logger(ERROR) << ctx << " handleCommand: chunk hash command " << command 
                     << " not handled! This should not happen.";
    }
  }

  return ret;
}

#undef HANDLE_NOTIFY

int CryptoNoteProtocolHandler::handle_notify_new_block(int command, NOTIFY_NEW_BLOCK::request &arg, CryptoNoteConnectionContext &context)
{
  logger(logging::TRACE) << context << "NOTIFY_NEW_BLOCK (hop " << arg.hop << ")";

  updateObservedHeight(arg.current_blockchain_height, context);

  context.m_remote_blockchain_height = arg.current_blockchain_height;

  if (context.m_state != CryptoNoteConnectionContext::state_normal)
  {
    return 1;
  }

  for (auto tx_blob_it = arg.b.txs.begin(); tx_blob_it != arg.b.txs.end(); tx_blob_it++)
  {
    cn::tx_verification_context tvc = boost::value_initialized<decltype(tvc)>();

    auto transactionBinary = asBinaryArray(*tx_blob_it);
    //crypto::Hash transactionHash = crypto::cn_fast_hash(transactionBinary.data(), transactionBinary.size());
    //logger(DEBUGGING) << "transaction " << transactionHash << " came in NOTIFY_NEW_BLOCK";

    m_core.handle_incoming_tx(transactionBinary, tvc, true);
    if (tvc.m_verification_failed)
    {
      logger(logging::INFO) << context << "Block verification failed: transaction verification failed, dropping connection";
      m_p2p->drop_connection(context, true);
      return 1;
    }
  }

  block_verification_context bvc = boost::value_initialized<block_verification_context>();
  m_core.handle_incoming_block_blob(asBinaryArray(arg.b.block), bvc, true, false);
  if (bvc.m_verification_failed)
  {
    logger(logging::DEBUGGING) << context << "Block verification failed, dropping connection";
    m_p2p->drop_connection(context, true);
    return 1;
  }
  if (bvc.m_added_to_main_chain)
  {
    ++arg.hop;
    //TODO: Add here announce protocol usage
    //relay_post_notify<NOTIFY_NEW_BLOCK>(*m_p2p, arg, &context.m_connection_id);
    relay_block(arg);
    // relay_block(arg, context);

    if (bvc.m_switched_to_alt_chain)
    {
      requestMissingPoolTransactions(context);
    }
  }
  else if (bvc.m_marked_as_orphaned)
  {
    context.m_state = CryptoNoteConnectionContext::state_synchronizing;
    NOTIFY_REQUEST_CHAIN::request r = boost::value_initialized<NOTIFY_REQUEST_CHAIN::request>();
    r.block_ids = m_core.buildSparseChain();
    logger(logging::TRACE) << context << "-->>NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << r.block_ids.size();
    post_notify<NOTIFY_REQUEST_CHAIN>(*m_p2p, r, context);
  }

  return 1;
}

int CryptoNoteProtocolHandler::handle_notify_new_transactions(int command, NOTIFY_NEW_TRANSACTIONS::request &arg, CryptoNoteConnectionContext &context)
{
  logger(logging::TRACE) << context << "NOTIFY_NEW_TRANSACTIONS";

  if (context.m_state != CryptoNoteConnectionContext::state_normal)
    return 1;

  if (context.m_pending_lite_block)
  {
    logger(logging::TRACE) << context
                           << " Pending lite block detected, handling request as missing lite block transactions response";
    std::vector<BinaryArray> _txs;
    for (const auto& tx : arg.txs)
    {
      _txs.push_back(asBinaryArray(tx));
    }
    return doPushLiteBlock(context.m_pending_lite_block->request, context, std::move(_txs));
  }
  else
  {
    for (auto tx_blob_it = arg.txs.begin(); tx_blob_it != arg.txs.end();)
    {
      auto transactionBinary = asBinaryArray(*tx_blob_it);
      crypto::Hash transactionHash = crypto::cn_fast_hash(transactionBinary.data(), transactionBinary.size());
      logger(DEBUGGING) << "transaction " << transactionHash << " came in NOTIFY_NEW_TRANSACTIONS";

      cn::tx_verification_context tvc = boost::value_initialized<decltype(tvc)>();
      m_core.handle_incoming_tx(transactionBinary, tvc, false);
      if (tvc.m_verification_failed)
      {
        logger(logging::DEBUGGING) << context << "Tx verification failed";
      }
      if (!tvc.m_verification_failed && tvc.m_should_be_relayed)
      {
        ++tx_blob_it;
      }
      else
      {
        tx_blob_it = arg.txs.erase(tx_blob_it);
      }
    }

    if (arg.txs.size())
    {
      //TODO: add announce usage here
      relay_post_notify<NOTIFY_NEW_TRANSACTIONS>(*m_p2p, arg, &context.m_connection_id);
    }
  }

  return true;
}

int CryptoNoteProtocolHandler::handle_request_get_objects(int command, NOTIFY_REQUEST_GET_OBJECTS::request &arg, CryptoNoteConnectionContext &context)
{
  logger(logging::TRACE) << context << "NOTIFY_REQUEST_GET_OBJECTS";
  size_t maxObjects = m_maxObjectCount.load();
  const size_t totalObjects = arg.blocks.size() + arg.txs.size();

  logger(INFO) << "DEBUG: Request for " << totalObjects << " objects (limit: " << maxObjects << ")";


  if (totalObjects > maxObjects)
  {
    logger(logging::ERROR) << context << "Requested objects count exceeds limit of " 
                          << maxObjects << ": blocks " << arg.blocks.size() 
                          << " + txs " << arg.txs.size() << " = " << totalObjects;
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  NOTIFY_RESPONSE_GET_OBJECTS::request rsp;
  if (!m_core.handle_get_objects(arg, rsp))
  {
    logger(logging::ERROR) << context << "failed to handle request NOTIFY_REQUEST_GET_OBJECTS, dropping connection";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }
  logger(logging::TRACE) << context << "-->>NOTIFY_RESPONSE_GET_OBJECTS: blocks.size()=" << rsp.blocks.size() << ", txs.size()=" << rsp.txs.size()
                         << ", rsp.m_current_blockchain_height=" << rsp.current_blockchain_height << ", missed_ids.size()=" << rsp.missed_ids.size();
  post_notify<NOTIFY_RESPONSE_GET_OBJECTS>(*m_p2p, rsp, context);
  return 1;
}

int CryptoNoteProtocolHandler::handle_response_get_objects(int command, NOTIFY_RESPONSE_GET_OBJECTS::request& arg, CryptoNoteConnectionContext& context) {
  logger(logging::TRACE) << context << "NOTIFY_RESPONSE_GET_OBJECTS";

  if (context.m_last_response_height > arg.current_blockchain_height) {
    logger(logging::ERROR) << context << "sent wrong NOTIFY_HAVE_OBJECTS: arg.m_current_blockchain_height=" << arg.current_blockchain_height
      << " < m_last_response_height=" << context.m_last_response_height << ", dropping connection";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  updateObservedHeight(arg.current_blockchain_height, context);

  context.m_remote_blockchain_height = arg.current_blockchain_height;

  size_t count = 0;
  std::vector<crypto::Hash> block_hashes;
  block_hashes.reserve(arg.blocks.size());
  std::vector<parsed_block_entry> parsed_blocks;
  parsed_blocks.reserve(arg.blocks.size());
  for (const block_complete_entry& block_entry : arg.blocks) {
    ++count;
    Block b;
    BinaryArray block_blob = asBinaryArray(block_entry.block);
    if (block_blob.size() > m_currency.maxBlockBlobSize()) {
      logger(logging::ERROR) << context << "sent wrong block: too big size " << block_blob.size() << ", dropping connection";
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    }
    if (!fromBinaryArray(b, block_blob)) {
      logger(logging::ERROR) << context << "sent wrong block: failed to parse and validate block: \r\n"
        << toHex(block_blob) << "\r\n dropping connection";
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    }

    //to avoid concurrency in core between connections, suspend connections which delivered block later then first one
    auto blockHash = get_block_hash(b);
    if (count == 2) {
      if (m_core.have_block(blockHash)) {
        context.m_state = CryptoNoteConnectionContext::state_idle;
        context.m_needed_objects.clear();
        context.m_requested_objects.clear();
        logger(DEBUGGING) << context << "Connection set to idle state.";
        return 1;
      }
    }

    auto req_it = context.m_requested_objects.find(blockHash);
    if (req_it == context.m_requested_objects.end()) {
      logger(logging::ERROR) << context << "sent wrong NOTIFY_RESPONSE_GET_OBJECTS: block with id=" << common::podToHex(blockHash)
        << " wasn't requested, dropping connection";
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    }
    if (b.transactionHashes.size() != block_entry.txs.size()) {
      logger(logging::ERROR) << context << "sent wrong NOTIFY_RESPONSE_GET_OBJECTS: block with id=" << common::podToHex(blockHash)
        << ", transactionHashes.size()=" << b.transactionHashes.size() << " mismatch with block_complete_entry.m_txs.size()=" << block_entry.txs.size() << ", dropping connection";
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    }

    context.m_requested_objects.erase(req_it);

    block_hashes.push_back(blockHash);

    parsed_block_entry parsedBlock;
    parsedBlock.block = std::move(b);
    for (auto& tx_blob : block_entry.txs) {
      auto transactionBinary = asBinaryArray(tx_blob);
      parsedBlock.txs.push_back(transactionBinary);
    }
    parsed_blocks.push_back(parsedBlock);
  }

  if (context.m_requested_objects.size()) {
    logger(logging::ERROR, logging::BRIGHT_RED) << context <<
      "returned not all requested objects (context.m_requested_objects.size()="
      << context.m_requested_objects.size() << "), dropping connection";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  uint32_t height;
  crypto::Hash top;
  {
    m_core.pause_mining();

    // we lock all the rest to avoid having multiple connections redo a lot
    // of the same work, and one of them doing it for nothing: subsequent
    // connections will wait until the current one's added its blocks, then
    // will add any extra it has, if any
    std::lock_guard<std::recursive_mutex> lk(m_sync_lock);

    // dismiss what another connection might already have done (likely everything)
    m_core.get_blockchain_top(height, top);
    uint64_t dismiss = 1;
    for (const auto &h : block_hashes) {
      if (top == h) {
        logger(DEBUGGING) << "Found current top block in synced blocks, dismissing "
          << dismiss << "/" << arg.blocks.size() << " blocks";
        while (dismiss--) {
          arg.blocks.erase(arg.blocks.begin());
          parsed_blocks.erase(parsed_blocks.begin());
        }
        break;
      }
      ++dismiss;
    }

    BOOST_SCOPE_EXIT_ALL(this) { m_core.update_block_template_and_resume_mining(); };

    int result = processObjects(context, parsed_blocks);
    if (result != 0) {
      return result;
    }
  }

  m_core.get_blockchain_top(height, top);
  logger(DEBUGGING, BRIGHT_GREEN) << "Local blockchain updated, new height = " << height;

  if (!m_stop && context.m_state == CryptoNoteConnectionContext::state_synchronizing) {
    request_missing_objects(context, true);
  }

  return 1;
}

int CryptoNoteProtocolHandler::processObjects(CryptoNoteConnectionContext& context, const std::vector<parsed_block_entry>& blocks) {

  for (const parsed_block_entry& block_entry : blocks) {
    if (m_stop) {
      break;
    }

    //process transactions
    for (size_t i = 0; i < block_entry.txs.size(); ++i) {
      auto transactionBinary = block_entry.txs[i];
      crypto::Hash transactionHash = crypto::cn_fast_hash(transactionBinary.data(), transactionBinary.size());
      logger(DEBUGGING) << "transaction " << transactionHash << " came in processObjects";

      // check if tx hashes match
      if (transactionHash != block_entry.block.transactionHashes[i]) {
        logger(DEBUGGING) << context << "transaction mismatch on NOTIFY_RESPONSE_GET_OBJECTS, \r\ntx_id = "
          << common::podToHex(transactionHash) << ", dropping connection";
        context.m_state = CryptoNoteConnectionContext::state_shutdown;
        return 1;
      }

      tx_verification_context tvc = boost::value_initialized<decltype(tvc)>();
      m_core.handle_incoming_tx(transactionBinary, tvc, true);
      if (tvc.m_verification_failed) {
        logger(DEBUGGING) << context << "transaction verification failed on NOTIFY_RESPONSE_GET_OBJECTS, \r\ntx_id = "
          << common::podToHex(transactionHash) << ", dropping connection";
        context.m_state = CryptoNoteConnectionContext::state_shutdown;
        return 1;
      }
    }

    // process block
    block_verification_context bvc = boost::value_initialized<block_verification_context>();
    m_core.handle_incoming_block(block_entry.block, bvc, false, false);

    if (bvc.m_verification_failed) {
      logger(DEBUGGING) << context << "Block verification failed, dropping connection";
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    } else if (bvc.m_marked_as_orphaned) {
      logger(logging::INFO) << context << "Block received at sync phase was marked as orphaned, dropping connection";
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    } else if (bvc.m_already_exists) {
      logger(DEBUGGING) << context << "Block already exists, switching to idle state";
      context.m_state = CryptoNoteConnectionContext::state_idle;
      context.m_needed_objects.clear();
      context.m_requested_objects.clear();
      return 1;
    }

    m_dispatcher.yield();
  }

  return 0;

}

bool CryptoNoteProtocolHandler::on_idle()
{
  // Periodically validate unverified chunks in chronological order
  // Only for version 2+ nodes (chunked checkpoint system)
  if (cn::P2P_CURRENT_VERSION >= cn::P2P_CHECKPOINT_LIST_VERSION)
  {
    validate_unverified_chunks();
  }
  
  return m_core.on_idle();
}

int CryptoNoteProtocolHandler::handle_request_chain(int command, NOTIFY_REQUEST_CHAIN::request &arg, CryptoNoteConnectionContext &context)
{
  logger(logging::TRACE) << context << "NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << arg.block_ids.size();

  if (arg.block_ids.empty())
  {
    logger(logging::ERROR, logging::BRIGHT_RED) << context << "Failed to handle NOTIFY_REQUEST_CHAIN. block_ids is empty";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  if (arg.block_ids.back() != m_core.getBlockIdByHeight(0))
  {
    logger(logging::ERROR) << context << "Failed to handle NOTIFY_REQUEST_CHAIN. block_ids doesn't end with genesis block ID";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  NOTIFY_RESPONSE_CHAIN_ENTRY::request r;
  r.m_block_ids = m_core.findBlockchainSupplement(arg.block_ids, BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT, r.total_height, r.start_height);

  logger(logging::TRACE) << context << "-->>NOTIFY_RESPONSE_CHAIN_ENTRY: m_start_height=" << r.start_height << ", m_total_height=" << r.total_height << ", m_block_ids.size()=" << r.m_block_ids.size();
  post_notify<NOTIFY_RESPONSE_CHAIN_ENTRY>(*m_p2p, r, context);
  return 1;
}

bool CryptoNoteProtocolHandler::request_missing_objects(CryptoNoteConnectionContext &context, bool check_having_blocks)
{
  if (context.m_needed_objects.size())
  {
    //we know objects that we need, request this objects
    NOTIFY_REQUEST_GET_OBJECTS::request req;
    size_t count = 0;
    auto it = context.m_needed_objects.begin();

    while (it != context.m_needed_objects.end() && count < BLOCKS_SYNCHRONIZING_DEFAULT_COUNT)
    {
      if (!(check_having_blocks && m_core.have_block(*it)))
      {
        req.blocks.push_back(*it);
        ++count;
        context.m_requested_objects.insert(*it);
      }
      it = context.m_needed_objects.erase(it);
    }
    logger(logging::TRACE) << context << "-->>NOTIFY_REQUEST_GET_OBJECTS: blocks.size()=" << req.blocks.size() << ", txs.size()=" << req.txs.size();
    post_notify<NOTIFY_REQUEST_GET_OBJECTS>(*m_p2p, req, context);
  }
  else if (context.m_last_response_height < context.m_remote_blockchain_height - 1)
  { //we have to fetch more objects ids, request blockchain entry

    NOTIFY_REQUEST_CHAIN::request r = boost::value_initialized<NOTIFY_REQUEST_CHAIN::request>();
    r.block_ids = m_core.buildSparseChain();
    logger(logging::TRACE) << context << "-->>NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << r.block_ids.size();
    post_notify<NOTIFY_REQUEST_CHAIN>(*m_p2p, r, context);
  }
  else
  {
    if (!(context.m_last_response_height ==
              context.m_remote_blockchain_height - 1 &&
          !context.m_needed_objects.size() &&
          !context.m_requested_objects.size()))
    {
      logger(logging::ERROR, logging::BRIGHT_RED)
          << "request_missing_blocks final condition failed!"
          << "\r\nm_last_response_height=" << context.m_last_response_height
          << "\r\nm_remote_blockchain_height=" << context.m_remote_blockchain_height
          << "\r\nm_needed_objects.size()=" << context.m_needed_objects.size()
          << "\r\nm_requested_objects.size()=" << context.m_requested_objects.size()
          << "\r\non connection [" << context << "]";
      return false;
    }

    requestMissingPoolTransactions(context);

    context.m_state = CryptoNoteConnectionContext::state_normal;
    logger(logging::INFO, logging::BRIGHT_GREEN)  << context << "Synchronization complete";
    on_connection_synchronized();
  }
  return true;
}

bool CryptoNoteProtocolHandler::on_connection_synchronized()
{
  bool val_expected = false;
  if (m_synchronized.compare_exchange_strong(val_expected, true))
  {
    logger(logging::INFO) << ENDL << "********************************************************************************" << ENDL
                          << "You are now synchronized with the Conceal network." << ENDL
                          << "Please note, that the blockchain will be saved only after you quit the daemon" << ENDL
                          << "with the \"exit\" command or if you use the \"save\" command." << ENDL
                          << "Otherwise, you will possibly need to synchronize the blockchain again." << ENDL
                          << "Use \"help\" command to see the list of available commands." << ENDL
                          << "********************************************************************************";
    m_core.on_synchronized();

    uint32_t height;
    crypto::Hash hash;
    m_core.get_blockchain_top(height, hash);
    m_observerManager.notify(&ICryptoNoteProtocolObserver::blockchainSynchronized, height);
  }
  return true;
}

int CryptoNoteProtocolHandler::handle_response_chain_entry(int command, NOTIFY_RESPONSE_CHAIN_ENTRY::request &arg, CryptoNoteConnectionContext &context)
{
  logger(logging::TRACE) << context << "NOTIFY_RESPONSE_CHAIN_ENTRY: m_block_ids.size()=" << arg.m_block_ids.size()
                         << ", m_start_height=" << arg.start_height << ", m_total_height=" << arg.total_height;

  if (!arg.m_block_ids.size())
  {
    logger(logging::ERROR) << context << "sent empty m_block_ids, dropping connection";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  if (!m_core.have_block(arg.m_block_ids.front()))
  {
    logger(logging::ERROR)
        << context << "sent m_block_ids starting from unknown id: "
        << common::podToHex(arg.m_block_ids.front())
        << " , dropping connection";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  context.m_remote_blockchain_height = arg.total_height;
  context.m_last_response_height = arg.start_height + static_cast<uint32_t>(arg.m_block_ids.size()) - 1;

  if (context.m_last_response_height > context.m_remote_blockchain_height)
  {
    logger(logging::ERROR)
        << context
        << "sent wrong NOTIFY_RESPONSE_CHAIN_ENTRY, with \r\nm_total_height="
        << arg.total_height << "\r\nm_start_height=" << arg.start_height
        << "\r\nm_block_ids.size()=" << arg.m_block_ids.size();
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
  }

  for (auto &bl_id : arg.m_block_ids)
  {
    if (!m_core.have_block(bl_id))
      context.m_needed_objects.push_back(bl_id);
  }

  request_missing_objects(context, false);
  return 1;
}

int CryptoNoteProtocolHandler::handle_notify_new_lite_block(int command, NOTIFY_NEW_LITE_BLOCK::request &arg,
                                                            CryptoNoteConnectionContext &context)
{
  logger(logging::TRACE) << context << "NOTIFY_NEW_LITE_BLOCK (hop " << arg.hop << ")";
  updateObservedHeight(arg.current_blockchain_height, context);
  context.m_remote_blockchain_height = arg.current_blockchain_height;
  if (context.m_state != CryptoNoteConnectionContext::state_normal)
  {
    return 1;
  }

  return doPushLiteBlock(std::move(arg), context, {});
}

int CryptoNoteProtocolHandler::handle_notify_missing_txs(int command, NOTIFY_MISSING_TXS::request &arg,
                                                         CryptoNoteConnectionContext &context)
{
  logger(logging::TRACE) << context << "NOTIFY_MISSING_TXS";

  NOTIFY_NEW_TRANSACTIONS::request req;

  std::list<Transaction> txs;
  std::list<crypto::Hash> missedHashes;
  m_core.getTransactions(arg.missing_txs, txs, missedHashes, true);
  if (!missedHashes.empty())
  {
    logger(logging::DEBUGGING) << "Failed to Handle NOTIFY_MISSING_TXS, Unable to retrieve requested "
                                  "transactions, Dropping Connection";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }
  else
  {
    for (auto &tx : txs)
    {
      req.txs.push_back(asString(toBinaryArray(tx)));
    }
  }

  logger(logging::DEBUGGING) << "--> NOTIFY_RESPONSE_MISSING_TXS: "
                             << "txs.size() = " << req.txs.size();

  if (post_notify<NOTIFY_NEW_TRANSACTIONS>(*m_p2p, req, context))
  {
    logger(logging::DEBUGGING) << "NOTIFY_MISSING_TXS response sent to peer successfully";
  }
  else
  {
    logger(logging::DEBUGGING) << "Error while sending NOTIFY_MISSING_TXS response to peer";
  }

  return 1;
}

int CryptoNoteProtocolHandler::handle_request_tx_pool(int command, NOTIFY_REQUEST_TX_POOL::request &arg,
                                                      CryptoNoteConnectionContext &context)
{
  logger(logging::TRACE) << context << "NOTIFY_REQUEST_TX_POOL: txs.size() = " << arg.txs.size();

  std::vector<Transaction> addedTransactions;
  std::vector<crypto::Hash> deletedTransactions;
  m_core.getPoolChanges(arg.txs, addedTransactions, deletedTransactions);

  if (!addedTransactions.empty())
  {
    NOTIFY_NEW_TRANSACTIONS::request notification;
    for (auto &tx : addedTransactions)
    {
      notification.txs.push_back(asString(toBinaryArray(tx)));
    }

    bool ok = post_notify<NOTIFY_NEW_TRANSACTIONS>(*m_p2p, notification, context);
    if (!ok)
    {
      logger(logging::WARNING, logging::BRIGHT_YELLOW) << "Failed to post notification NOTIFY_NEW_TRANSACTIONS to " << context.m_connection_id;
    }
  }

  return 1;
}

void CryptoNoteProtocolHandler::relay_block(NOTIFY_NEW_BLOCK::request &arg)
{
  // generate a lite block request from the received normal block
  NOTIFY_NEW_LITE_BLOCK::request lite_arg;
  lite_arg.current_blockchain_height = arg.current_blockchain_height;
  lite_arg.block = arg.b.block;
  lite_arg.hop = arg.hop;

  // encoding the request for sending the blocks to peers
  auto buf = LevinProtocol::encode(arg);
  auto lite_buf = LevinProtocol::encode(lite_arg);

  // logging the msg size to see the difference in payload size
  logger(logging::DEBUGGING) << "NOTIFY_NEW_BLOCK - MSG_SIZE = " << buf.size();
  logger(logging::DEBUGGING) << "NOTIFY_NEW_LITE_BLOCK - MSG_SIZE = " << lite_buf.size();

  std::list<boost::uuids::uuid> liteBlockConnections, normalBlockConnections;

  // sort the peers into their support categories
  m_p2p->for_each_connection([this, &liteBlockConnections, &normalBlockConnections](
                                 const CryptoNoteConnectionContext &ctx, uint64_t peerId) {
    if (ctx.version >= P2P_LITE_BLOCKS_PROPOGATION_VERSION)
    {
      logger(logging::DEBUGGING) << ctx << "Peer supports lite-blocks... adding peer to lite block list";
      liteBlockConnections.push_back(ctx.m_connection_id);
    }
    else
    {
      logger(logging::DEBUGGING) << ctx << "Peer doesn't support lite-blocks... adding peer to normal block list";
      normalBlockConnections.push_back(ctx.m_connection_id);
    }
  });

  // first send lite blocks as it's faster
  if (!liteBlockConnections.empty())
  {
    m_p2p->externalRelayNotifyToList(NOTIFY_NEW_LITE_BLOCK::ID, lite_buf, liteBlockConnections);
  }

  if (!normalBlockConnections.empty())
  {
    auto buf = LevinProtocol::encode(arg);
    m_p2p->externalRelayNotifyToAll(NOTIFY_NEW_BLOCK::ID, buf, nullptr);
  }
}

void CryptoNoteProtocolHandler::relay_transactions(NOTIFY_NEW_TRANSACTIONS::request &arg)
{
  auto buf = LevinProtocol::encode(arg);
  m_p2p->externalRelayNotifyToAll(NOTIFY_NEW_TRANSACTIONS::ID, buf, nullptr);
}

void CryptoNoteProtocolHandler::requestMissingPoolTransactions(const CryptoNoteConnectionContext &context)
{
  if (context.version < cn::P2P_VERSION_1)
  {
    return;
  }

  auto poolTxs = m_core.getPoolTransactions();

  NOTIFY_REQUEST_TX_POOL::request notification;
  for (auto &tx : poolTxs)
  {
    notification.txs.emplace_back(getObjectHash(tx));
  }

  bool ok = post_notify<NOTIFY_REQUEST_TX_POOL>(*m_p2p, notification, context);
  if (!ok)
  {
    logger(logging::WARNING, logging::BRIGHT_YELLOW) << "Failed to post notification NOTIFY_REQUEST_TX_POOL to " << context.m_connection_id;
  }
}

void CryptoNoteProtocolHandler::updateObservedHeight(uint32_t peerHeight, const CryptoNoteConnectionContext &context)
{
  bool updated = false;
  {
    std::lock_guard<std::mutex> lock(m_observedHeightMutex);

    uint32_t height = m_observedHeight;
    if (peerHeight > context.m_remote_blockchain_height)
    {
      m_observedHeight = std::max(m_observedHeight, peerHeight);
      if (m_observedHeight != height)
      {
        updated = true;
      }
    }
    else if (peerHeight != context.m_remote_blockchain_height && context.m_remote_blockchain_height == m_observedHeight)
    {
      //the client switched to alternative chain and had maximum observed height. need to recalculate max height
      recalculateMaxObservedHeight(context);
      if (m_observedHeight != height)
      {
        updated = true;
      }
    }
  }

  if (updated)
  {
    logger(TRACE) << "Observed height updated: " << m_observedHeight;
    m_observerManager.notify(&ICryptoNoteProtocolObserver::lastKnownBlockHeightUpdated, m_observedHeight);
  }
}

void CryptoNoteProtocolHandler::recalculateMaxObservedHeight(const CryptoNoteConnectionContext &context)
{
  //should be locked outside
  uint32_t peerHeight = 0;
  m_p2p->for_each_connection([&peerHeight, &context](const CryptoNoteConnectionContext &ctx, PeerIdType peerId) {
    if (ctx.m_connection_id != context.m_connection_id)
    {
      peerHeight = std::max(peerHeight, ctx.m_remote_blockchain_height);
    }
  });

  uint32_t localHeight = 0;
  crypto::Hash ignore;
  m_core.get_blockchain_top(localHeight, ignore);
  m_observedHeight = std::max(peerHeight, localHeight + 1);
  if (context.m_state == CryptoNoteConnectionContext::state_normal)
  {
    m_observedHeight = localHeight;
  }
}

uint32_t CryptoNoteProtocolHandler::getObservedHeight() const
{
  std::lock_guard<std::mutex> lock(m_observedHeightMutex);
  return m_observedHeight;
};

bool CryptoNoteProtocolHandler::addObserver(ICryptoNoteProtocolObserver *observer)
{
  return m_observerManager.add(observer);
}

bool CryptoNoteProtocolHandler::removeObserver(ICryptoNoteProtocolObserver *observer)
{
  return m_observerManager.remove(observer);
}

int CryptoNoteProtocolHandler::doPushLiteBlock(NOTIFY_NEW_LITE_BLOCK::request arg, CryptoNoteConnectionContext &context,
                                               std::vector<BinaryArray> missingTxs)
{
  Block b;
  if (!fromBinaryArray(b, asBinaryArray(arg.block)))
  {
    logger(logging::WARNING) << context << "Deserialization of Block Template failed, dropping connection";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  std::unordered_map<crypto::Hash, BinaryArray> provided_txs;
  provided_txs.reserve(missingTxs.size());
  for (const auto &missingTx : missingTxs)
  {
    provided_txs[getBinaryArrayHash(missingTx)] = missingTx;
  }

  std::vector<BinaryArray> have_txs;
  std::vector<crypto::Hash> need_txs;

  if (context.m_pending_lite_block)
  {
    for (const auto &requestedTxHash : context.m_pending_lite_block->missed_transactions)
    {
      if (provided_txs.find(requestedTxHash) == provided_txs.end())
      {
        logger(logging::DEBUGGING) << context
                                   << "Peer didn't provide a missing transaction, previously "
                                      "acquired for a lite block, dropping connection.";
        context.m_pending_lite_block.reset();
        context.m_state = CryptoNoteConnectionContext::state_shutdown;
        return 1;
      }
    }
  }

  /*
   * here we are finding out which txs are present in the pool and which are not
   * further we check for transactions in the blockchain to accept alternative blocks
   */
  for (const auto &transactionHash : b.transactionHashes)
  {
    auto providedSearch = provided_txs.find(transactionHash);
    if (providedSearch != provided_txs.end())
    {
      have_txs.push_back(providedSearch->second);
    }
    else
    {
      Transaction tx;
      if (m_core.getTransaction(transactionHash, tx, true))
      {
        have_txs.push_back(toBinaryArray(tx));
      }
      else
      {
        need_txs.push_back(transactionHash);
      }
    }
  }

  /*
   * if all txs are present then continue adding the block to
   * blockchain storage and relaying the lite-block to other peers
   *
   * if not request the missing txs from the sender
   * of the lite-block request
   */
  if (need_txs.empty())
  {
    context.m_pending_lite_block = boost::none;

    for (auto transactionBinary : have_txs)
    {
      cn::tx_verification_context tvc = boost::value_initialized<decltype(tvc)>();

      m_core.handle_incoming_tx(transactionBinary, tvc, true);
      if (tvc.m_verification_failed)
      {
        logger(logging::INFO) << context << "Lite block verification failed: transaction verification failed, dropping connection";
        m_p2p->drop_connection(context, true);
        return 1;
      }
    }

    block_verification_context bvc = boost::value_initialized<block_verification_context>();
    m_core.handle_incoming_block_blob(asBinaryArray(arg.block), bvc, true, false);
    if (bvc.m_verification_failed)
    {
      logger(logging::DEBUGGING) << context << "Lite block verification failed, dropping connection";
      m_p2p->drop_connection(context, true);
      return 1;
    }
    if (bvc.m_added_to_main_chain)
    {
      ++arg.hop;
      //TODO: Add here announce protocol usage
      relay_post_notify<NOTIFY_NEW_LITE_BLOCK>(*m_p2p, arg, &context.m_connection_id);

      if (bvc.m_switched_to_alt_chain)
      {
        requestMissingPoolTransactions(context);
      }
    }
    else if (bvc.m_marked_as_orphaned)
    {
      context.m_state = CryptoNoteConnectionContext::state_synchronizing;
      NOTIFY_REQUEST_CHAIN::request r = boost::value_initialized<NOTIFY_REQUEST_CHAIN::request>();
      r.block_ids = m_core.buildSparseChain();
      logger(logging::TRACE) << context << "-->>NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << r.block_ids.size();
      post_notify<NOTIFY_REQUEST_CHAIN>(*m_p2p, r, context);
    }
  }
  else
  {
    if (context.m_pending_lite_block)
    {
      context.m_pending_lite_block = boost::none;
      logger(logging::DEBUGGING) << context
                                 << " Peer has a pending lite block but didn't provide all necessary "
                                    "transactions, dropping the connection.";
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
    }
    else
    {
      NOTIFY_MISSING_TXS::request req;
      req.current_blockchain_height = arg.current_blockchain_height;
      req.blockHash = get_block_hash(b);
      req.missing_txs = std::move(need_txs);
      context.m_pending_lite_block = PendingLiteBlock{arg, {req.missing_txs.begin(), req.missing_txs.end()}};

      if (!post_notify<NOTIFY_MISSING_TXS>(*m_p2p, req, context))
      {
        logger(logging::DEBUGGING) << context
                                   << "Lite block is missing transactions but the publisher is not "
                                      "reachable, dropping connection.";
        context.m_state = CryptoNoteConnectionContext::state_shutdown;
      }
    }
  }

  return 1;
}

int CryptoNoteProtocolHandler::handle_request_checkpoint_list(int command, NOTIFY_REQUEST_CHECKPOINT_LIST::request& arg, CryptoNoteConnectionContext& context)
{
  // REFACTORED: This handler now supports both old (full checkpoint lists) and new (chunk-based) systems
  // For backward compatibility with old nodes, we still support full checkpoint list requests
  // However, new nodes should use NOTIFY_REQUEST_CHUNK_HASH for consensus
  
  // Only answer checkpoint requests if we have confirmed chunks
  // This ensures we don't serve unverified data to other nodes
  if (!m_core.getCheckpointList().can_answer_checkpoint_requests())
  {
    logger(logging::DEBUGGING) << context << " Cannot answer checkpoint request: no confirmed chunks available";
    NOTIFY_RESPONSE_CHECKPOINT_LIST::request rsp;
    rsp.list_start_height = 0;
    rsp.checkpoint_list.clear();
    if (!post_notify<NOTIFY_RESPONSE_CHECKPOINT_LIST>(*m_p2p, rsp, context))
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }
  
  // REFACTORED: For chunk-based system, we can still respond with block IDs if requested
  // but this is mainly for backward compatibility with old nodes
  // New nodes should use chunk hash requests instead
  NOTIFY_RESPONSE_CHECKPOINT_LIST::request rsp;
  rsp.list_start_height = arg.start_height;
  rsp.checkpoint_list = m_core.getBlockIds(arg.start_height, arg.end_height);
  
  crypto::Hash hv = crypto::cn_fast_hash(rsp.checkpoint_list.data(), rsp.checkpoint_list.size() * sizeof(crypto::Hash));
  if ( hv != arg.target_hash || rsp.checkpoint_list.size() == 0)
  {
    logger(ERROR) << context << " peer requested different hash for start_height " << arg.start_height
                      << " end_height " << arg.end_height << " checkpoint list size " << rsp.checkpoint_list.size()
                      << " I have hash " << hv << " peer wants " << arg.target_hash;

    rsp.checkpoint_list.clear();
    rsp.list_start_height = 0;
    context.m_checkpoints_not_in_sync = true;
  }
  
  if (!post_notify<NOTIFY_RESPONSE_CHECKPOINT_LIST>(*m_p2p, rsp, context))
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
  return 1;
}

int CryptoNoteProtocolHandler::handle_request_chunk_hash(int command, NOTIFY_REQUEST_CHUNK_HASH::request& arg, CryptoNoteConnectionContext& context)
{
  logger(INFO) << context << " Received chunk hash request for chunk " << arg.chunk_index;
  
  // Only answer chunk hash requests if peer supports chunk-based checkpoints
  if (context.version < cn::P2P_CHECKPOINT_LIST_VERSION)
  {
    logger(INFO) << context << " Peer doesn't support chunk-based checkpoints (version " << context.version << ")";
    NOTIFY_RESPONSE_CHUNK_HASH::request rsp;
    rsp.chunk_index = arg.chunk_index;
    rsp.chunk_hash = NULL_HASH;  // Signal that we can't answer
    if (!post_notify<NOTIFY_RESPONSE_CHUNK_HASH>(*m_p2p, rsp, context))
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }
  
  // Only answer chunk hash requests if we have confirmed chunks
  // This ensures we don't serve unverified data to other nodes
  if (!m_core.getCheckpointList().can_answer_checkpoint_requests())
  {
    logger(INFO) << context << " Cannot answer chunk hash request: no confirmed chunks available";
    NOTIFY_RESPONSE_CHUNK_HASH::request rsp;
    rsp.chunk_index = arg.chunk_index;
    rsp.chunk_hash = NULL_HASH;  // Signal that we can't answer
    if (!post_notify<NOTIFY_RESPONSE_CHUNK_HASH>(*m_p2p, rsp, context))
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }
  
  // Get the chunk hash for the requested chunk index
  // NOTE: We can return chunks that exist in memory (even if unverified) for P2P validation
  // The requester will validate against their own chunk hash
  crypto::Hash chunk_hash = m_core.getCheckpointList().get_chunk_hash(arg.chunk_index);
  
  NOTIFY_RESPONSE_CHUNK_HASH::request rsp;
  rsp.chunk_index = arg.chunk_index;
  rsp.chunk_hash = chunk_hash;
  
  if (chunk_hash == NULL_HASH)
  {
    logger(INFO) << context << " Cannot answer chunk hash request for chunk " << arg.chunk_index 
                             << ": chunk not found in memory (have " 
                             << m_core.getCheckpointList().get_all_chunk_hashes().size() << " chunks)";
  }
  else
  {
    logger(INFO) << context << " Responding to chunk hash request for chunk " << arg.chunk_index 
                             << " with hash " << chunk_hash;
  }
  
  // Always send a response (even if NULL_HASH) so the requester knows we processed the request
  logger(INFO) << context << " Sending chunk hash response for chunk " << arg.chunk_index 
                           << " (hash: " << (chunk_hash == NULL_HASH ? "NULL_HASH" : "valid") << ")";
  if (!post_notify<NOTIFY_RESPONSE_CHUNK_HASH>(*m_p2p, rsp, context))
  {
    logger(ERROR) << context << " Failed to send chunk hash response for chunk " << arg.chunk_index;
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
  }
  else
  {
    logger(INFO) << context << " Successfully sent chunk hash response for chunk " << arg.chunk_index;
  }
  return 1;
}

int CryptoNoteProtocolHandler::handle_response_chunk_hash(int command, NOTIFY_RESPONSE_CHUNK_HASH::request& arg, CryptoNoteConnectionContext& context)
{
  // Only process chunk hash responses if peer supports chunk-based checkpoints
  if (context.version < cn::P2P_CHECKPOINT_LIST_VERSION)
  {
    logger(INFO) << context << " Peer doesn't support chunk-based checkpoints (version " << context.version << ")";
    return 1;
  }
  
  // Find peer_id from context using connection_id (more reliable than pointer comparison)
  uint64_t peer_id = 0;
  m_p2p->for_each_connection([&peer_id, &context](const CryptoNoteConnectionContext& ctx, PeerIdType id) {
    if (ctx.m_connection_id == context.m_connection_id) {
      peer_id = id;
    }
  });
  
  if (peer_id == 0) {
    logger(WARNING) << context << " Cannot find peer ID for chunk hash response (connection_id: " 
                    << context.m_connection_id << ")";
    return 1;
  }
  
  // Store the response in pending chunk hashes map for the consensus mechanism
  // NOTE: NULL_HASH is a valid response (means peer doesn't have this chunk)
  {
    std::lock_guard<std::mutex> lock(m_pending_chunk_hashes_mutex);
    m_pending_chunk_hashes[std::make_pair(peer_id, arg.chunk_index)] = arg.chunk_hash;
  }
  
  if (arg.chunk_hash == NULL_HASH)
  {
    logger(INFO) << context << " Received chunk hash response for chunk " << arg.chunk_index 
                             << ": peer does not have this chunk (NULL_HASH)";
  }
  else
  {
    logger(INFO) << context << " Received chunk hash response for chunk " << arg.chunk_index 
                             << " with hash " << arg.chunk_hash;
  }
  return 1;
}

crypto::Hash CryptoNoteProtocolHandler::request_chunk_hash_from_peer(uint64_t peer_id, uint32_t chunk_index)
{
  // Find the peer connection
  CryptoNoteConnectionContext* peer_context = nullptr;
  uint32_t found_connections = 0;
  m_p2p->for_each_connection([&peer_context, &found_connections, peer_id](CryptoNoteConnectionContext& ctx, uint64_t id) {
    found_connections++;
    if (id == peer_id) {
      peer_context = &ctx;
    }
  });
  
  if (!peer_context) {
    logger(WARNING) << "Cannot request chunk hash from peer " << peer_id << ": peer not found (searched " 
                    << found_connections << " connections)";
    // Log all available peer IDs for debugging
    logger(DEBUGGING) << "Available peer IDs:";
    m_p2p->for_each_connection([&](CryptoNoteConnectionContext& ctx, uint64_t id) {
      logger(DEBUGGING) << "  Peer ID: " << id << ", connection_id: " << ctx.m_connection_id 
                        << ", state: " << get_protocol_state_string(ctx.m_state)
                        << ", version: " << static_cast<int>(ctx.version);
    });
    return NULL_HASH;
  }
  
  // Clear any previous pending response for this peer/chunk
  {
    std::lock_guard<std::mutex> lock(m_pending_chunk_hashes_mutex);
    m_pending_chunk_hashes.erase(std::make_pair(peer_id, chunk_index));
  }
  
  // Send request
  NOTIFY_REQUEST_CHUNK_HASH::request req;
  req.chunk_index = chunk_index;
  
  logger(INFO) << "Requesting chunk hash for chunk " << chunk_index << " from peer " << peer_id 
               << " (connection_id: " << peer_context->m_connection_id 
               << ", state: " << get_protocol_state_string(peer_context->m_state)
               << ", version: " << static_cast<int>(peer_context->version) << ")";
  
  bool sent = post_notify<NOTIFY_REQUEST_CHUNK_HASH>(*m_p2p, req, *peer_context);
  if (!sent) {
    logger(WARNING) << "Failed to send chunk hash request to peer " << peer_id << " for chunk " << chunk_index;
    return NULL_HASH;
  }
  
  logger(INFO) << "Successfully sent chunk hash request for chunk " << chunk_index 
               << " to peer " << peer_id << " (waiting for response...)";
  
  // Wait for response (with timeout)
  // In a real implementation, this should use async/await or a callback mechanism
  // For now, we'll use a simple polling approach with timeout
  const int max_wait_ms = 5000;  // 5 second timeout
  const int poll_interval_ms = 50;
  int waited_ms = 0;
  
  while (waited_ms < max_wait_ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
    waited_ms += poll_interval_ms;
    
    std::lock_guard<std::mutex> lock(m_pending_chunk_hashes_mutex);
    auto it = m_pending_chunk_hashes.find(std::make_pair(peer_id, chunk_index));
    if (it != m_pending_chunk_hashes.end()) {
      crypto::Hash result = it->second;
      m_pending_chunk_hashes.erase(it);
      logger(INFO) << "Received chunk hash response from peer " << peer_id 
                   << " for chunk " << chunk_index << ": " << result;
      return result;
    }
  }
  
  logger(logging::WARNING) << "Timeout waiting for chunk hash response from peer " << peer_id << " for chunk " << chunk_index;
  return NULL_HASH;
}

int CryptoNoteProtocolHandler::handle_response_checkpoint_list(int command, NOTIFY_RESPONSE_CHECKPOINT_LIST::request& arg, CryptoNoteConnectionContext& context)
{
  if( !context.m_active_checkpoint_req )
  {
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  m_last_checkpoint_req = 0;
  context.m_active_checkpoint_req = false;

  if( arg.checkpoint_list.size() == 0 || !m_core.getCheckpointList().add_checkpoint_list(arg.list_start_height, arg.checkpoint_list) )
  {
    context.m_checkpoints_not_in_sync = true;
  }

  return 1;
}

bool CryptoNoteProtocolHandler::validate_unverified_chunks()
{
  // Check if we have unverified chunks first (fast check)
  std::vector<uint32_t> unverified_chunks = m_core.getCheckpointList().get_unverified_chunks();
  if (unverified_chunks.empty())
  {
    return false; // No unverified chunks - nothing to validate
  }
  
  // We need at least some peers to validate (but don't require full synchronization)
  // Validation can happen during sync as long as we have peers
  if (m_peersCount.load() == 0)
  {
    logger(DEBUGGING) << "Cannot validate chunks: no peers connected yet";
    return false;
  }
  
  // Rate limit: don't attempt validation more than once every P2P_CHECKPOINT_LIST_RE_REQUEST seconds (5 minutes)
  // EXCEPTION: Allow immediate attempt if this is the first time we have unverified chunks
  // (bypass rate limit on first attempt after chunks are created)
  uint64_t time_now = time(nullptr);
  bool bypass_rate_limit = false;
  {
    std::lock_guard<std::mutex> lock(m_chunk_validation_mutex);
    
    // Check if this is the first validation attempt (m_last_chunk_validation_attempt == 0)
    // or if enough time has passed
    uint64_t time_since_last = time_now - m_last_chunk_validation_attempt;
    
    if (m_last_chunk_validation_attempt == 0)
    {
      // First validation attempt - allow it immediately
      bypass_rate_limit = true;
      logger(INFO) << "First chunk validation attempt - bypassing rate limit";
    }
    else if (time_since_last < cn::P2P_CHECKPOINT_LIST_RE_REQUEST)
    {
      // Too soon since last attempt
      logger(DEBUGGING) << "Chunk validation rate limited: " 
                        << (cn::P2P_CHECKPOINT_LIST_RE_REQUEST - time_since_last) 
                        << " seconds remaining";
      return false;
    }
    
    m_last_chunk_validation_attempt = time_now;
  }
  
  // Sort to ensure chronological order (oldest first)
  std::sort(unverified_chunks.begin(), unverified_chunks.end());
  
  logger(INFO) << "Attempting to validate " << unverified_chunks.size() 
                        << " unverified chunk(s) via P2P consensus";
  
  // Check if we're already validating a chunk
  {
    std::lock_guard<std::mutex> lock(m_chunk_validation_mutex);
    if (m_current_validating_chunk_index != UINT32_MAX)
    {
      // Already validating a chunk - wait for it to complete
      return false;
    }
  }
  
  // Get current blockchain height to calculate peer uptime in blocks
  uint32_t current_height = get_current_blockchain_height();
  uint32_t block_time = m_currency.difficultyTarget(); // Block time in seconds (120 for mainnet/testnet)
  uint32_t min_uptime_blocks = m_core.getCheckpointList().get_min_peer_uptime_blocks();
  
  // Find eligible peers (meet uptime requirement and support chunk-based checkpoints)
  std::vector<uint64_t> eligible_peers;
  std::map<uint64_t, uint32_t> peer_network_16; // peer_id -> /16 network
  
  m_p2p->for_each_connection([&](CryptoNoteConnectionContext& ctx, uint64_t peer_id) {
    // Only consider peers that support chunk-based checkpoints
    if (ctx.version < cn::P2P_CHECKPOINT_LIST_VERSION)
    {
      return; // Skip old version peers
    }
    
    // Accept peers in normal state (synchronized) OR synchronizing state
    // We can validate chunks even during sync, as long as peers are connected
    if (ctx.m_state != CryptoNoteConnectionContext::state_normal && 
        ctx.m_state != CryptoNoteConnectionContext::state_synchronizing)
    {
      return; // Skip peers that aren't in a usable state
    }
    
    // Calculate peer uptime in blocks
    // Uptime = (current_time - connection_start_time) / block_time
    // This is an approximation - ideally we'd track peer's blockchain height when they first connected
    time_t connection_duration = time_now - ctx.m_started;
    if (connection_duration < 0)
    {
      return; // Invalid connection time
    }
    
    uint32_t peer_uptime_blocks = static_cast<uint32_t>(connection_duration / block_time);
    
    // Check if peer meets minimum uptime requirement
    if (peer_uptime_blocks < min_uptime_blocks)
    {
      return; // Peer doesn't meet uptime requirement
    }
    
    // Peer is eligible
    eligible_peers.push_back(peer_id);
    peer_network_16[peer_id] = CheckpointList::get_network_16(ctx.m_remote_ip);
  });
  
  if (eligible_peers.empty())
  {
    logger(INFO) << "No eligible peers for chunk validation (need uptime > " 
                               << min_uptime_blocks << " blocks, version 2+, and in normal/synchronizing state)";
    logger(INFO) << "Chunk validation will be retried when eligible peers become available";
    return false;
  }
  
  logger(INFO) << "Found " << eligible_peers.size() << " eligible peer(s) for chunk validation "
                        << "(uptime > " << min_uptime_blocks << " blocks, support version 2+)";
  
  // Validate chunks in chronological order (oldest first)
  // This ensures we catch divergences at the root cause
  for (uint32_t chunk_index : unverified_chunks)
  {
    // Check if we should stop (node shutting down)
    if (m_stop.load())
    {
      break;
    }
    
    // Mark this chunk as being validated
    {
      std::lock_guard<std::mutex> lock(m_chunk_validation_mutex);
      if (m_current_validating_chunk_index != UINT32_MAX)
      {
        // Another thread started validation
        break;
      }
      m_current_validating_chunk_index = chunk_index;
    }
    
    // Get local chunk hash
    crypto::Hash local_chunk_hash = m_core.getCheckpointList().get_chunk_hash(chunk_index);
    if (local_chunk_hash == NULL_HASH)
    {
      logger(WARNING) << "Cannot validate chunk " << chunk_index << ": chunk hash not found in memory";
      {
        std::lock_guard<std::mutex> lock(m_chunk_validation_mutex);
        m_current_validating_chunk_index = UINT32_MAX;
      }
      continue;
    }
    
    logger(INFO) << "Validating chunk " << chunk_index << " (oldest unverified chunk) "
                          << "with " << eligible_peers.size() << " eligible peer(s)";
    
    // Prepare functions for consensus mechanism
    uint32_t validating_chunk = chunk_index; // Capture chunk_index for lambda
    auto getPeerChunkHashFunc = [this, validating_chunk](uint64_t peer_id) -> crypto::Hash {
      // Request chunk hash from peer (with timeout)
      crypto::Hash peer_hash = request_chunk_hash_from_peer(peer_id, validating_chunk);
      if (peer_hash == NULL_HASH)
      {
        logger(INFO) << "Peer " << peer_id << " does not have chunk " << validating_chunk 
                     << " in memory (returned NULL_HASH)";
      }
      return peer_hash;
    };
    
    auto getPeerNetwork16Func = [&peer_network_16](uint64_t peer_id) -> uint32_t {
      auto it = peer_network_16.find(peer_id);
      if (it != peer_network_16.end())
      {
        return it->second;
      }
      return 0; // Default network if not found
    };
    
    // Verify chunk with peer consensus
    CheckpointList::ConsensusResult result = m_core.getCheckpointList().verify_chunk_with_peer_consensus(
      chunk_index,
      local_chunk_hash,
      getPeerChunkHashFunc,
      eligible_peers,
      getPeerNetwork16Func
    );
    
    // Clear validation state
    {
      std::lock_guard<std::mutex> lock(m_chunk_validation_mutex);
      m_current_validating_chunk_index = UINT32_MAX;
    }
    
    // Check if all peers returned NULL_HASH (they don't have this chunk yet)
    // In this case, we should wait for peers to create the chunk, not fail consensus
    if (!result.consensus_reached && result.agreements_first_attempt == 0 && result.agreements_second_attempt == 0)
    {
      logger(INFO) << "Chunk " << chunk_index 
                            << " validation: No peers have this chunk in memory yet. "
                            << "Will retry validation once peers create this chunk.";
      // Don't rollback - peers just need to create the chunk first
      continue; // Skip to next chunk or wait
    }
    
    if (result.consensus_reached)
    {
      // Consensus reached - add chunk to checkpoint.dat
      if (m_core.getCheckpointList().add_verified_chunk_to_file(chunk_index))
      {
        logger(INFO, BRIGHT_GREEN) << "Chunk " << chunk_index 
                                                      << " validated and saved to checkpoint.dat via peer consensus";
        
        // Continue to next chunk
        continue;
      }
      else
      {
        logger(ERROR) << "Failed to save validated chunk " << chunk_index << " to checkpoint.dat";
        break; // Stop validation if we can't save
      }
    }
    else
    {
      // Consensus failed - this indicates our blockchain diverged
      // Rollback to the previous chunk boundary
      logger(ERROR, BRIGHT_RED) << "Chunk " << chunk_index 
                                                   << " validation FAILED: peer consensus did not agree with local chunk hash. "
                                                   << "This indicates blockchain divergence. Rolling back to chunk " 
                                                   << (chunk_index > 0 ? chunk_index - 1 : 0) << " boundary.";
      
      // Calculate rollback height
      // chunk[0]: blocks 0 to chunk_size (inclusive)
      // chunk[1]: blocks (chunk_size + 1) to (2 * chunk_size) (inclusive)
      // So chunk N ends at: (N + 1) * chunk_size
      // Rollback to end of previous chunk (chunk_index - 1)
      uint32_t chunk_size = m_core.getCheckpointList().get_chunk_size();
      uint32_t rollback_height;
      if (chunk_index == 0)
      {
        rollback_height = 0; // Rollback to genesis
      }
      else
      {
        // Rollback to end of previous chunk
        // Previous chunk (chunk_index - 1) ends at: chunk_index * chunk_size
        rollback_height = chunk_index * chunk_size;
      }
      
      // Trigger blockchain rollback
      logger(ERROR, BRIGHT_RED) << "Rolling back blockchain to height " << rollback_height 
                                                  << " (chunk " << (chunk_index > 0 ? chunk_index - 1 : 0) << " boundary)";
      
      // Truncate checkpoint.dat to previous chunk
      if (chunk_index > 0)
      {
        if (!m_core.getCheckpointList().truncate_checkpoint_file(chunk_index - 1))
        {
          logger(ERROR) << "Failed to truncate checkpoint.dat after validation failure";
        }
      }
      
      // Trigger blockchain rollback via core
      // Note: This is a critical operation - the blockchain will be rolled back to the chunk boundary
      logger(ERROR, BRIGHT_RED) << "Triggering blockchain rollback to height " << rollback_height 
                                                   << " due to chunk validation failure";
      
      // The rollback will be handled by the core's rollback mechanism
      // For now, we signal the need for rollback by truncating checkpoint.dat
      // The actual blockchain rollback should be triggered separately (e.g., via a flag or callback)
      // TODO: Implement proper rollback trigger mechanism
      
      // Stop validation - we found the divergence point
      // The node will need to resync from the rollback height
      break;
    }
  }
  
  return true;
}

}; // namespace cn

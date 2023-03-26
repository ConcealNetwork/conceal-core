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
  m_observedHeight(0),
  m_peersCount(0),
  logger(log, "protocol"),
  m_dispatcher(dispatcher)
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
  m_p2p->for_each_connection([&connections](const CryptoNoteConnectionContext &cntxt, [[maybe_unused]] PeerIdType peer_id)
                             {
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

    logger(diff >= 0 ? (is_inital ? logging::INFO : DEBUGGING) : logging::TRACE)  << context << "Unknown top block: " << get_current_blockchain_height() << " -> " << hshd.current_height
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

  switch (command)
  {
    HANDLE_NOTIFY(NOTIFY_NEW_BLOCK, &CryptoNoteProtocolHandler::handle_notify_new_block)
    HANDLE_NOTIFY(NOTIFY_NEW_TRANSACTIONS, &CryptoNoteProtocolHandler::handle_notify_new_transactions)
    HANDLE_NOTIFY(NOTIFY_REQUEST_GET_OBJECTS, &CryptoNoteProtocolHandler::handle_request_get_objects)
    HANDLE_NOTIFY(NOTIFY_RESPONSE_GET_OBJECTS, &CryptoNoteProtocolHandler::handle_response_get_objects)
    HANDLE_NOTIFY(NOTIFY_REQUEST_CHAIN, &CryptoNoteProtocolHandler::handle_request_chain)
    HANDLE_NOTIFY(NOTIFY_RESPONSE_CHAIN_ENTRY, &CryptoNoteProtocolHandler::handle_response_chain_entry)
    HANDLE_NOTIFY(NOTIFY_REQUEST_TX_POOL, &CryptoNoteProtocolHandler::handle_request_tx_pool)
    HANDLE_NOTIFY(NOTIFY_NEW_LITE_BLOCK, &CryptoNoteProtocolHandler::handle_notify_new_lite_block)
    HANDLE_NOTIFY(NOTIFY_MISSING_TXS, &CryptoNoteProtocolHandler::handle_notify_missing_txs)

  default:
    handled = false;
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
  if(arg.blocks.size() > COMMAND_RPC_GET_OBJECTS_MAX_COUNT || arg.txs.size() > COMMAND_RPC_GET_OBJECTS_MAX_COUNT)
  {
    logger(logging::ERROR) << context << "GET_OBJECTS_MAX_COUNT exceeded blocks: " << arg.blocks.size() << " txes: " << arg.txs.size();
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

}; // namespace cn

// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "HttpServer.h"

#include <functional>
#include <unordered_map>

#include <Logging/LoggerRef.h>
#include "Common/Math.h"
#include "CoreRpcServerCommandsDefinitions.h"

namespace cn {

class core;
class NodeServer;
class ICryptoNoteProtocolQuery;

class RpcServer : public HttpServer {
public:
  RpcServer(platform_system::Dispatcher& dispatcher, logging::ILogger& log, core& c, NodeServer& p2p, const ICryptoNoteProtocolQuery& protocolQuery);
  typedef std::function<bool(RpcServer*, const HttpRequest& request, HttpResponse& response)> HandlerFunction;
  bool setFeeAddress(const std::string& fee_address, const AccountPublicAddress& fee_acc);
  bool setViewKey(const std::string& view_key);
  bool restrictRPC(const bool is_resctricted);
  bool k_on_check_tx_proof(const K_COMMAND_RPC_CHECK_TX_PROOF::request& req, K_COMMAND_RPC_CHECK_TX_PROOF::response& res);
  bool k_on_check_reserve_proof(const K_COMMAND_RPC_CHECK_RESERVE_PROOF::request& req, K_COMMAND_RPC_CHECK_RESERVE_PROOF::response& res);
  bool remotenode_check_incoming_tx(const BinaryArray& tx_blob);
  bool enableCors(const std::string domain);
  std::string getCorsDomain();

private:

  template <class Handler>
  struct RpcHandler {
    const Handler handler;
    const bool allowBusyCore;
  };

  typedef void (RpcServer::*HandlerPtr)(const HttpRequest& request, HttpResponse& response);
  static std::unordered_map<std::string, RpcHandler<HandlerFunction>> s_handlers;

  virtual void processRequest(const HttpRequest& request, HttpResponse& response) override;
  bool processJsonRpcRequest(const HttpRequest& request, HttpResponse& response);
  bool isCoreReady();

  // binary handlers
  bool on_get_blocks(const COMMAND_RPC_GET_BLOCKS_FAST::request& req, COMMAND_RPC_GET_BLOCKS_FAST::response& res);
  bool on_query_blocks(const COMMAND_RPC_QUERY_BLOCKS::request& req, COMMAND_RPC_QUERY_BLOCKS::response& res);
  bool on_query_blocks_lite(const COMMAND_RPC_QUERY_BLOCKS_LITE::request& req, COMMAND_RPC_QUERY_BLOCKS_LITE::response& res);
  bool on_get_indexes(const COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::request& req, COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::response& res);
  bool on_get_random_outs_bin(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request& req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response& res);
  bool onGetPoolChanges(const COMMAND_RPC_GET_POOL_CHANGES::request& req, COMMAND_RPC_GET_POOL_CHANGES::response& rsp);
  bool onGetPoolChangesLite(const COMMAND_RPC_GET_POOL_CHANGES_LITE::request& req, COMMAND_RPC_GET_POOL_CHANGES_LITE::response& rsp);

  // json handlers
  bool on_get_info(const COMMAND_RPC_GET_INFO::request& req, COMMAND_RPC_GET_INFO::response& res);
  bool on_get_height(const COMMAND_RPC_GET_HEIGHT::request& req, COMMAND_RPC_GET_HEIGHT::response& res);
  bool on_get_peer_list(const COMMAND_RPC_GET_PEER_LIST::request& req, COMMAND_RPC_GET_PEER_LIST::response& res);
  bool on_get_transactions(const COMMAND_RPC_GET_TRANSACTIONS::request& req, COMMAND_RPC_GET_TRANSACTIONS::response& res);
  bool on_get_random_outs_json(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_JSON::request& req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_JSON::response& res);
  bool on_send_raw_tx(const COMMAND_RPC_SEND_RAW_TX::request& req, COMMAND_RPC_SEND_RAW_TX::response& res);
  bool on_start_mining(const COMMAND_RPC_START_MINING::request& req, COMMAND_RPC_START_MINING::response& res);
  bool on_stop_mining(const COMMAND_RPC_STOP_MINING::request& req, COMMAND_RPC_STOP_MINING::response& res);
  bool on_stop_daemon(const COMMAND_RPC_STOP_DAEMON::request& req, COMMAND_RPC_STOP_DAEMON::response& res);
  bool on_get_fee_address(const COMMAND_RPC_GET_FEE_ADDRESS::request& req, COMMAND_RPC_GET_FEE_ADDRESS::response& res);
  bool on_alt_blocks_list_json(const COMMAND_RPC_GET_ALT_BLOCKS_LIST::request &req, COMMAND_RPC_GET_ALT_BLOCKS_LIST::response &res);
  bool on_get_txs_with_output_global_indexes(const COMMAND_RPC_GET_TRANSACTIONS_WITH_OUTPUT_GLOBAL_INDEXES::request& req, COMMAND_RPC_GET_TRANSACTIONS_WITH_OUTPUT_GLOBAL_INDEXES::response& rsp);

  // json rpc
  bool on_getblockcount(const COMMAND_RPC_GETBLOCKCOUNT::request& req, COMMAND_RPC_GETBLOCKCOUNT::response& res);
  bool on_getblockhash(const COMMAND_RPC_GETBLOCKHASH::request& req, COMMAND_RPC_GETBLOCKHASH::response& res);
  bool on_getblocktemplate(const COMMAND_RPC_GETBLOCKTEMPLATE::request& req, COMMAND_RPC_GETBLOCKTEMPLATE::response& res);
  bool on_get_currency_id(const COMMAND_RPC_GET_CURRENCY_ID::request& req, COMMAND_RPC_GET_CURRENCY_ID::response& res);
  bool on_submitblock(const COMMAND_RPC_SUBMITBLOCK::request& req, COMMAND_RPC_SUBMITBLOCK::response& res);
  bool on_get_last_block_header(const COMMAND_RPC_GET_LAST_BLOCK_HEADER::request& req, COMMAND_RPC_GET_LAST_BLOCK_HEADER::response& res);
  bool on_get_block_header_by_hash(const COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::request& req, COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::response& res);
  bool on_get_block_header_by_height(const COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::request& req, COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::response& res);
  bool on_get_block_timestamp_by_height(const COMMAND_RPC_GET_BLOCK_TIMESTAMP_BY_HEIGHT::request& req, COMMAND_RPC_GET_BLOCK_TIMESTAMP_BY_HEIGHT::response& res);
  bool on_get_block_details_by_height(const COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT::request& req, COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT::response& res);
  bool on_get_transactions_pool_raw(const COMMAND_RPC_GET_RAW_TRANSACTIONS_POOL::request& req, COMMAND_RPC_GET_RAW_TRANSACTIONS_POOL::response& res);

  void fill_block_header_response(const Block& blk, bool orphan_status, uint64_t height, const crypto::Hash& hash, block_header_response& responce);

  bool f_on_blocks_list_json(const F_COMMAND_RPC_GET_BLOCKS_LIST::request& req, F_COMMAND_RPC_GET_BLOCKS_LIST::response& res);
  bool f_on_block_json(const F_COMMAND_RPC_GET_BLOCK_DETAILS::request& req, F_COMMAND_RPC_GET_BLOCK_DETAILS::response& res);
  bool f_on_transaction_json(const F_COMMAND_RPC_GET_TRANSACTION_DETAILS::request& req, F_COMMAND_RPC_GET_TRANSACTION_DETAILS::response& res);
  bool f_on_transactions_pool_json(const F_COMMAND_RPC_GET_POOL::request& req, F_COMMAND_RPC_GET_POOL::response& res);
  bool f_getMixin(const Transaction& transaction, uint64_t& mixin);

  bool fill_f_block_details_response(const crypto::Hash& hash, f_block_details_response& block);

  logging::LoggerRef logger;
  core& m_core;
  NodeServer& m_p2p;
  const ICryptoNoteProtocolQuery& m_protocolQuery;
  bool m_restricted_rpc;
  std::string m_cors_domain;
  std::string m_fee_address;
  crypto::SecretKey m_view_key = NULL_SECRET_KEY;
  AccountPublicAddress m_fee_acc; 
};

}

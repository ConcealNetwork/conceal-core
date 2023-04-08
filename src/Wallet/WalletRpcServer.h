// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma  once

#include <future>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include "WalletRpcServerCommandsDefinitions.h"
#include "WalletLegacy/WalletLegacy.h"
#include "Common/CommandLine.h"
#include "Rpc/HttpServer.h"

#include <Logging/LoggerRef.h>

namespace tools
{
  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  class wallet_rpc_server : cn::HttpServer
  {
  public:

    wallet_rpc_server(
      platform_system::Dispatcher& dispatcher,
      logging::ILogger& log,
      cn::IWalletLegacy &w,
      cn::INode &n,
      cn::Currency& currency,
      const std::string& walletFilename);


    static void init_options(boost::program_options::options_description& desc);
    bool init(const boost::program_options::variables_map& vm);

    bool run();
    void send_stop_signal();

    static const command_line::arg_descriptor<uint16_t> arg_rpc_bind_port;
    static const command_line::arg_descriptor<std::string> arg_rpc_bind_ip;
    static const command_line::arg_descriptor<std::string> arg_rpc_user;
    static const command_line::arg_descriptor<std::string> arg_rpc_password;

  private:

    virtual void processRequest(const cn::HttpRequest& request, cn::HttpResponse& response) override;

    //json_rpc
    bool on_getbalance(const wallet_rpc::COMMAND_RPC_GET_BALANCE::request& req, wallet_rpc::COMMAND_RPC_GET_BALANCE::response& res);
    bool on_create_integrated(const wallet_rpc::COMMAND_RPC_CREATE_INTEGRATED::request& req, wallet_rpc::COMMAND_RPC_CREATE_INTEGRATED::response& res);
    bool on_transfer(const wallet_rpc::COMMAND_RPC_TRANSFER::request& req, wallet_rpc::COMMAND_RPC_TRANSFER::response& res);
    bool on_store(const wallet_rpc::COMMAND_RPC_STORE::request& req, wallet_rpc::COMMAND_RPC_STORE::response& res);
    bool on_get_messages(const wallet_rpc::COMMAND_RPC_GET_MESSAGES::request& req, wallet_rpc::COMMAND_RPC_GET_MESSAGES::response& res);
    bool on_get_payments(const wallet_rpc::COMMAND_RPC_GET_PAYMENTS::request& req, wallet_rpc::COMMAND_RPC_GET_PAYMENTS::response& res);
    bool on_get_transfers(const wallet_rpc::COMMAND_RPC_GET_TRANSFERS::request& req, wallet_rpc::COMMAND_RPC_GET_TRANSFERS::response& res);
	  bool on_get_tx_proof(const wallet_rpc::COMMAND_RPC_GET_TX_PROOF::request& req, wallet_rpc::COMMAND_RPC_GET_TX_PROOF::response& res);
	  bool on_get_reserve_proof(const wallet_rpc::COMMAND_RPC_GET_BALANCE_PROOF::request& req, wallet_rpc::COMMAND_RPC_GET_BALANCE_PROOF::response& res);    
    bool on_get_height(const wallet_rpc::COMMAND_RPC_GET_HEIGHT::request& req, wallet_rpc::COMMAND_RPC_GET_HEIGHT::response& res);
    bool on_get_outputs(const wallet_rpc::COMMAND_RPC_GET_OUTPUTS::request& req, wallet_rpc::COMMAND_RPC_GET_OUTPUTS::response& res);
    bool on_optimize(const wallet_rpc::COMMAND_RPC_OPTIMIZE::request& req, wallet_rpc::COMMAND_RPC_OPTIMIZE::response& res);
    bool on_estimate_fusion(const wallet_rpc::COMMAND_RPC_ESTIMATE_FUSION::request& req, wallet_rpc::COMMAND_RPC_ESTIMATE_FUSION::response& res);
    bool on_send_fusion(const wallet_rpc::COMMAND_RPC_SEND_FUSION::request& req, wallet_rpc::COMMAND_RPC_SEND_FUSION::response& res);
    bool on_reset(const wallet_rpc::COMMAND_RPC_RESET::request& req, wallet_rpc::COMMAND_RPC_RESET::response& res);

    bool handle_command_line(const boost::program_options::variables_map& vm);

    logging::LoggerRef logger;
    cn::IWalletLegacy& m_wallet;
    cn::INode& m_node;
    uint16_t m_port;
    std::string m_bind_ip;
    std::string m_rpcUser;
    std::string m_rpcPassword;
    cn::Currency& m_currency;
    const std::string m_walletFilename;

    platform_system::Dispatcher& m_dispatcher;
    platform_system::Event m_stopComplete;
  };
}

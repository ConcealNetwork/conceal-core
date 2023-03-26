// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "RpcServerConfig.h"
#include "Common/CommandLine.h"
#include "CryptoNoteConfig.h"

namespace cn {

  namespace {

    const std::string DEFAULT_RPC_IP = "127.0.0.1";
    const uint16_t DEFAULT_RPC_PORT = RPC_DEFAULT_PORT;

    const command_line::arg_descriptor<std::string> arg_rpc_bind_ip = { "rpc-bind-ip", "", DEFAULT_RPC_IP };
    const command_line::arg_descriptor<uint16_t> arg_rpc_bind_port = { "rpc-bind-port", "", DEFAULT_RPC_PORT };
    const command_line::arg_descriptor<std::string> arg_enable_cors = { "enable-cors", "Adds header 'Access-Control-Allow-Origin' to the daemon's RPC responses. Uses the value as domain. Use * for all", "" };
  }


  RpcServerConfig::RpcServerConfig() : bindIp(DEFAULT_RPC_IP), bindPort(DEFAULT_RPC_PORT), enableCors("") {
  }

  std::string RpcServerConfig::getBindAddress() const {
    return bindIp + ":" + std::to_string(bindPort);
  }
  
  void RpcServerConfig::initOptions(boost::program_options::options_description& desc) {
    command_line::add_arg(desc, arg_rpc_bind_ip);
    command_line::add_arg(desc, arg_rpc_bind_port);
    command_line::add_arg(desc, arg_enable_cors);
  }

  void RpcServerConfig::init(const boost::program_options::variables_map &vm)
  {
    bool testnet = vm[command_line::arg_testnet_on.name].as<bool>();
    bindIp = command_line::get_arg(vm, arg_rpc_bind_ip);
    bindPort = command_line::get_arg(vm, arg_rpc_bind_port);
    uint16_t argPort = command_line::get_arg(vm, arg_rpc_bind_port);
    bindPort = argPort;
    if (testnet)
    {
      bindPort = TESTNET_RPC_DEFAULT_PORT;
      if (!vm[arg_rpc_bind_port.name].defaulted())
      {
        bindPort = argPort;
      }
    }
    enableCors = command_line::get_arg(vm, arg_enable_cors);
  }
}

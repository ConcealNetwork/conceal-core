// Copyright (c) 2018-2022 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "Common/CommandLine.h"

namespace {
  const command_line::arg_descriptor<std::string> arg_wallet_file = { "wallet-file", "Use wallet <arg>", "" };
  const command_line::arg_descriptor<std::string> arg_generate_new_wallet = { "generate-new-wallet", "Generate new wallet and save it to <arg>", "" };
  const command_line::arg_descriptor<std::string> arg_daemon_address = { "daemon-address", "Use daemon instance at <host>:<port>", "" };
  const command_line::arg_descriptor<std::string> arg_daemon_host = { "daemon-host", "Use daemon instance at host <arg> instead of localhost", "" };
  const command_line::arg_descriptor<std::string> arg_password = { "password", "Wallet password", "", true };
  const command_line::arg_descriptor<uint16_t>    arg_daemon_port = { "daemon-port", "Use daemon instance at port <arg> instead of default", cn::RPC_DEFAULT_PORT };
  const command_line::arg_descriptor<uint32_t>    arg_log_level = { "set_log", "", logging::INFO, true };
  const command_line::arg_descriptor<bool>        arg_testnet = { "testnet", "Used to deploy test nets. The daemon must be launched with --testnet flag", false };
  const command_line::arg_descriptor< std::vector<std::string> > arg_command = { "command", "" };

  const size_t TIMESTAMP_MAX_WIDTH = 32;
  const size_t HASH_MAX_WIDTH = 64;
  const size_t TOTAL_AMOUNT_MAX_WIDTH = 20;
  const size_t FEE_MAX_WIDTH = 14;
  const size_t BLOCK_MAX_WIDTH = 7;
  const size_t UNLOCK_TIME_MAX_WIDTH = 11;

}
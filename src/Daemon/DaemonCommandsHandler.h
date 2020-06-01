// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <boost/format.hpp>

#include "Common/ConsoleHandler.h"
#include "CryptoNoteProtocol/ICryptoNoteProtocolQuery.h"
#include "Rpc/RpcServer.h"

#include <Logging/LoggerRef.h>
#include <Logging/LoggerManager.h>

namespace CryptoNote {
class core;
class Currency;
class NodeServer;
class ICryptoNoteProtocolQuery;
}

class DaemonCommandsHandler
{
public:
  DaemonCommandsHandler(CryptoNote::core& core, CryptoNote::NodeServer& srv, Logging::LoggerManager& log, const CryptoNote::ICryptoNoteProtocolQuery& protocol, CryptoNote::RpcServer* prpc_server);

  bool start_handling() {
    m_consoleHandler.start();
    return true;
  }

  void stop_handling() {
    m_consoleHandler.stop();
  }

private:

  Common::ConsoleHandler m_consoleHandler;
  CryptoNote::core& m_core;
  CryptoNote::NodeServer& m_srv;
  Logging::LoggerRef logger;
  Logging::LoggerManager& m_logManager;
  const CryptoNote::ICryptoNoteProtocolQuery& protocolQuery;
  CryptoNote::RpcServer* m_prpc_server;

  std::string get_commands_str();
  std::string get_mining_speed(uint32_t hr);
  float get_sync_percentage(uint64_t height, uint64_t target_height);
  bool print_block_by_height(uint32_t height);
  bool print_block_by_hash(const std::string& arg);
  uint64_t calculatePercent(const CryptoNote::Currency& currency, uint64_t value, uint64_t total);

  bool exit(const std::vector<std::string>& args);
  bool help(const std::vector<std::string>& args);
  bool print_pl(const std::vector<std::string>& args);
  bool show_hr(const std::vector<std::string>& args);
  bool hide_hr(const std::vector<std::string>& args);
  bool rollbackchainto(uint32_t height);  
  bool rollback_chain(const std::vector<std::string>& args);  
  bool print_bc_outs(const std::vector<std::string>& args);
  bool print_cn(const std::vector<std::string>& args);
  bool print_bc(const std::vector<std::string>& args);
  bool print_bci(const std::vector<std::string>& args);
  bool set_log(const std::vector<std::string>& args);
  bool print_block(const std::vector<std::string>& args);
  bool print_tx(const std::vector<std::string>& args);
  bool print_pool(const std::vector<std::string>& args);
  bool print_pool_sh(const std::vector<std::string>& args);
  bool print_stat(const std::vector<std::string>& args);

  bool start_mining(const std::vector<std::string>& args);
  bool stop_mining(const std::vector<std::string>& args);
};

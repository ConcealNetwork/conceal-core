// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "CryptoTypes.h"

#include <System/ContextGroup.h>
#include <System/Dispatcher.h>
#include <System/Event.h>

#include "Logging/LoggerRef.h"

class BlockchainMonitor {
public:
  BlockchainMonitor(System::Dispatcher& dispatcher, const std::string& daemonHost, uint16_t daemonPort, uint64_t pollingInterval, Logging::ILogger& logger);

  void waitBlockchainUpdate();
  void stop();
private:
  System::Dispatcher& m_dispatcher;
  std::string m_daemonHost;
  uint16_t m_daemonPort;
  uint64_t m_pollingInterval;
  bool m_stopped;
  System::Event m_httpEvent;
  System::ContextGroup m_sleepingContext;

  Logging::LoggerRef m_logger;

  Crypto::Hash requestLastBlockHash();
};

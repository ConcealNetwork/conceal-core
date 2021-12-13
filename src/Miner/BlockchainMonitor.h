// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2021 Conceal Network & Conceal Devs
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
  BlockchainMonitor(platform_system::Dispatcher& dispatcher, const std::string& daemonHost, uint16_t daemonPort, size_t pollingInterval, logging::ILogger& logger);

  void waitBlockchainUpdate();
  void stop();
private:
  platform_system::Dispatcher& m_dispatcher;
  std::string m_daemonHost;
  uint16_t m_daemonPort;
  size_t m_pollingInterval;
  bool m_stopped;
  platform_system::Event m_httpEvent;
  platform_system::ContextGroup m_sleepingContext;

  logging::LoggerRef m_logger;

  crypto::Hash requestLastBlockHash();
};

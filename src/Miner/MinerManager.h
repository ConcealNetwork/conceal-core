// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2021 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <queue>

#include <System/ContextGroup.h>
#include <System/Event.h>

#include "BlockchainMonitor.h"
#include "Logging/LoggerRef.h"
#include "Miner.h"
#include "MinerEvent.h"
#include "MiningConfig.h"

namespace System {
class Dispatcher;
}

namespace Miner {

class MinerManager {
public:
  MinerManager(System::Dispatcher& dispatcher, const cn::MiningConfig& config, Logging::ILogger& logger);
  ~MinerManager();

  void start();

private:
  System::Dispatcher& m_dispatcher;
  Logging::LoggerRef m_logger;
  System::ContextGroup m_contextGroup;
  cn::MiningConfig m_config;
  cn::Miner m_miner;
  BlockchainMonitor m_blockchainMonitor;

  System::Event m_eventOccurred;
  System::Event m_httpEvent;
  std::queue<MinerEvent> m_events;

  cn::Block m_minedBlock;

  uint64_t m_lastBlockTimestamp;

  void eventLoop();
  MinerEvent waitEvent();
  void pushEvent(MinerEvent&& event);

  void startMining(const cn::BlockMiningParameters& params);
  void stopMining();

  void startBlockchainMonitoring();
  void stopBlockchainMonitoring();

  bool submitBlock(const cn::Block& minedBlock, const std::string& daemonHost, uint16_t daemonPort);
  cn::BlockMiningParameters requestMiningParameters(System::Dispatcher& dispatcher, const std::string& daemonHost, uint16_t daemonPort, const std::string& miningAddress);

  void adjustBlockTemplate(cn::Block& blockTemplate) const;
};

} //namespace Miner

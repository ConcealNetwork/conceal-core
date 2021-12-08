// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2021 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "BlockchainMonitor.h"

#include "Common/StringTools.h"

#include <System/EventLock.h>
#include <System/Timer.h>
#include <System/InterruptedException.h>

#include "Rpc/CoreRpcServerCommandsDefinitions.h"
#include "Rpc/JsonRpc.h"
#include "Rpc/HttpClient.h"

BlockchainMonitor::BlockchainMonitor(System::Dispatcher& dispatcher, const std::string& daemonHost, uint16_t daemonPort, size_t pollingInterval, logging::ILogger& logger):
  m_dispatcher(dispatcher),
  m_daemonHost(daemonHost),
  m_daemonPort(daemonPort),
  m_pollingInterval(pollingInterval),
  m_stopped(false),
  m_httpEvent(dispatcher),
  m_sleepingContext(dispatcher),
  m_logger(logger, "BlockchainMonitor") {

  m_httpEvent.set();
}

void BlockchainMonitor::waitBlockchainUpdate() {
  m_logger(logging::DEBUGGING) << "Waiting for blockchain updates";
  m_stopped = false;

  crypto::Hash lastBlockHash = requestLastBlockHash();

  while(!m_stopped) {
    m_sleepingContext.spawn([this] () {
      System::Timer timer(m_dispatcher);
      timer.sleep(std::chrono::seconds(m_pollingInterval));
    });

    m_sleepingContext.wait();

    if (lastBlockHash != requestLastBlockHash()) {
      m_logger(logging::DEBUGGING) << "Blockchain has been updated";
      break;
    }
  }

  if (m_stopped) {
    m_logger(logging::DEBUGGING) << "Blockchain monitor has been stopped";
    throw System::InterruptedException();
  }
}

void BlockchainMonitor::stop() {
  m_logger(logging::DEBUGGING) << "Sending stop signal to blockchain monitor";
  m_stopped = true;

  m_sleepingContext.interrupt();
  m_sleepingContext.wait();
}

crypto::Hash BlockchainMonitor::requestLastBlockHash() {
  m_logger(logging::DEBUGGING) << "Requesting last block hash";

  try {
    cn::HttpClient client(m_dispatcher, m_daemonHost, m_daemonPort);

    cn::COMMAND_RPC_GET_LAST_BLOCK_HEADER::request request;
    cn::COMMAND_RPC_GET_LAST_BLOCK_HEADER::response response;

    System::EventLock lk(m_httpEvent);
    cn::JsonRpc::invokeJsonRpcCommand(client, "getlastblockheader", request, response);

    if (response.status != CORE_RPC_STATUS_OK) {
      throw std::runtime_error("Core responded with wrong status: " + response.status);
    }

    crypto::Hash blockHash;
    if (!common::podFromHex(response.block_header.hash, blockHash)) {
      throw std::runtime_error("Couldn't parse block hash: " + response.block_header.hash);
    }

    m_logger(logging::DEBUGGING) << "Last block hash: " << common::podToHex(blockHash);

    return blockHash;
  } catch (std::exception& e) {
    m_logger(logging::ERROR) << "Failed to request last block hash: " << e.what();
    throw;
  }
}

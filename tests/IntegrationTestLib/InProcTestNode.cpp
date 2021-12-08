// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "InProcTestNode.h"

#include <future>

#include <Common/StringTools.h>
#include <Logging/ConsoleLogger.h>

#include "CryptoNoteCore/Core.h"
#include "CryptoNoteCore/CoreConfig.h"
#include "CryptoNoteCore/Miner.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "P2p/NetNode.h"
#include "InProcessNode/InProcessNode.h"

using namespace cn;

#undef ERROR

namespace Tests {

namespace {
bool parse_peer_from_string(NetworkAddress &pe, const std::string &node_addr) {
  return ::common::parseIpAddressAndPort(pe.ip, pe.port, node_addr);
}
}


InProcTestNode::InProcTestNode(const TestNodeConfiguration& cfg, const cn::Currency& currency) : 
  m_cfg(cfg), m_currency(currency) {

  std::promise<std::string> initPromise;
  std::future<std::string> initFuture = initPromise.get_future();

  m_thread = std::thread(std::bind(&InProcTestNode::workerThread, this, std::ref(initPromise)));
  auto initError = initFuture.get();

  if (!initError.empty()) {
    m_thread.join();
    throw std::runtime_error(initError);
  }
}

InProcTestNode::~InProcTestNode() {
  if (m_thread.joinable()) {
    m_thread.join();
  }
}

void InProcTestNode::workerThread(std::promise<std::string>& initPromise) {

  platform_system::Dispatcher dispatcher;

  logging::ConsoleLogger log;

  logging::LoggerRef logger(log, "InProcTestNode");

  try {

    core.reset(new cn::core(m_currency, NULL, log));
    protocol.reset(new cn::CryptoNoteProtocolHandler(m_currency, dispatcher, *core, NULL, log));
    p2pNode.reset(new cn::NodeServer(dispatcher, *protocol, log));
    protocol->set_p2p_endpoint(p2pNode.get());
    core->set_cryptonote_protocol(protocol.get());

    cn::NetNodeConfig p2pConfig;

    p2pConfig.setBindIp("127.0.0.1");
    p2pConfig.setBindPort(m_cfg.p2pPort);
    p2pConfig.setExternalPort(0);
    p2pConfig.setAllowLocalIp(false);
    p2pConfig.setHideMyPort(false);
    p2pConfig.setConfigFolder(m_cfg.dataDir);

    std::vector<NetworkAddress> exclusiveNodes;
    for (const auto& en : m_cfg.exclusiveNodes) {
      NetworkAddress na;
      parse_peer_from_string(na, en);
      exclusiveNodes.push_back(na);
    }

    p2pConfig.setExclusiveNodes(exclusiveNodes);

    if (!p2pNode->init(p2pConfig)) {
      throw std::runtime_error("Failed to init p2pNode");
    }

    cn::MinerConfig emptyMiner;
    cn::CoreConfig coreConfig;

    coreConfig.configFolder = m_cfg.dataDir;
    
    if (!core->init(coreConfig, emptyMiner, true)) {
      throw std::runtime_error("Core failed to initialize");
    }

    initPromise.set_value(std::string());

  } catch (std::exception& e) {
    logger(logging::ERROR) << "Failed to initialize: " << e.what();
    initPromise.set_value(e.what());
    return;
  }

  try {
    p2pNode->run();
  } catch (std::exception& e) {
    logger(logging::ERROR) << "exception in p2p::run: " << e.what();
  }

  core->deinit();
  p2pNode->deinit();
  core->set_cryptonote_protocol(NULL);
  protocol->set_p2p_endpoint(NULL);

  p2pNode.reset();
  protocol.reset();
  core.reset();
}

bool InProcTestNode::startMining(size_t threadsCount, const std::string &address) {
  assert(core.get());
  AccountPublicAddress addr;
  m_currency.parseAccountAddressString(address, addr);
  return core->get_miner().start(addr, threadsCount);
}

bool InProcTestNode::stopMining() {
  assert(core.get());
  return core->get_miner().stop();
}

bool InProcTestNode::stopDaemon() {
  if (!p2pNode.get()) {
    return false;
  }

  p2pNode->sendStopSignal();
  m_thread.join();
  return true;
}

bool InProcTestNode::getBlockTemplate(const std::string &minerAddress, cn::Block &blockTemplate, uint64_t &difficulty) {
  AccountPublicAddress addr;
  m_currency.parseAccountAddressString(minerAddress, addr);
  uint32_t height = 0;
  return core->get_block_template(blockTemplate, addr, difficulty, height, BinaryArray());
}

bool InProcTestNode::submitBlock(const std::string& block) {
  block_verification_context bvc = boost::value_initialized<block_verification_context>();
  core->handle_incoming_block_blob(common::fromHex(block), bvc, true, true);
  return bvc.m_added_to_main_chain;
}

bool InProcTestNode::getTailBlockId(crypto::Hash &tailBlockId) {
  tailBlockId = core->get_tail_id();
  return true;
}

bool InProcTestNode::makeINode(std::unique_ptr<cn::INode> &node) {

  std::unique_ptr<InProcessNode> inprocNode(new cn::InProcessNode(*core, *protocol));

  std::promise<std::error_code> p;
  auto future = p.get_future();

  inprocNode->init([&p](std::error_code ec) {
    std::promise<std::error_code> localPromise(std::move(p));
    localPromise.set_value(ec);
  });

  auto ec = future.get();

  if (!ec) {
    node = std::move(inprocNode);
    return true;
  }

  return false;
}

uint64_t InProcTestNode::getLocalHeight() {
  return core->get_current_blockchain_height();
}

}

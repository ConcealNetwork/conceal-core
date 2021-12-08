// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "TestNode.h"
#include "NetworkConfiguration.h"

#include <future>
#include <memory>
#include <thread>

#include <System/Dispatcher.h>


namespace cn {
class core;
class CryptoNoteProtocolHandler;
class NodeServer;
class Currency;
}

namespace Tests {

class InProcTestNode : public TestNode {
public:
  InProcTestNode(const TestNodeConfiguration& cfg, const cn::Currency& currency);
  ~InProcTestNode();

  virtual bool startMining(size_t threadsCount, const std::string &address) override;
  virtual bool stopMining() override;
  virtual bool stopDaemon() override;
  virtual bool getBlockTemplate(const std::string &minerAddress, cn::Block &blockTemplate, uint64_t &difficulty) override;
  virtual bool submitBlock(const std::string& block) override;
  virtual bool getTailBlockId(crypto::Hash &tailBlockId) override;
  virtual bool makeINode(std::unique_ptr<cn::INode>& node) override;
  virtual uint64_t getLocalHeight() override;

private:

  void workerThread(std::promise<std::string>& initPromise);

  std::unique_ptr<cn::core> core;
  std::unique_ptr<cn::CryptoNoteProtocolHandler> protocol;
  std::unique_ptr<cn::NodeServer> p2pNode;

  std::thread m_thread;
  const cn::Currency& m_currency;
  TestNodeConfiguration m_cfg;
};

}

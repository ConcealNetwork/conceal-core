// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "NetworkConfiguration.h"
#include "Process.h"
#include "TestNode.h"

namespace platform_system {
class Dispatcher;
}

namespace cn {
class Currency;
}

namespace Tests {

enum class Topology {
  Ring,
  Line,
  Star
};


class TestNetworkBuilder {
public:

  TestNetworkBuilder(size_t nodeCount, Topology topology = Topology::Line, uint16_t rpcBasePort = 9200, uint16_t p2pBasePort = 9000);

  TestNetworkBuilder& setDataDirectory(const std::string& dataDir);
  TestNetworkBuilder& setBlockchain(const std::string& blockchainDir);
  TestNetworkBuilder& setTestnet(bool isTestnet);

  std::vector<TestNodeConfiguration> build();

private:

  TestNodeConfiguration buildNodeConfiguration(size_t index);

  uint16_t rpcBasePort;
  uint16_t p2pBasePort;
  Topology topology;
  size_t nodeCount;
  std::string baseDataDir;
  std::string blockchainLocation;
  bool testnet;
};


class TestNetwork {

public:

  TestNetwork(platform_system::Dispatcher& dispatcher, const cn::Currency& currency);

  void addNodes(const std::vector<TestNodeConfiguration>& nodes);
  void addNode(const TestNodeConfiguration& cfg);
  void waitNodesReady();
  void shutdown();

  TestNode& getNode(size_t index);

private:

  std::unique_ptr<TestNode> startDaemon(const TestNodeConfiguration& cfg);

  std::vector<std::pair<std::unique_ptr<TestNode>, TestNodeConfiguration>> nodes;
  platform_system::Dispatcher& m_dispatcher;
  const cn::Currency& m_currency;
  std::vector<Process> m_daemons;
};

}

// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2021 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "BaseTests.h"

#include <System/Timer.h>
#include "WalletLegacy/WalletLegacy.h"
#include "WalletLegacyObserver.h"

using namespace Tests;
using namespace cn;

class WalletLegacyTests : public BaseTest
{
};

TEST_F(WalletLegacyTests, checkNetworkShutdown)
{
  auto networkCfg = TestNetworkBuilder(3, Topology::Star).setBlockchain("testnet_300").build();

  networkCfg[0].nodeType = NodeType::InProcess;
  network.addNodes(networkCfg);
  network.waitNodesReady();

  auto &daemon = network.getNode(0);

  {
    logging::ConsoleLogger m_logger;
    auto node = daemon.makeINode();
    WalletLegacy wallet(currency, *node, m_logger);
    wallet.initAndGenerate("pass");

    WalletLegacyObserver observer;
    wallet.addObserver(&observer);

    std::error_code syncResult;
    ASSERT_TRUE(observer.m_syncResult.waitFor(std::chrono::seconds(10), syncResult));
    ASSERT_TRUE(!syncResult);

    // sync completed
    auto syncProgress = observer.getSyncProgress();

    network.getNode(1).stopDaemon();
    network.getNode(2).stopDaemon();

    platform_system::Timer(dispatcher).sleep(std::chrono::seconds(10));

    // check that sync progress was not updated
    ASSERT_EQ(syncProgress, observer.getSyncProgress());
  }
}

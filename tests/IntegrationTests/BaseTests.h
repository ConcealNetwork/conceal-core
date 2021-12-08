// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <gtest/gtest.h>
#include <future>

#include <Logging/ConsoleLogger.h>
#include <System/Dispatcher.h>
#include "CryptoNoteCore/Currency.h"

#include "../IntegrationTestLib/TestNetwork.h"

namespace Tests {

class BaseTest : public testing::Test {
public:

  BaseTest() :
    currency(cn::CurrencyBuilder(logger).testnet(true).currency()),
    network(dispatcher, currency) {
  }

protected:

  virtual void TearDown() override {
    network.shutdown();
  }

  platform_system::Dispatcher& getDispatcher() {
    return dispatcher;
  }

  platform_system::Dispatcher dispatcher;
  logging::ConsoleLogger logger;
  cn::Currency currency;
  TestNetwork network;
};

}

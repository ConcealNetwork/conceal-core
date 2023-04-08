// Copyright (c) 2012-2017 The Cryptonote developers
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "../IntegrationTestLib/BaseFunctionalTests.h"
#include "../IntegrationTestLib/Logger.h"
#include "gtest/gtest.h"

extern platform_system::Dispatcher globalSystem;
extern cn::Currency currency;
extern Tests::common::BaseFunctionalTestsConfig config;

class TransfersTest :
  public Tests::common::BaseFunctionalTests,
  public ::testing::Test {

public:
  TransfersTest() : BaseFunctionalTests(currency, globalSystem, config) {
  }
};

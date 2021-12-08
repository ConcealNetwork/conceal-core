// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
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

// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest/gtest.h"
#include "Globals.h"

#include <Logging/ConsoleLogger.h>

logging::ConsoleLogger logger;
platform_system::Dispatcher globalSystem;
cn::Currency currency = cn::CurrencyBuilder(logger).testnet(true).currency();
Tests::common::BaseFunctionalTestsConfig config;


namespace po = boost::program_options;

int main(int argc, char** argv) {
  CLogger::Instance().init(CLogger::DEBUG);

  po::options_description desc;
  po::variables_map vm;
  
  config.init(desc);
  po::store(po::command_line_parser(argc, argv).options(desc).allow_unregistered().run(), vm);
  po::notify(vm);
  config.handleCommandLine(vm);

  try {

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();

  } catch (std::exception& ex) {
    LOG_ERROR("Fatal error: " + std::string(ex.what()));
    return 1;
  }
}

// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2015-2016 The Bytecoin developers
// Copyright (c) 2016-2017 The TurtleCoin developers
// Copyright (c) 2017-2018 krypt0x aka krypt0chaos
// Copyright (c) 2018 The Circle Foundation
//
// This file is part of Conceal Sense Crypto Engine.
//
// Conceal is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Conceal is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Conceal.  If not, see <http://www.gnu.org/licenses/>.

#include "gtest/gtest.h"
#include "Globals.h"

#include <Logging/ConsoleLogger.h>

Logging::ConsoleLogger logger;
System::Dispatcher globalSystem;
CryptoNote::Currency currency = CryptoNote::CurrencyBuilder(logger).testnet(true).currency();
Tests::Common::BaseFunctionalTestsConfig config;


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

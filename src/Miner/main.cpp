// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2021 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "Common/SignalHandler.h"

#include "Logging/LoggerGroup.h"
#include "Logging/ConsoleLogger.h"
#include "Logging/LoggerRef.h"

#include "MinerManager.h"

#include <System/Dispatcher.h>

int main(int argc, char** argv) {
  try {
    cn::MiningConfig config;
    config.parse(argc, argv);

    if (config.help) {
      config.printHelp();
      return 0;
    }

    logging::LoggerGroup loggerGroup;
    logging::ConsoleLogger consoleLogger(static_cast<logging::Level>(config.logLevel));
    loggerGroup.addLogger(consoleLogger);

    System::Dispatcher dispatcher;
    Miner::MinerManager app(dispatcher, config, loggerGroup);

    app.start();
  } catch (std::exception& e) {
    std::cerr << "Fatal: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}

// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2021 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "ConfigurationManager.h"
#include "PaymentServiceConfiguration.h"

#include "Logging/ConsoleLogger.h"
#include "Logging/LoggerGroup.h"
#include "Logging/StreamLogger.h"

#include "PaymentGate/NodeFactory.h"
#include "PaymentGate/WalletService.h"

class PaymentGateService {
public:

  PaymentGateService() : dispatcher(nullptr), stopEvent(nullptr), config(), service(nullptr), logger(), currencyBuilder(logger) {
  }

  bool init(int argc, char** argv);

  const payment_service::ConfigurationManager& getConfig() const { return config; }
  payment_service::WalletConfiguration getWalletConfig() const;
  const cn::Currency getCurrency();

  void run();
  void stop();
  
  logging::ILogger& getLogger() { return logger; }

private:

  void runInProcess(logging::LoggerRef& log);
  void runRpcProxy(logging::LoggerRef& log);

  void runWalletService(const cn::Currency& currency, cn::INode& node);

  platform_system::Dispatcher* dispatcher;
  platform_system::Event* stopEvent;
  payment_service::ConfigurationManager config;
  payment_service::WalletService* service;
  cn::CurrencyBuilder currencyBuilder;
  
  logging::LoggerGroup logger;
  std::ofstream fileStream;
  logging::StreamLogger fileLogger;
  logging::ConsoleLogger consoleLogger;
};

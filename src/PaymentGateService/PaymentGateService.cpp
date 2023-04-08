// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "PaymentGateService.h"

#include <future>

#include "Common/SignalHandler.h"
#include "InProcessNode/InProcessNode.h"
#include "Logging/LoggerRef.h"
#include "PaymentGate/PaymentServiceJsonRpcServer.h"

#include "CryptoNoteCore/CoreConfig.h"
#include "CryptoNoteCore/Core.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "P2p/NetNode.h"
#include "Rpc/RpcServer.h"
#include <System/Context.h>
#include "Wallet/WalletGreen.h"

#ifdef ERROR
#undef ERROR
#endif

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

using namespace payment_service;

void changeDirectory(const std::string& path) {
  if (chdir(path.c_str())) {
    throw std::runtime_error("Couldn't change directory to \'" + path + "\': " + strerror(errno));
  }
}

void stopSignalHandler(PaymentGateService* pg) {
  pg->stop();
}

bool PaymentGateService::init(int argc, char** argv) {
  if (!config.init(argc, argv)) {
    return false;
  }

  logger.setMaxLevel(static_cast<logging::Level>(config.gateConfiguration.logLevel));
  logger.addLogger(consoleLogger);

  logging::LoggerRef log(logger, "main");

  if (config.gateConfiguration.testnet) {
    log(logging::INFO, logging::MAGENTA) << "/!\\ Starting in testnet mode /!\\";
    currencyBuilder.testnet(true);
  }

  if (!config.gateConfiguration.serverRoot.empty()) {
    changeDirectory(config.gateConfiguration.serverRoot);
    log(logging::INFO) << "Current working directory now is " << config.gateConfiguration.serverRoot;
  }

  fileStream.open(config.gateConfiguration.logFile, std::ofstream::app);

  if (!fileStream) {
    throw std::runtime_error("Couldn't open log file");
  }

  fileLogger.attachToStream(fileStream);
  logger.addLogger(fileLogger);

  return true;
}

WalletConfiguration PaymentGateService::getWalletConfig() const {
  return WalletConfiguration{
    config.gateConfiguration.containerFile,
    config.gateConfiguration.containerPassword,
    config.gateConfiguration.secretSpendKey,
    config.gateConfiguration.secretViewKey
  };
}

const cn::Currency PaymentGateService::getCurrency() {
  return currencyBuilder.currency();
}

void PaymentGateService::run() {
  
  platform_system::Dispatcher localDispatcher;
  platform_system::Event localStopEvent(localDispatcher);

  this->dispatcher = &localDispatcher;
  this->stopEvent = &localStopEvent;

  tools::SignalHandler::install(std::bind(&stopSignalHandler, this));

  logging::LoggerRef log(logger, "run");

  if (config.startInprocess) {
    runInProcess(log);
  } else {
    runRpcProxy(log);
  }

  this->dispatcher = nullptr;
  this->stopEvent = nullptr;
}

void PaymentGateService::stop() {
  logging::LoggerRef log(logger, "stop");

  log(logging::INFO) << "Stop signal caught";

  if (dispatcher != nullptr) {
    dispatcher->remoteSpawn([this]() {
      if (stopEvent != nullptr) {
        stopEvent->set();
      }
    });
  }
}

void PaymentGateService::runInProcess(const logging::LoggerRef& log) {
  if (!config.coreConfig.configFolderDefaulted) {
    if (!tools::directoryExists(config.coreConfig.configFolder)) {
      throw std::runtime_error("Directory does not exist: " + config.coreConfig.configFolder);
    }
  } else {
    if (!tools::create_directories_if_necessary(config.coreConfig.configFolder)) {
      throw std::runtime_error("Can't create directory: " + config.coreConfig.configFolder);
    }
  }

  log(logging::INFO) << "Starting Payment Gate with local node";

  cn::Currency currency = currencyBuilder.currency();
  cn::core core(currency, nullptr, logger, false, false);

  cn::CryptoNoteProtocolHandler protocol(currency, *dispatcher, core, nullptr, logger);
  cn::NodeServer p2pNode(*dispatcher, protocol, logger);
  cn::RpcServer rpcServer(*dispatcher, logger, core, p2pNode, protocol);

  protocol.set_p2p_endpoint(&p2pNode);
  core.set_cryptonote_protocol(&protocol);

  log(logging::INFO) << "initializing p2pNode";
  if (!p2pNode.init(config.netNodeConfig)) {
    throw std::runtime_error("Failed to init p2pNode");
  }

  log(logging::INFO) << "initializing core";
  cn::MinerConfig emptyMiner;
  core.init(config.coreConfig, emptyMiner, true);

  std::promise<std::error_code> initPromise;
  auto initFuture = initPromise.get_future();

  std::unique_ptr<cn::INode> node(new cn::InProcessNode(core, protocol));

  node->init([&initPromise, &log](std::error_code ec) {
    if (ec) {
      log(logging::WARNING, logging::YELLOW) << "Failed to init node: " << ec.message();
    } else {
      log(logging::INFO) << "node is inited successfully";
    }

    initPromise.set_value(ec);
  });

  auto ec = initFuture.get();
  if (ec) {
    throw std::system_error(ec);
  }

  log(logging::INFO) << "Starting core rpc server on "
    << config.remoteNodeConfig.daemonHost << ":" << config.remoteNodeConfig.daemonPort;
  rpcServer.start(config.remoteNodeConfig.daemonHost, config.remoteNodeConfig.daemonPort);
  log(logging::INFO) << "Core rpc server started ok";

  log(logging::INFO) << "Spawning p2p server";

  platform_system::Event p2pStarted(*dispatcher);
  
  platform_system::Context<> context(*dispatcher, [&]() {
    p2pStarted.set();
    p2pNode.run();
  });

  p2pStarted.wait();

  runWalletService(currency, *node);

  log(logging::INFO) << "Stopping core rpc server...";
  rpcServer.stop();
  p2pNode.sendStopSignal();
  context.get();
  node->shutdown();
  core.deinit();
  p2pNode.deinit(); 
}

void PaymentGateService::runRpcProxy(const logging::LoggerRef& log) {
  log(logging::INFO) << "Starting Payment Gate with remote node";
  cn::Currency currency = currencyBuilder.currency();
  
  std::unique_ptr<cn::INode> node(
    payment_service::NodeFactory::createNode(
      config.remoteNodeConfig.daemonHost, 
      config.remoteNodeConfig.daemonPort));

  runWalletService(currency, *node);
}

void PaymentGateService::runWalletService(const cn::Currency& currency, cn::INode& node) {
  payment_service::WalletConfiguration walletConfiguration{
    config.gateConfiguration.containerFile,
    config.gateConfiguration.containerPassword
  };

  std::unique_ptr<cn::WalletGreen> wallet(new cn::WalletGreen(*dispatcher, currency, node, logger));

  service = new payment_service::WalletService(currency, *dispatcher, node, *wallet, *wallet, walletConfiguration, logger, config.gateConfiguration.testnet);
  std::unique_ptr<payment_service::WalletService> serviceGuard(service);
  try {
    service->init();
  } catch (std::exception& e) {
    logging::LoggerRef(logger, "run")(logging::ERROR, logging::BRIGHT_RED) << "Failed to init walletService reason: " << e.what();
    return;
  }

  if (config.gateConfiguration.printAddresses) {
    // print addresses and exit
    std::vector<std::string> addresses;
    service->getAddresses(addresses);
    for (const auto& address: addresses) {
      std::cout << "Address: " << address << std::endl;
    }
  } else {
    payment_service::PaymentServiceJsonRpcServer rpcServer(*dispatcher, *stopEvent, *service, logger);
    rpcServer.start(config.gateConfiguration.bindAddress, config.gateConfiguration.bindPort,
      config.gateConfiguration.rpcUser, config.gateConfiguration.rpcPassword);

    try {
      service->saveWallet();
    } catch (std::exception& ex) {
      logging::LoggerRef(logger, "saveWallet")(logging::WARNING, logging::YELLOW) << "Couldn't save container: " << ex.what();
    }
  }
}

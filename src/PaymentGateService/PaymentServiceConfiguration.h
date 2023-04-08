// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <stdexcept>
#include <cstdint>

#include <boost/program_options.hpp>

#include "Logging/ILogger.h"

namespace payment_service {

class ConfigurationError : public std::runtime_error {
public:
  using runtime_error::runtime_error;
};

struct Configuration {
  Configuration() = default;

  void init(const boost::program_options::variables_map& options);
  static void initOptions(boost::program_options::options_description& desc);

  std::string bindAddress;
  uint16_t bindPort = 0;
  std::string rpcUser;
  std::string rpcPassword;
  std::string secretSpendKey;
  std::string secretViewKey;  

  std::string containerFile;
  std::string containerPassword;
  std::string logFile = "payment_gate.log";
  std::string serverRoot;

  bool generateNewContainer = false;
  bool daemonize = false;
  bool registerService = false;
  bool unregisterService = false;
  bool testnet = false;
  bool printAddresses = false;

  size_t logLevel = logging::INFO;
};

} //namespace payment_service

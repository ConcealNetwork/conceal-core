// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "RpcNodeConfiguration.h"
#include "CryptoNoteConfig.h"

namespace payment_service {

namespace po = boost::program_options;

void RpcNodeConfiguration::initOptions(boost::program_options::options_description& desc) {
  desc.add_options()
    ("daemon-address", po::value<std::string>()->default_value("127.0.0.1"), "daemon address")
    ("daemon-port", po::value<uint16_t>()->default_value(cn::RPC_DEFAULT_PORT), "daemon port");
}

void RpcNodeConfiguration::init(const boost::program_options::variables_map& options) {
  if (options.count("daemon-address") != 0 && (!options["daemon-address"].defaulted() || daemonHost.empty())) {
    daemonHost = options["daemon-address"].as<std::string>();
  }

  if (options.count("daemon-port") != 0 && (!options["daemon-port"].defaulted() || daemonPort == 0)) {
    daemonPort = options["daemon-port"].as<uint16_t>();
  }

  bool testnet = options["testnet"].as<bool>();
  if (testnet)
  {
    daemonPort = cn::TESTNET_RPC_DEFAULT_PORT;
    if (!options["daemon-port"].defaulted())
    {
      daemonPort = options["daemon-port"].as<uint16_t>();
    }
  }
}

} //namespace payment_service

// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <string>

namespace CryptoNote {

struct MiningConfig {
  MiningConfig();

  void parse(int argc, char** argv);
  void printHelp();

  std::string miningAddress;
  std::string daemonHost;
  uint16_t daemonPort;
  uint64_t threadCount;
  uint64_t scanPeriod;
  uint8_t logLevel;
  uint64_t blocksLimit;
  uint64_t firstBlockTimestamp;
  int64_t blockTimestampInterval;
  bool help;
};

} //namespace CryptoNote

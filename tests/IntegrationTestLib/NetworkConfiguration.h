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

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Tests {

enum class NodeType {
  RPC,
  InProcess
};

struct TestNodeConfiguration {
  NodeType nodeType = NodeType::RPC;
  bool testnet = true;
  uint16_t p2pPort;
  uint16_t rpcPort;

  std::string dataDir;
  std::string blockchainLocation; // copy blockchain from this path
  std::string logFile;
  std::string daemonPath; // only for rpc node
  bool cleanupDataDir = true;

  std::vector<std::string> exclusiveNodes;

  std::string getP2pAddress() const {
    return "127.0.0.1:" + std::to_string(p2pPort);
  }
};

}

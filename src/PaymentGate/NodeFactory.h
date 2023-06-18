// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "INode.h"

#include <string>

namespace payment_service {

class NodeFactory {
public:
  static cn::INode* createNode(const std::string& daemonAddress, uint16_t daemonPort);
  static cn::INode* createNodeStub();
private:
  NodeFactory();
  ~NodeFactory();

  cn::INode* getNode(const std::string& daemonAddress, uint16_t daemonPort);

  static NodeFactory factory;
};

} //namespace payment_service

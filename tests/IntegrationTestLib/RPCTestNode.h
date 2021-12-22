// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stdint.h>
#include <System/Dispatcher.h>
#include "HTTP/HttpRequest.h"
#include "HTTP/HttpResponse.h"
#include "Rpc/HttpClient.h"

#include "TestNode.h"

using namespace cn;

namespace Tests {
  class RPCTestNode : public TestNode {
  public:
    RPCTestNode(uint16_t port, platform_system::Dispatcher& d);

    virtual bool startMining(size_t threadsCount, const std::string& address) override;
    virtual bool stopMining() override;
    virtual bool stopDaemon() override;
    virtual bool getBlockTemplate(const std::string& minerAddress, cn::Block& blockTemplate, uint64_t& difficulty) override;
    virtual bool submitBlock(const std::string& block) override;
    virtual bool getTailBlockId(crypto::Hash& tailBlockId) override;
    virtual bool makeINode(std::unique_ptr<cn::INode>& node) override;
    virtual uint64_t getLocalHeight() override;

    virtual ~RPCTestNode() { }

  private:

    uint16_t m_rpcPort;
    platform_system::Dispatcher& m_dispatcher;
    cn::HttpClient m_httpClient;
  };
}

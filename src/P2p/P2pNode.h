// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <functional>
#include <list>

#include <Logging/LoggerRef.h>
#include <System/ContextGroup.h>
#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/TcpListener.h>
#include <System/Timer.h>

#include "IP2pNodeInternal.h"
#include "IStreamSerializable.h"
#include "NetNodeConfig.h"

#include <P2p/P2pInterfaces.h>
#include <P2p/P2pNodeConfig.h>
#include <P2p/P2pProtocolDefinitions.h>
#include <P2p/PeerListManager.h>
#include <P2p/Peerlist.h>

namespace CryptoNote {

class P2pContext;
class P2pConnectionProxy;

class P2pNode : 
  public IP2pNode, 
  public IStreamSerializable,
  IP2pNodeInternal {

public:

  P2pNode(
    const P2pNodeConfig& cfg,
    System::Dispatcher& dispatcher, 
    Logging::ILogger& log, 
    const Crypto::Hash& genesisHash, 
    uint64_t peerId);

  ~P2pNode();
  
  // IP2pNode
  virtual std::unique_ptr<IP2pConnection> receiveConnection() override;
  virtual void stop() override;

  // IStreamSerializable
  virtual void save(std::ostream& os) override;
  virtual void load(std::istream& in) override;

  // P2pNode
  void start();
  void serialize(ISerializer& s);

private:
  typedef std::unique_ptr<P2pContext> ContextPtr;
  typedef std::list<ContextPtr> ContextList;

  Logging::LoggerRef logger;
  bool m_stopRequested;
  const P2pNodeConfig m_cfg;
  const uint64_t m_myPeerId;
  const CORE_SYNC_DATA m_genesisPayload;

  System::Dispatcher& m_dispatcher;
  System::ContextGroup workingContextGroup;
  System::TcpListener m_listener;
  System::Timer m_connectorTimer;
  PeerlistManager m_peerlist;
  ContextList m_contexts;
  System::Event m_queueEvent;
  std::deque<std::unique_ptr<IP2pConnection>> m_connectionQueue;

  // IP2pNodeInternal
  virtual const CORE_SYNC_DATA& getGenesisPayload() const override;
  virtual std::list<PeerlistEntry> getLocalPeerList() override;
  virtual basic_node_data getNodeData() const override;
  virtual uint64_t getPeerId() const override;

  virtual void handleNodeData(const basic_node_data& node, P2pContext& ctx) override;
  virtual bool handleRemotePeerList(const std::list<PeerlistEntry>& peerlist, time_t local_time) override;
  virtual void tryPing(P2pContext& ctx) override;

  // spawns
  void acceptLoop();
  void connectorLoop();

  // connection related
  void connectPeers();
  void connectPeerList(const std::vector<NetworkAddress>& peers);
  bool isPeerConnected(const NetworkAddress& address);
  bool isPeerUsed(const PeerlistEntry& peer);
  ContextPtr tryToConnectPeer(const NetworkAddress& address);
  bool fetchPeerList(ContextPtr connection);

  // making and processing connections
  uint64_t getOutgoingConnectionsCount() const;
  void makeExpectedConnectionsCount(const Peerlist& peerlist, uint64_t connectionsCount);
  bool makeNewConnectionFromPeerlist(const Peerlist& peerlist);
  void preprocessIncomingConnection(ContextPtr ctx);
  void enqueueConnection(std::unique_ptr<P2pConnectionProxy> proxy);
  std::unique_ptr<P2pConnectionProxy> createProxy(ContextPtr ctx);
};

}

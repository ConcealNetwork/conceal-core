// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "NetNodeConfig.h"

#include <chrono>
#include <boost/uuid/uuid.hpp>

namespace CryptoNote {

class P2pNodeConfig : public NetNodeConfig {
public:
  P2pNodeConfig();

  // getters
  std::chrono::nanoseconds getTimedSyncInterval() const;
  std::chrono::nanoseconds getHandshakeTimeout() const;
  std::chrono::nanoseconds getConnectInterval() const;
  std::chrono::nanoseconds getConnectTimeout() const;
  uint64_t getExpectedOutgoingConnectionsCount() const;
  uint64_t getWhiteListConnectionsPercent() const;
  boost::uuids::uuid getNetworkId() const;
  uint64_t getPeerListConnectRange() const;
  uint64_t getPeerListGetTryCount() const;

  // setters
  void setTimedSyncInterval(std::chrono::nanoseconds interval);
  void setHandshakeTimeout(std::chrono::nanoseconds timeout);
  void setConnectInterval(std::chrono::nanoseconds interval);
  void setConnectTimeout(std::chrono::nanoseconds timeout);
  void setExpectedOutgoingConnectionsCount(uint64_t count);
  void setWhiteListConnectionsPercent(uint64_t percent);
  void setNetworkId(const boost::uuids::uuid& id);
  void setPeerListConnectRange(uint64_t range);
  void setPeerListGetTryCount(uint64_t count);

private:
  std::chrono::nanoseconds timedSyncInterval;
  std::chrono::nanoseconds handshakeTimeout;
  std::chrono::nanoseconds connectInterval;
  std::chrono::nanoseconds connectTimeout;
  boost::uuids::uuid networkId;
  uint64_t expectedOutgoingConnectionsCount;
  uint64_t whiteListConnectionsPercent;
  uint64_t peerListConnectRange;
  uint64_t peerListGetTryCount;
};

}

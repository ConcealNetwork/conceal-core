// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2021 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <vector>

#include <CryptoNote.h>

namespace cn {

class NewBlockMessage {
public:
  NewBlockMessage(const crypto::Hash& hash);
  NewBlockMessage() = default;
  void get(crypto::Hash& hash) const;
private:
  crypto::Hash blockHash;
};

class NewAlternativeBlockMessage {
public:
  NewAlternativeBlockMessage(const crypto::Hash& hash);
  NewAlternativeBlockMessage() = default;
  void get(crypto::Hash& hash) const;
private:
  crypto::Hash blockHash;
};

class ChainSwitchMessage {
public:
  ChainSwitchMessage(std::vector<crypto::Hash>&& hashes);
  ChainSwitchMessage(const ChainSwitchMessage& other);
  void get(std::vector<crypto::Hash>& hashes) const;
private:
  std::vector<crypto::Hash> blocksFromCommonRoot;
};

class BlockchainMessage {
public:
  enum class MessageType {
    NEW_BLOCK_MESSAGE,
    NEW_ALTERNATIVE_BLOCK_MESSAGE,
    CHAIN_SWITCH_MESSAGE
  };

  BlockchainMessage(NewBlockMessage&& message);
  BlockchainMessage(NewAlternativeBlockMessage&& message);
  BlockchainMessage(ChainSwitchMessage&& message);

  BlockchainMessage(const BlockchainMessage& other);

  ~BlockchainMessage();

  MessageType getType() const;

  bool getNewBlockHash(crypto::Hash& hash) const;
  bool getNewAlternativeBlockHash(crypto::Hash& hash) const;
  bool getChainSwitch(std::vector<crypto::Hash>& hashes) const;
private:
  const MessageType type;

  union {
    NewBlockMessage newBlockMessage;
    NewAlternativeBlockMessage newAlternativeBlockMessage;
    ChainSwitchMessage* chainSwitchMessage;
  };
};

}

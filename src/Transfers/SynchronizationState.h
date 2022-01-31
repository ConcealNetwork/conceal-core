// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2022 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "CommonTypes.h"
#include "IStreamSerializable.h"
#include "Serialization/ISerializer.h"
#include <vector>
#include <map>

namespace cn {

class SynchronizationState : public IStreamSerializable {
public:

  struct CheckResult {
    bool detachRequired;
    uint32_t detachHeight;
    bool hasNewBlocks;
    uint32_t newBlockHeight;
  };

  typedef std::vector<crypto::Hash> ShortHistory;

  explicit SynchronizationState(const crypto::Hash& genesisBlockHash) {
    m_blockchain.push_back(genesisBlockHash);
  }

  ShortHistory getShortHistory(uint32_t localHeight) const;
  CheckResult checkInterval(const BlockchainInterval& interval) const;

  void detach(uint32_t height);
  void addBlocks(const crypto::Hash* blockHashes, uint32_t height, uint32_t count);
  uint32_t getHeight() const;
  const std::vector<crypto::Hash>& getKnownBlockHashes() const;

  // IStreamSerializable
  virtual void save(std::ostream& os) override;
  virtual void load(std::istream& in) override;

  // serialization
  cn::ISerializer& serialize(cn::ISerializer& s, const std::string& name);

private:

  std::vector<crypto::Hash> m_blockchain;
};

}

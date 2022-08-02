// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2022 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "BlockIndex.h"

#include <boost/utility/value_init.hpp>

#include "CryptoNoteSerialization.h"
#include "Serialization/SerializationOverloads.h"

namespace cn {
  crypto::Hash BlockIndex::getBlockId(uint32_t height) const {
    assert(height < m_container.size());

    return m_container[static_cast<size_t>(height)];
  }

  std::vector<crypto::Hash> BlockIndex::getBlockIds(uint32_t startBlockIndex, uint32_t maxCount) const {
    std::vector<crypto::Hash> result;
    if (startBlockIndex >= m_container.size()) {
      return result;
    }

    size_t count = std::min(static_cast<size_t>(maxCount), m_container.size() - static_cast<size_t>(startBlockIndex));
    result.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      result.push_back(m_container[startBlockIndex + i]);
    }

    return result;
  }

  bool BlockIndex::findSupplement(const std::vector<crypto::Hash>& ids, uint32_t& offset) const {
    for (const auto& id : ids) {
      if (getBlockHeight(id, offset)) {
        return true;
      }
    }

    return false;
  }

  std::vector<crypto::Hash> BlockIndex::buildSparseChain(const crypto::Hash &startBlockId) const
  {
    assert(m_index.count(startBlockId) > 0);

    uint32_t startBlockHeight = 0;
    std::vector<crypto::Hash> result;
    if (getBlockHeight(startBlockId, startBlockHeight))
    {
      size_t sparseChainEnd = static_cast<size_t>(startBlockHeight + 1);
      for (size_t i = 1; i <= sparseChainEnd; i *= 2)
      {
        result.emplace_back(m_container[sparseChainEnd - i]);
      }
    }

    if (result.back() != m_container[0])
    {
      result.emplace_back(m_container[0]);
    }

    return result;
  }

  crypto::Hash BlockIndex::getTailId() const {
    assert(!m_container.empty());
    return m_container.back();
  }

  void BlockIndex::serialize(ISerializer& s) {
    if (s.type() == ISerializer::INPUT) {
      readSequence<crypto::Hash>(std::back_inserter(m_container), "index", s);
    } else {
      writeSequence<crypto::Hash>(m_container.begin(), m_container.end(), "index", s);
    }
  }
}

// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/random_access_index.hpp>

#include "crypto/hash.h"
#include <vector>

namespace cn
{
  class ISerializer;

  class BlockIndex {

  public:

    BlockIndex() : 
      m_index(m_container.get<1>()) {}

    void pop() {
      m_container.pop_back();
    }

    // returns true if new element was inserted, false if already exists
    bool push(const crypto::Hash& h) {
      auto result = m_container.push_back(h);
      return result.second;
    }

    bool hasBlock(const crypto::Hash& h) const {
      return m_index.find(h) != m_index.end();
    }

    bool getBlockHeight(const crypto::Hash& h, uint32_t& height) const {
      auto hi = m_index.find(h);
      if (hi == m_index.end())
        return false;

      height = static_cast<uint32_t>(std::distance(m_container.begin(), m_container.project<0>(hi)));
      return true;
    }

    uint32_t size() const {
      return static_cast<uint32_t>(m_container.size());
    }

    void clear() {
      m_container.clear();
    }

    crypto::Hash getBlockId(uint32_t height) const;
    std::vector<crypto::Hash> getBlockIds(uint32_t startBlockIndex, uint32_t maxCount) const;
    bool findSupplement(const std::vector<crypto::Hash>& ids, uint32_t& offset) const;
    std::vector<crypto::Hash> buildSparseChain(const crypto::Hash& startBlockId) const;
    crypto::Hash getTailId() const;

    void serialize(ISerializer& s);

  private:

    typedef boost::multi_index_container <
      crypto::Hash,
      boost::multi_index::indexed_by<
        boost::multi_index::random_access<>,
        boost::multi_index::hashed_unique<boost::multi_index::identity<crypto::Hash>>
      >
    > ContainerT;

    ContainerT m_container;
    ContainerT::nth_index<1>::type& m_index;

  };
}

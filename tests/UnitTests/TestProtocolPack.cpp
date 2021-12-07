// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/lexical_cast.hpp>

#include "gtest/gtest.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolDefinitions.h"
#include "Serialization/SerializationTools.h"

TEST(protocol_pack, protocol_pack_command) 
{
  std::string buff;
  cn::NOTIFY_RESPONSE_CHAIN_ENTRY::request r;
  r.start_height = 1;
  r.total_height = 3;
  for(int i = 1; i < 10000; i += i*10) {
    r.m_block_ids.resize(i, cn::NULL_HASH);
    buff = cn::storeToBinaryKeyValue(r);

    cn::NOTIFY_RESPONSE_CHAIN_ENTRY::request r2;
    cn::loadFromBinaryKeyValue(r2, buff);
    ASSERT_TRUE(r.m_block_ids.size() == i);
    ASSERT_TRUE(r.start_height == 1);
    ASSERT_TRUE(r.total_height == 3);
  }
}

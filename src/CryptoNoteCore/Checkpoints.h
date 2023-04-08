// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once
#include <map>
#include "CryptoNoteBasicImpl.h"
#include <Logging/LoggerRef.h>

namespace cn
{
  class Checkpoints
  {
  public:
    explicit Checkpoints(logging::ILogger& log);

    bool add_checkpoint(uint32_t height, const std::string& hash_str);
    bool is_in_checkpoint_zone(uint32_t height) const;
    bool load_checkpoints_from_file(const std::string& fileName);
    bool load_checkpoints_from_dns();
    bool load_checkpoints();    
    bool check_block(uint32_t height, const crypto::Hash& h) const;
    bool check_block(uint32_t height, const crypto::Hash& h, bool& is_a_checkpoint) const;
    bool is_alternative_block_allowed(uint32_t blockchain_height, uint32_t block_height) const;
    std::vector<uint32_t> getCheckpointHeights() const;
    void set_testnet(bool testnet);
    
  private:
    bool m_testnet = false;
    std::map<uint32_t, crypto::Hash> m_points;
    logging::LoggerRef logger;
  };
}

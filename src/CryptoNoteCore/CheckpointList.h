// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once
#include <vector>
#include <unordered_set>

#include "CryptoNoteBasicImpl.h"
#include <Logging/LoggerRef.h>

namespace cn
{
  class CheckpointList
  {
  public:
    explicit CheckpointList(logging::ILogger& log) :  logger(log, "checkpoint_list") {}

    void init_targets(bool is_testnet, const std::string& save_file);
  
    bool add_checkpoint_list(const std::vector<crypto::Hash>& points);
    bool set_checkpoint_list(std::vector<crypto::Hash>&& points);
    bool load_checkpoints_from_file();

    uint32_t get_points_height() const 
    {
      return m_points.size()-1;
    }

    uint32_t get_greatest_target_height() const
    {
      return m_targets.rbegin()->first - 1;
    }

    bool is_ready() const
    { 
      return m_points.size()-1 >= get_greatest_target_height(); 
    }

    bool is_in_checkpoint_zone(uint32_t height) const 
    {
      return m_points.size() < height;
    }

    enum check_rt
    {
      is_out_of_zone,
      is_in_zone_failed,
      is_checkpointed
    };

    check_rt check_checkpoint(uint32_t height, const crypto::Hash& hv) const 
    {
      if(m_points.size() >= height)
        return is_out_of_zone;
      if(m_points[height] == hv)
        return is_checkpointed;
      else
        return is_in_zone_failed;
    }
    
    std::vector<std::pair<uint32_t, crypto::Hash>> get_checkpoint_targets() const
    {
      std::vector<std::pair<uint32_t, crypto::Hash>> rv;
      rv.reserve(m_targets.size());
      for(auto it = m_targets.rbegin(); it != m_targets.rend(); ++it)
        rv.emplace_back(it->first-1, it->second);
      return rv;
    }

  private:
    bool m_testnet;
    logging::LoggerRef logger;
    std::string m_save_file;

    std::map<uint32_t, crypto::Hash> m_targets; /*NB uint32_t is size not height */
    std::unordered_set<uint32_t> m_valid_point_sizes;
    std::vector<crypto::Hash> m_points;

    bool save_checkpoints();
    bool add_checkpoint_target(uint32_t height, const std::string &hash_str);
    bool is_fsize_valid(uint32_t fsize)
    {
      if(fsize % sizeof(crypto::Hash) != 0)
        return false;
      fsize /= sizeof(crypto::Hash);
      
      return m_valid_point_sizes.find(fsize) != m_valid_point_sizes.end();
    }
  };
}

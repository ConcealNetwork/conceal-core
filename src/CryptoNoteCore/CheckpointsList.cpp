// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Copyright (c) 2016-2019, The Karbo developers

// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <cstdlib>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <cstring>
#include <string>
#include <string.h>
#include <sstream>
#include <vector>
#include <iterator>

#include "CheckpointList.h"
#include "../CryptoNoteConfig.h"
#include "Common/StringTools.h"
#include "Common/DnsTools.h"
#include "crypto/hash.h"

using namespace logging;

namespace cn {
  void CheckpointList::init_targets(bool is_testnet, const std::string& save_file)
  {
    m_testnet = is_testnet;
    m_save_file = save_file;

    for (const auto &cp : m_testnet ? cn::TESTNET_CHECKPOINTS : cn::CHECKPOINTS)
    {
      add_checkpoint_target(cp.height, cp.blockId);
    }

    const char* domain;
    if (m_testnet)
      domain = TESTNET_DNS_CHECKPOINT_DOMAIN;
    else
      domain = DNS_CHECKPOINT_DOMAIN;

    std::vector<std::string>records;

    logger(logging::DEBUGGING) << "<< CheckpointList.cpp << " << "Fetching DNS checkpoint records from " << domain;

    if (!common::fetch_dns_txt(domain, records)) {
      logger(logging::DEBUGGING) << "<< CheckpointList.cpp << " << "Failed to lookup DNS checkpoint records from " << domain;
    }

    for (const auto& record : records) {
      uint32_t height;
      crypto::Hash hash = NULL_HASH;
      std::stringstream ss;
      size_t del = record.find_first_of(':');
      std::string height_str = record.substr(0, del), hash_str = record.substr(del + 1, 64);
      ss.str(height_str);
      ss >> height;
      char c;
      if (del == std::string::npos) continue;
      if ((ss.fail() || ss.get(c)) || !common::podFromHex(hash_str, hash)) {
        logger(logging::INFO) << "<< CheckpointList.cpp << " << "Failed to parse DNS checkpoint record: " << record;
        continue;
      }

      if (!(0 == m_targets.count(height))) {
        logger(DEBUGGING) << "<< CheckpointList.cpp << " << "Checkpoint already exists for height: " << height << ". Ignoring DNS checkpoint.";
      } else {
        add_checkpoint_target(height, hash_str);
        logger(DEBUGGING) << "<< CheckpointList.cpp << " << "Added DNS checkpoint target: " << height_str << ":" << hash_str;
      }
    }
  }

  bool CheckpointList::add_checkpoint_target(uint32_t height, const std::string &hash_str) {
    crypto::Hash h = NULL_HASH;

    if (!common::podFromHex(hash_str, h)) {
      logger(ERROR) << "<< Checkpoints.cpp << " << "Incorrect hash in checkpoints";
      return false;
    }

    if (!(0 == m_targets.count(height))) {
      logger(DEBUGGING) << "Checkpoint already exists for height " << height;
      return false;
    }
    
    height += 1;
    m_targets[height] = h;
    m_valid_point_sizes.insert(height);

    return true;
  }

  bool CheckpointList::set_checkpoint_list(std::vector<crypto::Hash>&& points) 
  {
    const std::lock_guard<std::mutex> lock(m_points_lock);
    uint32_t point_size = points.size();
    if(m_valid_point_sizes.find(point_size) == m_valid_point_sizes.end())
      return false;

    crypto::Hash hv = crypto::cn_fast_hash(points.data(), points.size() * sizeof(crypto::Hash));
    if(hv != m_targets[point_size])
    {
      logger(logging::ERROR) << "<< CheckpointList.cpp << " << "CheckpointList verification failed for height " << point_size-1 << 
                         ". Expected hash: " << m_targets[point_size]  << 
                         ", Fetched hash: " << hv;
      return false;
    }
    
    m_points = std::move(points);
    logger(logging::INFO) << "Loaded " << m_points.size() << " checkpoints from local index";
    
    save_checkpoints();
    return true;
  }

  bool CheckpointList::add_checkpoint_list(uint32_t start_height, std::vector<crypto::Hash>& points) 
  {
    const std::lock_guard<std::mutex> lock(m_points_lock);

    if(m_points.size() != start_height)
      return true;

    uint32_t point_size = points.size() + m_points.size();
    if(m_valid_point_sizes.find(point_size) == m_valid_point_sizes.end())
      return false;

    /* This copy wouldn't be needed with three funciton hash */
    std::vector<crypto::Hash> new_points(m_points);
    new_points.insert(std::end(new_points), std::begin(points), std::end(points));

    crypto::Hash hv = crypto::cn_fast_hash(new_points.data(), new_points.size() * sizeof(crypto::Hash));
    if(hv != m_targets[point_size])
    {
      logger(logging::ERROR) << "<< CheckpointList.cpp << " << "CheckpointList verification failed for height " << point_size-1 << 
                         ". Expected hash: " << m_targets[point_size]  << 
                         ", Fetched hash: " << hv;
      return false;
    }
    
    m_points = std::move(new_points);
    logger(logging::INFO) << "Loaded " << points.size() << " checkpoints from p2p, total " << m_points.size();
    
    save_checkpoints();
    return true;
  }

  bool CheckpointList::load_checkpoints_from_file()
  {
    std::ifstream file(m_save_file, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
      return false;
    }

    uint64_t fsize = file.tellg();
    if(!is_fsize_valid(fsize)) {
      logger(logging::ERROR) << "<< CheckpointList.cpp << " << "Invalid file size" << fsize;
      return false;
    }
    uint32_t point_size = fsize / sizeof(crypto::Hash);
    
    file.seekg(0, std::ios::beg);

    std::vector<crypto::Hash> points(point_size);
    if (!file.read(reinterpret_cast<char*>(points.data()), fsize)) {
      logger(logging::ERROR) << "<< CheckpointList.cpp << " << "error reading file";
      return false;
    }

    crypto::Hash hv = crypto::cn_fast_hash(points.data(), points.size() * sizeof(crypto::Hash));
    if(hv != m_targets[point_size])
    {
      logger(logging::ERROR) << "<< CheckpointList.cpp << " << "CheckpointList verification (from file) failed for height " << point_size-1
                          << ". Expected hash: " << m_targets[point_size]
                          << ", Fetched hash: " << hv;
      return false;
    }

    const std::lock_guard<std::mutex> lock(m_points_lock);
    m_points = std::move(points);
    logger(logging::INFO) << "Loaded " << m_points.size() << " checkpoints from disk " << m_save_file;
    return true;
  }
  
  bool CheckpointList::save_checkpoints()
  {
    std::ofstream file(m_save_file, std::ios::binary);

    if (!file.is_open()) {
      logger(logging::ERROR) << "<< CheckpointList.cpp << " << "error opening file for write " << m_save_file;
      return false;
    }

    file.write(reinterpret_cast<const char*>(m_points.data()), m_points.size() * sizeof(crypto::Hash));
    file.close();

    if (!file) {
      logger(logging::ERROR) << "<< CheckpointList.cpp << " << "error writing to file " << m_save_file;
      return false;
    }

    return true;
  }
}

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

#include "Checkpoints.h"
#include "../CryptoNoteConfig.h"
#include "Common/StringTools.h"
#include "Common/DnsTools.h"

using namespace logging;

namespace cn {
//---------------------------------------------------------------------------
Checkpoints::Checkpoints(logging::ILogger &log) : logger(log, "checkpoints") {}
//---------------------------------------------------------------------------
bool Checkpoints::add_checkpoint(uint32_t height, const std::string &hash_str) {
  crypto::Hash h = NULL_HASH;

  if (!common::podFromHex(hash_str, h)) {
    logger(ERROR) << "<< Checkpoints.cpp << " << "Incorrect hash in checkpoints";
    return false;
  }

  if (!(0 == m_points.count(height))) {
    logger(DEBUGGING) << "Checkpoint already exists for height " << height;
    return false;
  }
  
  m_points[height] = h;

  return true;
}
//---------------------------------------------------------------------------
bool Checkpoints::is_in_checkpoint_zone(uint32_t  height) const {
  return !m_points.empty() && (height <= (--m_points.end())->first);
}
//---------------------------------------------------------------------------
bool Checkpoints::check_block(uint32_t  height, const crypto::Hash &h, bool &is_a_checkpoint) const {
  auto it = m_points.find(height);
  is_a_checkpoint = it != m_points.end();
  if (!is_a_checkpoint)
    return true;

  if (it->second == h) {    
    return true;
  } else {
    logger(logging::ERROR) << "<< Checkpoints.cpp << " << "Checkpoint failed for height " << height
                           << ". Expected hash: " << it->second
                           << ", Fetched hash: " << h;
    return false;
  }
}
//---------------------------------------------------------------------------
bool Checkpoints::check_block(uint32_t  height, const crypto::Hash &h) const {
  bool ignored;
  return check_block(height, h, ignored);
}
//---------------------------------------------------------------------------
bool Checkpoints::is_alternative_block_allowed(uint32_t  blockchain_height, uint32_t  block_height) const {
  if (0 == block_height)
    return false;

  uint32_t lowest_height = blockchain_height - cn::parameters::CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW;

  if (blockchain_height < cn::parameters::CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW)
  {
    lowest_height = 0;
  }

  if (block_height < lowest_height && !is_in_checkpoint_zone(block_height))
  {
    logger(logging::DEBUGGING, logging::WHITE)
        << "<< Checkpoints.cpp << "
        << "Reorganization depth too deep : " << (blockchain_height - block_height) << ". Block Rejected";
    return false;
  }

  auto it = m_points.upper_bound(blockchain_height);
  if (it == m_points.begin())
    return true;

  --it;
  uint32_t  checkpoint_height = it->first;
  return checkpoint_height < block_height;
}

//---------------------------------------------------------------------------

std::vector<uint32_t> Checkpoints::getCheckpointHeights() const {
  std::vector<uint32_t> checkpointHeights;
  checkpointHeights.reserve(m_points.size());
  for (const auto& it : m_points) {
    checkpointHeights.push_back(it.first);
  }

  return checkpointHeights;
}

bool Checkpoints::load_checkpoints_from_dns()
{
  std::string domain("checkpoints.conceal.id");
  if (m_testnet)
  {
    domain = "testpoints.conceal.gq";
  }
  std::vector<std::string>records;

  logger(logging::DEBUGGING) << "<< Checkpoints.cpp << " << "Fetching DNS checkpoint records from " << domain;

  if (!common::fetch_dns_txt(domain, records)) {
    logger(logging::DEBUGGING) << "<< Checkpoints.cpp << " << "Failed to lookup DNS checkpoint records from " << domain;
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
      logger(logging::INFO) << "<< Checkpoints.cpp << " << "Failed to parse DNS checkpoint record: " << record;
      continue;
    }

    if (!(0 == m_points.count(height))) {
      logger(DEBUGGING) << "<< Checkpoints.cpp << " << "Checkpoint already exists for height: " << height << ". Ignoring DNS checkpoint.";
    } else {
      add_checkpoint(height, hash_str);
	  logger(DEBUGGING) << "<< Checkpoints.cpp << " << "Added DNS checkpoint: " << height_str << ":" << hash_str;
    }
  }

  return true;
}

bool Checkpoints::load_checkpoints()
{
  if (m_testnet)
  {
    for (const auto &cp : cn::TESTNET_CHECKPOINTS)
    {
      add_checkpoint(cp.height, cp.blockId);
    }
  }
  else
  {
    for (const auto &cp : cn::CHECKPOINTS)
    {
      add_checkpoint(cp.height, cp.blockId);
    }
  }
  return true;
}

bool Checkpoints::load_checkpoints_from_file(const std::string& fileName) {
	std::ifstream file(fileName);
	if (!file) {
		logger(logging::ERROR, BRIGHT_RED) << "Could not load checkpoints file: " << fileName;
		return false;
	}
	std::string indexString;
	std::string hash;
	uint32_t height;
	while (std::getline(file, indexString, ','), std::getline(file, hash)) {
		try {
			height = std::stoi(indexString);
		} catch (const std::invalid_argument &)	{
			logger(ERROR, BRIGHT_RED) << "Invalid checkpoint file format - "
				<< "could not parse height as a number";
			return false;
		}
		if (!add_checkpoint(height, hash)) {
			return false;
		}
	}
	logger(logging::INFO) << "Loaded " << m_points.size() << " checkpoints from "	<< fileName;
	return true;
}

void Checkpoints::set_testnet(bool testnet) { m_testnet = testnet; }

}

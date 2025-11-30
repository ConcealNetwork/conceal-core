// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Copyright (c) 2016-2019, The Karbo developers

// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * CHECKPOINT CHUNKING IMPLEMENTATION
 * ===================================
 * 
 * PROBLEM SOLVED:
 * - Old approach: Store all 1.87M block hashes (60MB file, expensive to hash/compare)
 * - New approach: Store chunk hashes (187 chunks × 32 bytes = ~6KB file)
 * 
 * HOW CHUNKING WORKS:
 * 1. Blocks are grouped into chunks (10,000 blocks per chunk for mainnet, 25,000 for testnet)
 * 2. Each chunk hash = hash(block_id_0 || block_id_1 || ... || block_id_N)
 * 3. checkpoint.dat stores: [chunk0_hash, chunk1_hash, ..., chunk186_hash]
 * 
 * BENEFITS:
 * 1. Fast P2P Comparison:
 *    - Compare 187 chunk hashes (6KB) instead of hashing 60MB
 *    - Binary search through chunks to find disagreement (log2(187) ≈ 8 comparisons)
 * 
 * 2. Efficient Disagreement Detection:
 *    - Node A: [chunk0, chunk1, ..., chunk186]
 *    - Node B: [chunk0, chunk1, ..., chunk186]
 *    - Compare sequentially: chunk0 matches? chunk1 matches? ...
 *    - First mismatch → problem is in that chunk's 10,000-block range
 *    - Request specific chunk's block IDs to find exact block
 * 
 * 3. Small File Size:
 *    - 6KB vs 60MB (10,000x smaller)
 *    - Fast to load/save
 * 
 * 4. Efficient Rollback:
 *    - Know exactly which chunk (10,000-block range) has the problem
 *    - Rollback to chunk boundary, then sync from there
 * 
 * P2P PROTOCOL:
 * 1. Nodes exchange chunk hashes first (fast comparison)
 * 2. If chunks match → checkpoints are in sync
 * 3. If chunks differ → find first mismatched chunk
 * 4. Request block IDs for that specific chunk
 * 5. Compare block IDs to find exact disagreement point
 * 6. Rollback to last matching chunk boundary
 * 
 * FILE FORMAT:
 * - Binary file: [chunk0_hash][chunk1_hash]...[chunkN_hash]
 * - Each hash is 32 bytes (crypto::Hash)
 * - File size = num_chunks × 32 bytes
 * 
 * VALIDATION:
 * - Chunks are validated during generation against hardcoded checkpoints
 * - Individual block validation uses chunk integrity + blockchain validation
 * - Checkpoint heights (from CryptoNoteConfig.h) are validated against targets
 */

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
#include <unordered_set>
#include <algorithm>
#include <cmath>
#include <random>


#include "CheckpointList.h"
#include "../CryptoNoteConfig.h"
#include "Common/StringTools.h"
#include "Common/DnsTools.h"
#include "crypto/hash.h"
#include <ctime>

using namespace logging;

namespace cn {
  void CheckpointList::init_targets(bool is_testnet, const std::string& save_file)
  {
    m_testnet = is_testnet;
    m_save_file = save_file;

    // Set chunk size based on network type
    // Mainnet: 10,000 blocks per chunk (matches checkpoint frequency)
    // Testnet: 25,000 blocks per chunk (matches checkpoint frequency)
    m_chunk_size = is_testnet ? CHUNK_SIZE_TESTNET : CHUNK_SIZE_MAINNET;
    
    logger(logging::INFO) << "Initializing checkpoint targets with chunk size: " 
                          << m_chunk_size << " blocks per chunk";

    // Load hardcoded checkpoints from CryptoNoteConfig.h as validation targets
    // These serve as the source of truth - all P2P-received checkpoint lists must
    // hash to match these target hashes to be considered valid
    // 
    // IMPORTANT: Also store the old checkpoint block hashes for validation during chunk generation
    // This ensures backward compatibility - when generating chunks, we validate that
    // any checkpoint heights within a chunk match the old checkpoint values from CryptoNoteConfig.h
    m_old_checkpoint_hashes.clear();
    for (const auto &cp : m_testnet ? cn::TESTNET_CHECKPOINTS : cn::CHECKPOINTS)
    {
      crypto::Hash checkpoint_hash = NULL_HASH;
      if (common::podFromHex(cp.blockId, checkpoint_hash))
      {
        // Store old checkpoint hash (individual block hash at this height)
        // This will be used to validate chunks during generation
        m_old_checkpoint_hashes[cp.height] = checkpoint_hash;
      }
      add_checkpoint_target(cp.height, cp.blockId);
    }

    logger(INFO) << "Stored " << m_old_checkpoint_hashes.size() 
                               << " checkpoint block hashes from CryptoNoteConfig.h (PRIORITY 1)";

    // Fetch DNS checkpoint records (PRIORITY 2: secondary trusted source)
    const char* domain;
    if (m_testnet)
      domain = TESTNET_DNS_CHECKPOINT_DOMAIN;
    else
      domain = DNS_CHECKPOINT_DOMAIN;

    std::vector<std::string>records;
    m_dns_checkpoint_hashes.clear();

    logger(INFO) << "Fetching DNS checkpoint records from " << domain;

    if (!common::fetch_dns_txt(domain, records)) {
      logger(INFO) << "DNS checkpoint lookup failed or no records found for " << domain 
                   << " (this is OK - will use CryptoNoteConfig.h checkpoints and P2P validation)";
    }
    else if (records.empty())
    {
      logger(INFO) << "DNS checkpoint domain " << domain << " exists but contains no TXT records "
                   << "(will use CryptoNoteConfig.h checkpoints and P2P validation)";
    }
    else
    {
      logger(INFO) << "Found " << records.size() << " DNS TXT record(s) from " << domain;
      
      uint32_t dns_checkpoints_added = 0;
      uint32_t dns_checkpoints_skipped = 0;
      uint32_t dns_checkpoints_invalid = 0;

    for (const auto& record : records) {
      uint32_t height;
      crypto::Hash hash = NULL_HASH;
      std::stringstream ss;
      size_t del = record.find_first_of(':');
        
        if (del == std::string::npos) {
          logger(DEBUGGING) << "Invalid DNS checkpoint record format (missing ':'): " << record;
          dns_checkpoints_invalid++;
          continue;
        }
        
      std::string height_str = record.substr(0, del), hash_str = record.substr(del + 1, 64);
      ss.str(height_str);
      ss >> height;
      char c;
        
      if ((ss.fail() || ss.get(c)) || !common::podFromHex(hash_str, hash)) {
          logger(DEBUGGING) << "Failed to parse DNS checkpoint record: " << record;
          dns_checkpoints_invalid++;
        continue;
      }

        // Check if this height already exists in CryptoNoteConfig.h (PRIORITY 1 takes precedence)
        if (m_old_checkpoint_hashes.count(height) > 0) {
          logger(DEBUGGING) << "Checkpoint at height " << height << " already exists in CryptoNoteConfig.h. "
                           << "Skipping DNS checkpoint (CryptoNoteConfig.h takes precedence).";
          dns_checkpoints_skipped++;
          continue;
        }

        // Store DNS checkpoint hash (for use in chunk generation with priority)
        m_dns_checkpoint_hashes[height] = hash;
        
        // Also add as target for validation (if not already a target)
        if (m_targets.count(height) == 0) {
        add_checkpoint_target(height, hash_str);
        }
        
        dns_checkpoints_added++;
        logger(INFO) << "Added DNS checkpoint (PRIORITY 2): " << height_str << ":" << hash_str;
      }
      
      if (dns_checkpoints_added > 0)
      {
        logger(INFO) << "Stored " << m_dns_checkpoint_hashes.size() 
                                   << " DNS checkpoint block hashes (PRIORITY 2)"
                                   << " (" << dns_checkpoints_added << " added, " 
                                   << dns_checkpoints_skipped << " skipped - already in CryptoNoteConfig.h, "
                                   << dns_checkpoints_invalid << " invalid records)";
      }
      else
      {
        logger(INFO) << "No valid DNS checkpoints added (all " << records.size() 
                     << " record(s) were either invalid, duplicate, or already in CryptoNoteConfig.h)";
      }
    }
  }

  void CheckpointList::convert_old_checkpoints_to_list_hashes(
    std::function<std::vector<crypto::Hash>(uint32_t startHeight, uint32_t maxCount)> getBlockIdsFunc)
  {
    // Store old targets temporarily (they're individual block hashes from CryptoNoteConfig.h)
    // Note: m_targets stores size (height+1) as key, so we need to convert back to get heights
    // IMPORTANT: m_old_checkpoint_hashes is preserved - it contains the individual block hashes
    // needed for validation during chunk generation
    std::map<uint32_t, crypto::Hash> old_targets = m_targets;
    m_targets.clear();
    m_valid_point_sizes.clear();
    
    logger(INFO) << "Converting old-style checkpoints (individual block hashes) "
                          << "to new-style (list hashes) for P2P compatibility";
    logger(INFO) << "Preserving " << m_old_checkpoint_hashes.size() 
                               << " old checkpoint block hashes for chunk validation";
    
    uint32_t converted_count = 0;
    
    // For each old checkpoint, compute the list hash from genesis to that height
    for (const auto& old_target : old_targets)
    {
      uint32_t size = old_target.first;  // This is already height+1 (stored by add_checkpoint_target)
      uint32_t height = size - 1;
      
      // Get block IDs from genesis (0) to this height
      std::vector<crypto::Hash> blockIds = getBlockIdsFunc(0, size);
      
      if (blockIds.size() != size)
      {
        logger(WARNING) << "Could not get all block IDs for height " 
                                 << height << " (got " << blockIds.size() << ", expected " << size << "). Skipping.";
        continue;
      }
      
      // VALIDATION: Before converting, verify that the block hash at this checkpoint height
      // matches the old checkpoint value from CryptoNoteConfig.h
      // This ensures the blockchain matches the expected checkpoints
      auto old_checkpoint_it = m_old_checkpoint_hashes.find(height);
      if (old_checkpoint_it != m_old_checkpoint_hashes.end())
      {
        const crypto::Hash& expected_hash = old_checkpoint_it->second;
        const crypto::Hash& actual_hash = blockIds[height]; // blockIds[height] is the hash at that height
        
        if (actual_hash != expected_hash)
        {
          logger(ERROR) << "CHECKPOINT VALIDATION FAILED at height " << height 
                                 << "! Expected (from CryptoNoteConfig.h): " << expected_hash
                                 << ", Got (from blockchain.dat): " << actual_hash
                                 << ". Cannot convert checkpoints - blockchain doesn't match expected values.";
          // Don't clear m_targets - keep old format for now
          return;
        }
        
        logger(INFO) << "Validated checkpoint at height " << height 
                                   << " before conversion (matches CryptoNoteConfig.h)";
      }
      
      // Compute hash of the list of block IDs from genesis to this height
      crypto::Hash listHash = crypto::cn_fast_hash(blockIds.data(), blockIds.size() * sizeof(crypto::Hash));
      
      // Store as new target (list hash instead of individual block hash)
      m_targets[size] = listHash;
      m_valid_point_sizes.insert(size);
      converted_count++;
      
      logger(INFO) << "Converted checkpoint at height " << height 
                                  << " (old hash: " << old_target.second << ") to list hash: " << listHash;
    }
    
    logger(INFO) << "Converted " << converted_count 
                                << " checkpoint validation targets for P2P compatibility";
    logger(INFO) << "Old checkpoint block hashes preserved for chunk validation: " 
                               << m_old_checkpoint_hashes.size() << " checkpoints";
  }

  /**
   * Generate checkpoint chunks from block IDs
   * 
   * CHUNKING EXPLANATION:
   * Instead of storing all 1.87M block hashes (60MB), we store chunk hashes:
   * - Chunk 0: hash(blocks 0-9999)     -> 1 hash (32 bytes)
   * - Chunk 1: hash(blocks 10000-19999) -> 1 hash (32 bytes)
   * - ...
   * - Chunk 186: hash(blocks 1860000-1869999) -> 1 hash (32 bytes)
   * 
   * Total: 187 chunks × 32 bytes = ~6KB (vs 60MB for full list)
   * 
   * Benefits:
   * 1. Fast P2P comparison: Compare 187 chunk hashes instead of hashing 60MB
   * 2. Fast disagreement detection: Binary search through chunks (log2(187) ≈ 8 comparisons)
   * 3. Efficient rollback: Know exactly which 10,000-block range has the problem
   * 4. Small file size: 6KB vs 60MB
   * 
   * VALIDATION PROCESS:
   * Before creating each chunk hash, we validate against old checkpoints from CryptoNoteConfig.h:
   * 1. Check if any checkpoint heights (e.g., 290665, 290674, 290675, 290676, 290720) fall within the chunk
   * 2. Validate that the block hash at those checkpoint heights matches CryptoNoteConfig.h values
   * 3. Only if validation passes, create the chunk hash
   * 
   * This ensures:
   * - Correctness: blockchain.dat matches expected checkpoints
   * - Compatibility: Smooth transition from version 1 to version 2
   * - Security: Invalid or corrupted blockchains cannot create valid chunks
   * 
   * @param getBlockIdsFunc Function to retrieve block IDs from the blockchain
   * @param max_height Maximum block height to generate chunks for
   * @return true if chunks were successfully generated and validated
   */
  bool CheckpointList::generate_chunks_from_block_ids(
    std::function<std::vector<crypto::Hash>(uint32_t startHeight, uint32_t maxCount)> getBlockIdsFunc,
    uint32_t max_height)
  {
    std::vector<crypto::Hash> chunks_to_save;
    std::string file_to_save;
    uint32_t chunk_size;
    
    // Generate chunks while holding lock
    {
      const std::lock_guard<std::mutex> lock(m_chunks_lock);
      
      // Calculate how many chunks we need
    // SIMPLIFIED CHUNK STRUCTURE (block 0/genesis is NOT included in chunks):
    // chunk[0]: blocks 1 to chunk_size (inclusive) = chunk_size blocks
    // chunk[1]: blocks (chunk_size + 1) to (2 * chunk_size) (inclusive) = chunk_size blocks
    // chunk[n]: blocks (n * chunk_size + 1) to ((n + 1) * chunk_size) (inclusive) = chunk_size blocks
    // 
    // If max_height = 1,870,000 and chunk_size = 10,000:
    // - chunk[0]: 1-10000 (10,000 blocks)
    // - chunk[1]: 10001-20000 (10,000 blocks)
    // - ...
    // - chunk[186]: 1860001-1870000 (10,000 blocks)
    // So we need 187 chunks (0-186)
    // 
    // Formula: num_chunks = max_height / chunk_size
    uint32_t num_chunks = max_height / m_chunk_size;
    
    logger(INFO) << "Generating " << num_chunks 
                          << " checkpoint chunks (chunk size: " << m_chunk_size 
                          << ", max height: " << max_height << ") - block 0 (genesis) excluded";
    
    m_chunks.clear();
    m_chunks.reserve(num_chunks);
    
    // Generate each chunk
    for (uint32_t chunk_index = 0; chunk_index < num_chunks; chunk_index++)
    {
      // SIMPLIFIED: All chunks are uniform (chunk_size blocks each)
      // chunk[0]: blocks 1 to chunk_size
      // chunk[1]: blocks (chunk_size + 1) to (2 * chunk_size)
      // chunk[n]: blocks (n * chunk_size + 1) to ((n + 1) * chunk_size)
      uint32_t chunk_start_height = chunk_index * m_chunk_size + 1;
      uint32_t chunk_end_height = std::min((chunk_index + 1) * m_chunk_size, max_height);
      uint32_t blocks_in_chunk = chunk_end_height - chunk_start_height + 1;
      
      // Get all block IDs for this chunk from blockchain.dat
      std::vector<crypto::Hash> chunk_block_ids = getBlockIdsFunc(chunk_start_height, blocks_in_chunk);
      
      if (chunk_block_ids.size() != blocks_in_chunk)
      {
        logger(ERROR) << "Failed to get all block IDs for chunk " 
                               << chunk_index << " (got " << chunk_block_ids.size() 
                               << ", expected " << blocks_in_chunk << ")";
        m_chunks.clear();
        return false;
      }
      
      // PRIORITY ORDER for block hashes in chunk (applied in reverse order to maintain priority):
      // 1. Start with blockchain.dat (base data)
      // 2. Apply DNS checkpoints (overwrites blockchain.dat)
      // 3. Apply CryptoNoteConfig.h checkpoints (overwrites both DNS and blockchain.dat)
      // Final priority: CryptoNoteConfig.h > DNS > blockchain.dat
      
      std::vector<uint32_t> checkpoints_in_chunk;
      uint32_t checkpoints_from_config = 0;
      uint32_t checkpoints_from_dns = 0;
      
      // STEP 1: Apply DNS checkpoints (PRIORITY 2)
      // These will be overwritten by CryptoNoteConfig.h if there's a conflict
      for (const auto& checkpoint : m_dns_checkpoint_hashes)
      {
        uint32_t checkpoint_height = checkpoint.first;
        if (checkpoint_height >= chunk_start_height && checkpoint_height <= chunk_end_height)
        {
          uint32_t index_in_chunk = checkpoint_height - chunk_start_height;
          
          if (index_in_chunk >= chunk_block_ids.size())
          {
            logger(ERROR) << "DNS checkpoint height " << checkpoint_height 
                                   << " index out of range in chunk " << chunk_index;
            m_chunks.clear();
            return false;
          }
          
          // VALIDATION: Verify that blockchain.dat matches the DNS checkpoint
          const crypto::Hash& blockchain_hash = chunk_block_ids[index_in_chunk];
          const crypto::Hash& dns_hash = checkpoint.second;
          
          if (blockchain_hash != dns_hash)
          {
            logger(ERROR) << "DNS CHECKPOINT VALIDATION FAILED for chunk " 
                                   << chunk_index << " at height " << checkpoint_height << "!"
                                   << " Expected (from DNS): " << dns_hash
                                   << ", Got (from blockchain.dat): " << blockchain_hash
                                   << ". Cannot create chunk - blockchain validation failed.";
            m_chunks.clear();
            return false;
          }
          
          // Replace with DNS checkpoint hash
          chunk_block_ids[index_in_chunk] = dns_hash;
          checkpoints_from_dns++;
          checkpoints_in_chunk.push_back(checkpoint_height);
          
          logger(DEBUGGING) << "Applied DNS checkpoint hash for height " 
                                       << checkpoint_height << " in chunk " << chunk_index;
        }
      }
      
      // STEP 2: Apply CryptoNoteConfig.h checkpoints (PRIORITY 1 - HIGHEST)
      // These overwrite both blockchain.dat and DNS checkpoints
      for (const auto& checkpoint : m_old_checkpoint_hashes)
      {
        uint32_t checkpoint_height = checkpoint.first;
        if (checkpoint_height >= chunk_start_height && checkpoint_height <= chunk_end_height)
        {
          uint32_t index_in_chunk = checkpoint_height - chunk_start_height;
          
          if (index_in_chunk >= chunk_block_ids.size())
          {
            logger(ERROR) << "CryptoNoteConfig.h checkpoint height " << checkpoint_height 
                                   << " index out of range in chunk " << chunk_index;
            m_chunks.clear();
            return false;
          }
          
          // VALIDATION: Verify that blockchain.dat matches the CryptoNoteConfig.h checkpoint
          // (We validate against blockchain.dat, not the potentially overwritten DNS value)
          // Get the original blockchain hash by checking if we already applied DNS
          const crypto::Hash& config_hash = checkpoint.second;
          const crypto::Hash& current_hash = chunk_block_ids[index_in_chunk];
          
          // If this height was already overwritten by DNS, we need to validate against blockchain.dat
          // For now, we validate against the current value (which might be DNS or blockchain.dat)
          // The important thing is that blockchain.dat must match the config checkpoint
          // We'll validate this by checking if current_hash matches config_hash OR if we need to check blockchain.dat
          
          // Actually, we should validate against blockchain.dat directly
          // But we've already overwritten chunk_block_ids with DNS values
          // So we need to track original blockchain.dat values, OR re-fetch, OR validate before overwriting
          // Simplest: validate that current value (blockchain.dat or DNS) matches config
          // If DNS was applied, current_hash is DNS hash, which should match blockchain.dat (we validated above)
          // So if config_hash != current_hash, we need to check if blockchain.dat matches config_hash
          
          // Re-fetch the original blockchain.dat hash for validation
          std::vector<crypto::Hash> original_block_ids = getBlockIdsFunc(checkpoint_height, 1);
          if (original_block_ids.empty() || original_block_ids[0] != config_hash)
          {
            logger(ERROR) << "CryptoNoteConfig.h CHECKPOINT VALIDATION FAILED for chunk " 
                                   << chunk_index << " at height " << checkpoint_height << "!"
                                   << " Expected (from CryptoNoteConfig.h): " << config_hash
                                   << ", Got (from blockchain.dat): " << (original_block_ids.empty() ? NULL_HASH : original_block_ids[0])
                                   << ". Cannot create chunk - blockchain validation failed.";
            m_chunks.clear();
            return false;
          }
          
          // Replace with CryptoNoteConfig.h checkpoint hash (overwrites DNS if it was applied)
          chunk_block_ids[index_in_chunk] = config_hash;
          
          // Update counters (only count if not already counted from DNS)
          if (std::find(checkpoints_in_chunk.begin(), checkpoints_in_chunk.end(), checkpoint_height) == checkpoints_in_chunk.end())
          {
            checkpoints_in_chunk.push_back(checkpoint_height);
          }
          else
          {
            // This checkpoint was already applied from DNS, now overwritten by CryptoNoteConfig.h
            checkpoints_from_dns--; // Remove DNS count, add config count
          }
          checkpoints_from_config++;
          
          logger(DEBUGGING) << "Applied CryptoNoteConfig.h checkpoint hash for height " 
                                       << checkpoint_height << " in chunk " << chunk_index
                                       << " (overwrites DNS if present)";
        }
      }
      
      // Log validation success if there were checkpoints in this chunk
      if (!checkpoints_in_chunk.empty())
      {
        std::stringstream ss;
        ss << "Validated and replaced " << checkpoints_in_chunk.size() << " checkpoint(s) in chunk " << chunk_index 
           << " (heights: ";
        for (size_t i = 0; i < checkpoints_in_chunk.size(); i++) {
          if (i > 0) ss << ", ";
          ss << checkpoints_in_chunk[i];
        }
        ss << ") - Priority: " << checkpoints_from_config << " from CryptoNoteConfig.h, " 
           << checkpoints_from_dns << " from DNS, rest from blockchain.dat";
        logger(INFO) << ss.str();
      }
      
      // Compute hash of this chunk's block IDs
      // This is the chunk hash: hash(block_id_0 || block_id_1 || ... || block_id_N)
      crypto::Hash chunk_hash = crypto::cn_fast_hash(
        chunk_block_ids.data(), 
        chunk_block_ids.size() * sizeof(crypto::Hash)
      );
      
      m_chunks.push_back(chunk_hash);
      
      if ((chunk_index + 1) % 10 == 0 || chunk_index == num_chunks - 1)
      {
        logger(INFO) << "Generated chunk " 
                                   << (chunk_index + 1) << "/" << num_chunks 
                                   << " (blocks " << chunk_start_height << "-" << chunk_end_height << ")";
      }
    }
    
      logger(INFO) << "Successfully generated " 
                            << m_chunks.size() << " checkpoint chunks";
      
      // Mark all chunks as confirmed (they were validated against hardcoded checkpoints)
      // Chunks generated from hardcoded checkpoints are automatically confirmed
      for (uint32_t i = 0; i < m_chunks.size(); i++)
      {
        m_confirmed_chunks.insert(i);
      }
      
      logger(INFO) << "Marked " << m_chunks.size() 
                            << " chunks as confirmed (validated against CryptoNoteConfig.h)";
      
      // Copy data before releasing lock (to avoid deadlock when calling save_checkpoints_impl)
      chunks_to_save = m_chunks;
      file_to_save = m_save_file;
      chunk_size = m_chunk_size;
    } // Lock released here
    
    // Now save without holding the lock
    return save_checkpoints_impl(chunks_to_save, file_to_save, chunk_size);
  }

  /**
   * Add a single chunk (called when a new chunk_size blocks are added)
   * 
   * This is called every 10,000 blocks (mainnet) or 25,000 blocks (testnet)
   * to compute a new chunk hash and store it in memory.
   * 
   * CRITICAL: This does NOT save to checkpoint.dat. The chunk is stored in memory only.
   * After peer consensus is reached, call add_verified_chunk_to_file() to save it.
   * Adding to checkpoint.dat IS the confirmation - the file only contains verified chunks.
   * 
   * @param getBlockIdsFunc Function to retrieve block IDs from the blockchain
   * @param chunk_start_height The starting height of the chunk to add
   * @return true if chunk hash was successfully computed and stored in memory
   */
  bool CheckpointList::add_chunk_from_block_ids(
    std::function<std::vector<crypto::Hash>(uint32_t startHeight, uint32_t maxCount)> getBlockIdsFunc,
    uint32_t chunk_start_height)
  {
    {
      const std::lock_guard<std::mutex> lock(m_chunks_lock);
      
      // SIMPLIFIED: Calculate chunk index and boundaries
      // All chunks are uniform: chunk_size blocks each (block 0/genesis excluded)
      // chunk[0]: blocks 1 to chunk_size
      // chunk[1]: blocks (chunk_size + 1) to (2 * chunk_size)
      // chunk[n]: blocks (n * chunk_size + 1) to ((n + 1) * chunk_size)
      // 
      // Formula: chunk_index = (chunk_start_height - 1) / chunk_size
      // Example: chunk_start_height = 1 → chunk_index = 0 ✓
      //          chunk_start_height = 10001 → chunk_index = 10000 / 10000 = 1 ✓
      //          chunk_start_height = 20001 → chunk_index = 20000 / 10000 = 2 ✓
      uint32_t chunk_index = (chunk_start_height - 1) / m_chunk_size;
      uint32_t chunk_end_height = (chunk_index + 1) * m_chunk_size;
      uint32_t blocks_in_chunk = m_chunk_size;
      
      // Get all block IDs for this chunk from blockchain.dat
      std::vector<crypto::Hash> chunk_block_ids = getBlockIdsFunc(chunk_start_height, blocks_in_chunk);
      
      if (chunk_block_ids.size() != blocks_in_chunk)
      {
        logger(ERROR) << "Failed to get all block IDs for chunk " 
                               << chunk_index << " (got " << chunk_block_ids.size() 
                               << ", expected " << blocks_in_chunk << ")";
        return false;
      }
      
      // PRIORITY ORDER for block hashes in chunk (applied in reverse order to maintain priority):
      // 1. Start with blockchain.dat (base data - already loaded)
      // 2. Apply DNS checkpoints (overwrites blockchain.dat)
      // 3. Apply CryptoNoteConfig.h checkpoints (overwrites both DNS and blockchain.dat)
      // Final priority: CryptoNoteConfig.h > DNS > blockchain.dat
      
      uint32_t checkpoints_from_config = 0;
      uint32_t checkpoints_from_dns = 0;
      
      // STEP 1: Apply DNS checkpoints (PRIORITY 2)
      // These will be overwritten by CryptoNoteConfig.h if there's a conflict
      for (const auto& checkpoint : m_dns_checkpoint_hashes)
      {
        uint32_t checkpoint_height = checkpoint.first;
        if (checkpoint_height >= chunk_start_height && checkpoint_height <= chunk_end_height)
        {
          uint32_t index_in_chunk = checkpoint_height - chunk_start_height;
          
          // VALIDATION: Verify that blockchain.dat matches the DNS checkpoint
          const crypto::Hash& blockchain_hash = chunk_block_ids[index_in_chunk];
          const crypto::Hash& dns_hash = checkpoint.second;
          
          if (blockchain_hash != dns_hash)
          {
            logger(ERROR) << "DNS CHECKPOINT VALIDATION FAILED for chunk " 
                                   << chunk_index << " at height " << checkpoint_height << "!"
                                   << " Expected (from DNS): " << dns_hash
                                   << ", Got (from blockchain.dat): " << blockchain_hash;
            return false;
          }
          
          // Replace with DNS checkpoint hash
          chunk_block_ids[index_in_chunk] = dns_hash;
          checkpoints_from_dns++;
          
          logger(DEBUGGING) << "Applied DNS checkpoint hash for height " 
                                       << checkpoint_height << " in chunk " << chunk_index;
        }
      }
      
      // STEP 2: Apply CryptoNoteConfig.h checkpoints (PRIORITY 1 - HIGHEST)
      // These overwrite both blockchain.dat and DNS checkpoints
      for (const auto& checkpoint : m_old_checkpoint_hashes)
      {
        uint32_t checkpoint_height = checkpoint.first;
        if (checkpoint_height >= chunk_start_height && checkpoint_height <= chunk_end_height)
        {
          uint32_t index_in_chunk = checkpoint_height - chunk_start_height;
          const crypto::Hash& config_hash = checkpoint.second;
          
          // VALIDATION: Verify that blockchain.dat matches the CryptoNoteConfig.h checkpoint
          // Re-fetch the original blockchain.dat hash for validation (before DNS overwrite)
          std::vector<crypto::Hash> original_block_ids = getBlockIdsFunc(checkpoint_height, 1);
          if (original_block_ids.empty() || original_block_ids[0] != config_hash)
          {
            logger(ERROR) << "CryptoNoteConfig.h CHECKPOINT VALIDATION FAILED for chunk " 
                                   << chunk_index << " at height " << checkpoint_height << "!"
                                   << " Expected (from CryptoNoteConfig.h): " << config_hash
                                   << ", Got (from blockchain.dat): " << (original_block_ids.empty() ? NULL_HASH : original_block_ids[0]);
            return false;
          }
          
          // Replace with CryptoNoteConfig.h checkpoint hash (overwrites DNS if it was applied)
          chunk_block_ids[index_in_chunk] = config_hash;
          
          // Update counters
          if (checkpoints_from_dns > 0)
          {
            // Check if this height was in DNS (we can't easily track which specific heights, so we approximate)
            // Actually, we can't know for sure without tracking, so we'll just count config checkpoints
            // The logging will show the actual priority applied
          }
          checkpoints_from_config++;
          
          logger(DEBUGGING) << "Applied CryptoNoteConfig.h checkpoint hash for height " 
                                       << checkpoint_height << " in chunk " << chunk_index
                                       << " (overwrites DNS if present)";
        }
      }
      
      uint32_t total_checkpoints_applied = checkpoints_from_config + checkpoints_from_dns;
      if (total_checkpoints_applied > 0)
      {
        logger(INFO) << "Applied " << total_checkpoints_applied 
                                   << " checkpoint(s) to chunk " << chunk_index 
                                   << " (Priority: " << checkpoints_from_config << " from CryptoNoteConfig.h, " 
                                   << checkpoints_from_dns << " from DNS, rest from blockchain.dat)";
      }
      
      // Compute hash of this chunk's block IDs
      crypto::Hash chunk_hash = crypto::cn_fast_hash(
        chunk_block_ids.data(), 
        chunk_block_ids.size() * sizeof(crypto::Hash)
      );
      
      // Ensure we have enough space in m_chunks
      if (chunk_index >= m_chunks.size())
      {
        m_chunks.resize(chunk_index + 1);
      }
      
      m_chunks[chunk_index] = chunk_hash;
      
      logger(INFO) << "Computed chunk " << chunk_index 
                            << " hash (blocks " << chunk_start_height << "-" << chunk_end_height << ")"
                            << " - stored in memory, NOT yet saved to checkpoint.dat"
                            << " (requires peer consensus with peers having uptime > "
                            << get_min_peer_uptime_blocks() << " blocks)";
    }
    
    // NOTE: We do NOT save to checkpoint.dat here
    // The chunk will be saved after peer consensus via add_verified_chunk_to_file()
    return true;
  }
  
  /**
   * Add a verified chunk to checkpoint.dat (marks it as confirmed)
   * 
   * This is called after peer consensus is reached (M out of K peers agree).
   * Adding a chunk to checkpoint.dat IS the confirmation - the file only contains verified chunks.
   * 
   * @param chunk_index The chunk index to add to checkpoint.dat
   * @return true if chunk was successfully added to file
   */
  bool CheckpointList::add_verified_chunk_to_file(uint32_t chunk_index)
  {
    std::vector<crypto::Hash> chunks_to_save;
    std::string file_to_save;
    uint32_t chunk_size;
    
    {
      const std::lock_guard<std::mutex> lock(m_chunks_lock);
      
      // Verify chunk exists in memory
      if (chunk_index >= m_chunks.size() || m_chunks[chunk_index] == NULL_HASH)
      {
        logger(ERROR) << "Cannot add chunk " << chunk_index 
                               << " to checkpoint.dat: chunk not found in memory";
        return false;
      }
      
      // Get current chunks from file (only verified chunks are in file)
      // We need to load existing verified chunks and append the new one
      std::vector<crypto::Hash> verified_chunks;
      
      // Load existing verified chunks from file
      std::ifstream file(m_save_file, std::ios::binary | std::ios::ate);
      if (file.is_open())
      {
        uint64_t fsize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        if (fsize > 0 && fsize % sizeof(crypto::Hash) == 0)
        {
          uint32_t num_existing_chunks = static_cast<uint32_t>(fsize / sizeof(crypto::Hash));
          verified_chunks.resize(num_existing_chunks);
          file.read(reinterpret_cast<char*>(verified_chunks.data()), fsize);
        }
        file.close();
      }
      
      // Ensure verified_chunks has enough space for the new chunk
      if (chunk_index >= verified_chunks.size())
      {
        verified_chunks.resize(chunk_index + 1, NULL_HASH);
      }
      
      // Add the verified chunk
      verified_chunks[chunk_index] = m_chunks[chunk_index];
      
      // Mark as confirmed (chunks in checkpoint.dat are confirmed)
      m_confirmed_chunks.insert(chunk_index);
      
      logger(INFO) << "Adding chunk " << chunk_index 
                                  << " to checkpoint.dat";
      
      // Copy data before releasing lock
      chunks_to_save = verified_chunks;
      file_to_save = m_save_file;
      chunk_size = m_chunk_size;
    } // Lock released here
    
    // Save verified chunks to file
    return save_checkpoints_impl(chunks_to_save, file_to_save, chunk_size);
  }

  /**
   * Get the hash of a specific chunk
   * 
   * @param chunk_index The chunk index (0-based)
   * @return The chunk hash, or NULL_HASH if chunk doesn't exist
   */
  crypto::Hash CheckpointList::get_chunk_hash(uint32_t chunk_index) const
  {
    const std::lock_guard<std::mutex> lock(m_chunks_lock);
    
    if (chunk_index >= m_chunks.size())
      return NULL_HASH;
    
    return m_chunks[chunk_index];
  }

  /**
   * Get all chunk hashes (for P2P comparison)
   * 
   * This allows nodes to quickly compare their checkpoint chunks:
   * 1. Node A sends: [chunk0_hash, chunk1_hash, ..., chunk186_hash]
   * 2. Node B compares: chunk0 matches? chunk1 matches? ...
   * 3. First mismatch found -> problem is in that chunk's 10,000-block range
   * 4. Request specific chunk's block IDs to find exact block
   * 
   * @return Vector of all chunk hashes
   */
  std::vector<crypto::Hash> CheckpointList::get_all_chunk_hashes() const
  {
    const std::lock_guard<std::mutex> lock(m_chunks_lock);
    return m_chunks; // Return copy
  }

  /**
   * Set chunk hashes (from P2P or file)
   * 
   * @param chunk_hashes Vector of chunk hashes to set
   * @return true if successfully set
   */
  bool CheckpointList::set_chunk_hashes(std::vector<crypto::Hash>&& chunk_hashes)
  {
    std::vector<crypto::Hash> chunks_to_save;
    std::string file_to_save;
    uint32_t chunk_size;
    
    // Set chunks while holding lock
    {
      const std::lock_guard<std::mutex> lock(m_chunks_lock);
      
      // Basic validation: should have at least some chunks
      if (chunk_hashes.empty())
      {
        logger(WARNING) << "Received empty chunk list";
        return false;
      }
      
      m_chunks = std::move(chunk_hashes);
      logger(INFO) << "Set " << m_chunks.size() << " checkpoint chunks";
      
      // Copy data before releasing lock (to avoid deadlock when calling save_checkpoints_impl)
      chunks_to_save = m_chunks;
      file_to_save = m_save_file;
      chunk_size = m_chunk_size;
    } // Lock released here
    
    // Save to file without holding lock
    return save_checkpoints_impl(chunks_to_save, file_to_save, chunk_size);
  }

  /**
   * Validate a specific chunk against expected hash
   * 
   * Used during P2P comparison: when a chunk mismatch is found,
   * validate the chunk to confirm the disagreement.
   * 
   * @param chunk_index The chunk index to validate
   * @param expected_hash The expected hash for this chunk
   * @return true if chunk hash matches expected hash
   */
  bool CheckpointList::validate_chunk(uint32_t chunk_index, const crypto::Hash& expected_hash) const
  {
    const std::lock_guard<std::mutex> lock(m_chunks_lock);
    
    if (chunk_index >= m_chunks.size())
    {
      logger(WARNING) << "Chunk index " << chunk_index 
                               << " out of range (have " << m_chunks.size() << " chunks)";
      return false;
    }
    
    bool matches = (m_chunks[chunk_index] == expected_hash);
    
    if (!matches)
    {
      logger(WARNING) << "Chunk " << chunk_index 
                               << " mismatch: expected " << expected_hash 
                               << ", got " << m_chunks[chunk_index];
    }
    
    return matches;
  }

  bool CheckpointList::add_checkpoint_target(uint32_t height, const std::string &hash_str) {
    crypto::Hash h = NULL_HASH;

    if (!common::podFromHex(hash_str, h)) {
      logger(ERROR) << "Incorrect hash in checkpoints";
      return false;
    }

    if (!(0 == m_targets.count(height))) {
      logger(INFO) << "Checkpoint already exists for height " << height;
      return false;
    }
    
    height += 1;
    m_targets[height] = h;
    m_valid_point_sizes.insert(height);

    return true;
  }

  bool CheckpointList::add_checkpoint_target_for_test(uint32_t height, const std::string& hash_str) {
    return add_checkpoint_target(height, hash_str);
  }

  bool CheckpointList::set_checkpoint_list(std::vector<crypto::Hash>&& points) 
  {
    const std::lock_guard<std::mutex> lock(m_points_lock);
    uint32_t point_size = points.size();
    if(m_valid_point_sizes.find(point_size) == m_valid_point_sizes.end())
      return false;

    // Validate the checkpoint list against the hardcoded target from CryptoNoteConfig.h
    // The target hash is the expected hash of the continuous list of block hashes from height 0
    crypto::Hash hv = crypto::cn_fast_hash(points.data(), points.size() * sizeof(crypto::Hash));
    if(hv != m_targets[point_size])
    {
      logger(ERROR) << "CheckpointList verification failed for height " << point_size-1 << 
                         ". Expected hash (from hardcoded checkpoints): " << m_targets[point_size]  << 
                         ", Fetched hash: " << hv;
      return false;
    }
    
    m_points = std::move(points);
    logger(INFO) << "Loaded " << m_points.size() << " checkpoints from local index";
    
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

    // Validate the combined checkpoint list against the hardcoded target from CryptoNoteConfig.h
    // This ensures P2P-received checkpoint lists match the expected values
    crypto::Hash hv = crypto::cn_fast_hash(new_points.data(), new_points.size() * sizeof(crypto::Hash));
    if(hv != m_targets[point_size])
    {
      logger(ERROR) << "CheckpointList verification failed for height " << point_size-1 << 
                         ". Expected hash (from hardcoded checkpoints): " << m_targets[point_size]  << 
                         ", Fetched hash: " << hv;
      return false;
    }
    
    m_points = std::move(new_points);
    logger(INFO) << "Loaded " << points.size() << " checkpoints from p2p, total " << m_points.size();
    
    save_checkpoints();
    return true;
  }

  /**
   * Load checkpoints from file (chunked format)
   * 
   * FILE FORMAT:
   * - Binary file containing chunk hashes
   * - Each chunk hash is 32 bytes (crypto::Hash)
   * - File size = num_chunks × 32 bytes
   * 
   * Example: 187 chunks = 187 × 32 = 5,984 bytes (~6KB)
   * vs old format: 1,870,000 blocks × 32 = 59,840,000 bytes (~60MB)
   */
  bool CheckpointList::load_checkpoints_from_file_chunked()
  {
    std::ifstream file(m_save_file, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
      // File doesn't exist - this is normal on first run or after deleting checkpoint.dat
      logger(DEBUGGING) << "checkpoint.dat not found (will generate from blockchain if needed)";
      return false;
    }

    uint64_t fsize = file.tellg();
    
    // Validate file size: must be multiple of hash size
    if (fsize % sizeof(crypto::Hash) != 0) {
      logger(ERROR) << "Invalid checkpoint file size: " << fsize 
                             << " (not multiple of " << sizeof(crypto::Hash) << ")";
      return false;
    }
    
    uint32_t num_chunks = static_cast<uint32_t>(fsize / sizeof(crypto::Hash));
    
    if (num_chunks == 0) {
      logger(WARNING) << "Checkpoint file is empty";
      return false;
    }
    
    file.seekg(0, std::ios::beg);

    std::vector<crypto::Hash> chunks(num_chunks);
    if (!file.read(reinterpret_cast<char*>(chunks.data()), fsize)) {
      logger(ERROR) << "Error reading checkpoint file";
      return false;
    }

    const std::lock_guard<std::mutex> lock(m_chunks_lock);
    m_chunks = std::move(chunks);
    
    // CRITICAL: All chunks in checkpoint.dat are automatically confirmed
    // The file only contains verified chunks (up to hardcoded checkpoints + peer-verified chunks)
    // On restart, everything in checkpoint.dat is 100% reliable - no re-validation needed
    m_confirmed_chunks.clear();
    for (uint32_t i = 0; i < m_chunks.size(); i++)
    {
      m_confirmed_chunks.insert(i);
    }
    
    // Calculate covered height
    // SIMPLIFIED: All chunks are uniform (block 0 excluded)
    // chunk[0]: covers blocks 1 to chunk_size (chunk_size blocks)
    // chunk[1]: covers blocks (chunk_size + 1) to (2 * chunk_size) (chunk_size blocks)
    // chunk[n]: covers blocks (n * chunk_size + 1) to ((n + 1) * chunk_size) (chunk_size blocks)
    // Formula: covered_height = num_chunks * chunk_size
    uint32_t covered_height = num_chunks * m_chunk_size;
    
    uint32_t greatest_target_height = get_greatest_target_height();
    // Calculate which chunk the greatest target belongs to
    // Formula: chunk_index = (height - 1) / chunk_size
    uint32_t greatest_target_chunk = (greatest_target_height - 1) / m_chunk_size;
    
    // Calculate hardcoded covered height (up to greatest target chunk)
    // SIMPLIFIED: hardcoded_end = (greatest_target_chunk + 1) * chunk_size
    uint32_t hardcoded_end = (greatest_target_chunk + 1) * m_chunk_size;
    uint32_t hardcoded_covered_height = std::min(hardcoded_end, covered_height);
    
    logger(INFO) << "Loaded " << num_chunks 
                          << " checkpoint chunks from disk (covers up to height " << covered_height << ")";
    logger(INFO) << "Auto-confirmed ALL " << m_confirmed_chunks.size() 
                          << " chunks from checkpoint.dat (100% reliable on restart)"
                          << " - up to height " << hardcoded_covered_height 
                          << " from hardcoded checkpoints, beyond that from peer consensus";
    return true;
  }

  /**
   * Save checkpoints to file (chunked format)
   * 
   * Writes chunk hashes to binary file for fast loading and P2P sharing.
   */
  bool CheckpointList::save_checkpoints()
  {
    std::vector<crypto::Hash> chunks_to_save;
    std::string file_to_save;
    uint32_t chunk_size;
    
    // Copy data while holding lock
    {
      const std::lock_guard<std::mutex> lock(m_chunks_lock);
      
      if (m_chunks.empty()) {
        logger(WARNING) << "No chunks to save";
        return false;
      }
      
      // Copy data before releasing lock (to avoid deadlock when calling save_checkpoints_impl)
      chunks_to_save = m_chunks;
      file_to_save = m_save_file;
      chunk_size = m_chunk_size;
    } // Lock released here
    
    // Save without holding lock
    return save_checkpoints_impl(chunks_to_save, file_to_save, chunk_size);
  }
  
  // Private implementation that doesn't lock (assumes caller handles locking)
  // NOTE: Only saves chunk hashes, NOT confirmation status (security: minimal disk footprint)
  // Confirmation status is restored on load: chunks up to last hardcoded checkpoint are auto-confirmed,
  // remaining chunks are validated via P2P consensus after restart
  bool CheckpointList::save_checkpoints_impl(const std::vector<crypto::Hash>& chunks, const std::string& file_path, uint32_t chunk_size)
  {
    if (chunks.empty()) {
      logger(WARNING) << "No chunks to save";
      return false;
    }
    
    std::ofstream file(file_path, std::ios::binary);

    if (!file.is_open()) {
      logger(ERROR) << "Error opening checkpoint file for write: " << file_path;
      return false;
    }

    // Write all chunk hashes to file
    // NOTE: We do NOT save confirmation status - this is a security feature:
    // - Minimal disk footprint (less information that could be used against us)
    // - Confirmation is restored on load (chunks up to hardcoded checkpoints are auto-confirmed)
    // - Remaining chunks are validated via P2P consensus after restart
    file.write(reinterpret_cast<const char*>(chunks.data()), chunks.size() * sizeof(crypto::Hash));
    file.close();

    if (!file) {
      logger(ERROR) << "Error writing to checkpoint file: " << file_path;
      return false;
    }

    uint32_t covered_height = (static_cast<uint32_t>(chunks.size()) * chunk_size) - 1;
    logger(INFO) << "Saved " << chunks.size() 
                          << " checkpoint chunks to file (covers up to height " << covered_height << ")";
    return true;
  }

  /**
   * Load checkpoints from file (with automatic format detection)
   * 
   * Tries to load chunked format first, falls back to legacy format if needed.
   */
  bool CheckpointList::load_checkpoints_from_file()
  {
    // Try chunked format first (new format)
    if (load_checkpoints_from_file_chunked()) {
      return true;
    }
    
    // Fall back to legacy format (for backward compatibility during transition)
    // Note: This will also fail if checkpoint.dat doesn't exist at all
    logger(DEBUGGING) << "Chunked format load failed, checking for legacy format";
    return load_checkpoints_from_file_legacy();
  }

  /**
   * Load checkpoints from file (legacy format - full block hash list)
   * 
   * This is kept for backward compatibility during the transition period.
   * Old nodes may have checkpoint.dat files with full block hash lists.
   */
  bool CheckpointList::load_checkpoints_from_file_legacy()
  {
    std::ifstream file(m_save_file, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
      logger(DEBUGGING) << "No checkpoint.dat file found (neither chunked nor legacy format)";
      return false;
    }

    uint64_t fsize = file.tellg();
    if(!is_fsize_valid(fsize)) {
      logger(ERROR) << "Invalid file size" << fsize;
      return false;
    }
    uint32_t point_size = fsize / sizeof(crypto::Hash);
    
    file.seekg(0, std::ios::beg);

    std::vector<crypto::Hash> points(point_size);
    if (!file.read(reinterpret_cast<char*>(points.data()), fsize)) {
      logger(ERROR) << "error reading file";
      return false;
    }

    // Validate the checkpoint list loaded from file against the hardcoded target from CryptoNoteConfig.h
    // This ensures the saved checkpoint list matches the expected values from the source code
    crypto::Hash hv = crypto::cn_fast_hash(points.data(), points.size() * sizeof(crypto::Hash));
    if(hv != m_targets[point_size])
    {
      logger(ERROR) << "CheckpointList verification (from file) failed for height " << point_size-1
                          << ". Expected hash (from hardcoded checkpoints): " << m_targets[point_size]
                          << ", File hash: " << hv;
      return false;
    }

    const std::lock_guard<std::mutex> lock(m_points_lock);
    m_points = std::move(points);
    logger(INFO) << "Loaded " << m_points.size() << " checkpoints from disk (legacy format) " << m_save_file;
    
    // TODO: Convert legacy format to chunked format automatically
    // For now, we keep both formats during transition
    
    return true;
  }
  
  /**
   * Save checkpoints to file (legacy format - for backward compatibility)
   */
  bool CheckpointList::save_checkpoints_legacy()
  {
    const std::lock_guard<std::mutex> lock(m_points_lock);
    
    if (m_points.empty()) {
      return false;
    }
    
    std::ofstream file(m_save_file, std::ios::binary);

    if (!file.is_open()) {
      logger(ERROR) << "error opening file for write " << m_save_file;
      return false;
    }

    file.write(reinterpret_cast<const char*>(m_points.data()), m_points.size() * sizeof(crypto::Hash));
    file.close();

    if (!file) {
      logger(ERROR) << "error writing to file " << m_save_file;
      return false;
    }

    return true;
  }

  void CheckpointList::mark_chunk_confirmed(uint32_t chunk_index)
  {
    // Adding to checkpoint.dat IS the confirmation
    // This method is a convenience wrapper that calls add_verified_chunk_to_file()
    if (!add_verified_chunk_to_file(chunk_index))
    {
      logger(ERROR) << "Failed to mark chunk " << chunk_index 
                            << " as confirmed (failed to add to checkpoint.dat)";
      return;
    }
    
    // Log health metrics
    HealthMetrics metrics = get_health_metrics();
    
    logger(INFO) << "Marked chunk " << chunk_index 
                          << " as confirmed (added to checkpoint.dat via peer consensus with peers having uptime > " 
                          << get_min_peer_uptime_blocks() << " blocks)";
    
    // Log health metrics every 10 chunks or on important milestones
    if (chunk_index % 10 == 0 || chunk_index == metrics.highest_total)
    {
      logger(INFO) << "Checkpoint Health: " 
                            << metrics.confirmed_chunks << "/" << metrics.total_chunks 
                            << " chunks confirmed (" << (metrics.confirmation_ratio * 100.0) << "%), "
                            << "highest confirmed: " << metrics.highest_confirmed 
                            << ", unconfirmed beyond hardcoded: " << metrics.unconfirmed_count;
    }
  }

  bool CheckpointList::is_chunk_confirmed(uint32_t chunk_index) const
  {
    const std::lock_guard<std::mutex> lock(m_chunks_lock);
    return m_confirmed_chunks.find(chunk_index) != m_confirmed_chunks.end();
  }

  bool CheckpointList::can_answer_checkpoint_requests() const
  {
    const std::lock_guard<std::mutex> lock(m_chunks_lock);
    
    // Node can only answer checkpoint requests if it has confirmed chunks
    // This ensures we don't serve unverified data to other nodes
    if (m_chunks.empty())
      return false;
    
    // Check if we have at least some confirmed chunks
    // For safety, we require at least the first chunk to be confirmed
    // (chunks generated from hardcoded checkpoints are automatically confirmed)
    return !m_confirmed_chunks.empty();
  }

  uint32_t CheckpointList::get_highest_confirmed_chunk() const
  {
    const std::lock_guard<std::mutex> lock(m_chunks_lock);
    
    if (m_confirmed_chunks.empty())
      return 0;
    
    // Find the highest confirmed chunk index
    uint32_t highest = 0;
    for (uint32_t chunk_index : m_confirmed_chunks)
    {
      if (chunk_index > highest)
        highest = chunk_index;
    }
    
    return highest;
  }
  
  std::vector<uint32_t> CheckpointList::get_unverified_chunks() const
  {
    const std::lock_guard<std::mutex> lock(m_chunks_lock);
    
    std::vector<uint32_t> unverified;
    
    // Find chunks that are in memory but not confirmed (not in checkpoint.dat)
    for (uint32_t i = 0; i < m_chunks.size(); i++)
    {
      if (m_chunks[i] != NULL_HASH && m_confirmed_chunks.find(i) == m_confirmed_chunks.end())
      {
        unverified.push_back(i);
      }
    }
    
    return unverified;
  }

  CheckpointList::ConsensusRequirements CheckpointList::calculate_consensus_requirements(size_t available_peers) const
  {
    ConsensusRequirements req;
    
    // Simple fixed consensus requirements (M, K, n) based on network type
    // M = minimum agreements required (all M peers must agree)
    // K = total peers to sample
    // n = minimum distinct /16 networks required
    if (m_testnet)
    {
      req.min_agreements = cn::CKPT_MIN_CONSENSUS_PEERS_TESTNET;  // M
      req.min_peers = cn::CKPT_CONSENSUS_PEERS_TESTNET;           // K
      req.min_diverse_networks = cn::CKPT_MIN_DIVERSE_NETWORKS_TESTNET; // n
    }
    else
    {
      req.min_agreements = cn::CKPT_MIN_CONSENSUS_PEERS;  // M
      req.min_peers = cn::CKPT_CONSENSUS_PEERS;           // K
      req.min_diverse_networks = cn::CKPT_MIN_DIVERSE_NETWORKS; // n
    }
    
    // Warn if not enough peers available
    if (available_peers < req.min_peers)
    {
      logger(WARNING) << "Not enough peers available (" << available_peers 
                                << ") for consensus (need K=" << req.min_peers << " for " << (m_testnet ? "testnet" : "mainnet") << ")";
    }
    
    logger(INFO) << "Consensus requirements (" << (m_testnet ? "testnet" : "mainnet") 
                                << "): M=" << req.min_agreements 
                                << ", K=" << req.min_peers << ", n=" << req.min_diverse_networks
                                << " (have " << available_peers << " available peers)";
    
    return req;
  }

  uint32_t CheckpointList::get_network_16(uint32_t ip)
  {
    // Extract /16 network prefix: first 16 bits of IP address
    // For IPv4: network = (ip >> 16) & 0xFFFF
    return (ip >> 16) & 0xFFFF;
  }

  CheckpointList::HealthMetrics CheckpointList::get_health_metrics() const
  {
    HealthMetrics metrics;
    uint32_t greatest_target_chunk;
    
      // Get greatest target height first (doesn't require lock)
      uint32_t greatest_target_height = get_greatest_target_height();
      // SIMPLIFIED: Calculate which chunk the greatest target belongs to
      // Formula: chunk_index = (height - 1) / chunk_size
      greatest_target_chunk = (greatest_target_height - 1) / m_chunk_size;
    
    // Now acquire lock to read chunk data
    {
      const std::lock_guard<std::mutex> lock(m_chunks_lock);
      
      metrics.total_chunks = static_cast<uint32_t>(m_chunks.size());
      metrics.confirmed_chunks = static_cast<uint32_t>(m_confirmed_chunks.size());
      
      if (metrics.total_chunks > 0)
      {
        metrics.confirmation_ratio = static_cast<double>(metrics.confirmed_chunks) / metrics.total_chunks;
      }
      else
      {
        metrics.confirmation_ratio = 0.0;
      }
      
      // Find highest confirmed chunk
      metrics.highest_confirmed = 0;
      for (uint32_t chunk_index : m_confirmed_chunks)
      {
        if (chunk_index > metrics.highest_confirmed)
          metrics.highest_confirmed = chunk_index;
      }
      
      metrics.highest_total = metrics.total_chunks > 0 ? metrics.total_chunks - 1 : 0;
      
      // Calculate unconfirmed chunks (beyond hardcoded checkpoints)
      uint32_t confirmed_from_hardcoded = 0;
      for (uint32_t i = 0; i <= greatest_target_chunk && i < m_chunks.size(); i++)
      {
        if (m_confirmed_chunks.find(i) != m_confirmed_chunks.end())
          confirmed_from_hardcoded++;
      }
      
      metrics.unconfirmed_count = metrics.confirmed_chunks - confirmed_from_hardcoded;
    }
    
    return metrics;
  }
  
  /**
   * Validate chunks in checkpoint.dat against checkpoints from CryptoNoteConfig.h and DNS
   * 
   * REFINED STRATEGY: Validates the highest checkpoint from both sources (if available)
   * Priority: CryptoNoteConfig.h > DNS > blockchain.dat
   * 
   * Process:
   * 1. Find the highest checkpoint in CryptoNoteConfig.h (PRIORITY 1)
   * 2. Find the highest checkpoint in DNS (PRIORITY 2, if available)
   * 3. For each checkpoint we can verify (based on current blockchain height):
   *    - Compute chunk hash using priority order (CryptoNoteConfig.h > DNS > blockchain.dat)
   *    - Compare with stored chunk hash in checkpoint.dat
   * 4. If mismatch, return the mismatched chunk index
   * 
   * Example: 
   * - Blockchain height: 1,800,000
   * - CryptoNoteConfig.h highest: 1,700,000 (chunk 170)
   * - DNS highest: 1,900,000 (chunk 190, but we can't verify yet - not synced)
   * - We can only verify chunk 170 (CryptoNoteConfig.h checkpoint)
   * 
   * @param getBlockIdsFunc Function to retrieve block IDs from blockchain
   * @param current_blockchain_height Current blockchain height (to know what we can verify)
   * @return ValidationResult with is_valid flag and first_mismatched_chunk_index if invalid
   */
  CheckpointList::ValidationResult CheckpointList::validate_chunks_against_checkpoints(
    std::function<std::vector<crypto::Hash>(uint32_t startHeight, uint32_t maxCount)> getBlockIdsFunc,
    uint32_t current_blockchain_height) const
  {
    ValidationResult result;
    result.is_valid = true;
    result.first_mismatched_chunk_index = 0;
    
    const std::lock_guard<std::mutex> lock(m_chunks_lock);
    
    // If no chunks loaded, validation passes (nothing to validate)
    if (m_chunks.empty())
    {
      return result;
    }
    
    // Find highest checkpoints from both sources
    uint32_t highest_config_checkpoint_height = 0;
    crypto::Hash highest_config_checkpoint_hash = NULL_HASH;
    
    for (const auto& checkpoint : m_old_checkpoint_hashes)
    {
      if (checkpoint.first > highest_config_checkpoint_height)
      {
        highest_config_checkpoint_height = checkpoint.first;
        highest_config_checkpoint_hash = checkpoint.second;
      }
    }
    
    uint32_t highest_dns_checkpoint_height = 0;
    crypto::Hash highest_dns_checkpoint_hash = NULL_HASH;
    
    for (const auto& checkpoint : m_dns_checkpoint_hashes)
    {
      if (checkpoint.first > highest_dns_checkpoint_height)
      {
        highest_dns_checkpoint_height = checkpoint.first;
        highest_dns_checkpoint_hash = checkpoint.second;
      }
    }
    
    // Determine which checkpoints we can verify (must be <= current_blockchain_height)
    std::vector<std::pair<uint32_t, std::pair<crypto::Hash, std::string>>> checkpoints_to_verify;
    
    if (highest_config_checkpoint_height > 0 && highest_config_checkpoint_height <= current_blockchain_height)
    {
      checkpoints_to_verify.push_back(std::make_pair(highest_config_checkpoint_height, 
                                                      std::make_pair(highest_config_checkpoint_hash, "CryptoNoteConfig.h")));
    }
    
    // Only verify DNS checkpoint if it's higher than config checkpoint and we're synced to it
    if (highest_dns_checkpoint_height > highest_config_checkpoint_height && 
        highest_dns_checkpoint_height <= current_blockchain_height)
    {
      checkpoints_to_verify.push_back(std::make_pair(highest_dns_checkpoint_height,
                                                      std::make_pair(highest_dns_checkpoint_hash, "DNS")));
    }
    
    if (checkpoints_to_verify.empty())
    {
      // No checkpoints to verify (either none exist or we haven't synced to them yet)
      if (highest_config_checkpoint_height > current_blockchain_height)
      {
        logger(INFO) << "Highest checkpoint from CryptoNoteConfig.h (" << highest_config_checkpoint_height 
                     << ") is beyond current blockchain height (" << current_blockchain_height 
                     << "). Will validate when synced to that height.";
      }
      if (highest_dns_checkpoint_height > current_blockchain_height)
      {
        logger(INFO) << "Highest checkpoint from DNS (" << highest_dns_checkpoint_height 
                     << ") is beyond current blockchain height (" << current_blockchain_height 
                     << "). Will validate when synced to that height.";
      }
      return result;
    }
    
    // Validate each checkpoint we can verify
    for (const auto& checkpoint_entry : checkpoints_to_verify)
    {
      uint32_t checkpoint_height = checkpoint_entry.first;
      const crypto::Hash& expected_checkpoint_hash = checkpoint_entry.second.first;
      const std::string& source = checkpoint_entry.second.second;
      
      // SIMPLIFIED: Find which chunk this checkpoint belongs to
      // Formula: chunk_index = (height - 1) / chunk_size
      uint32_t chunk_index = (checkpoint_height - 1) / m_chunk_size;
      
      // Only validate if we have this chunk in checkpoint.dat
      if (chunk_index >= m_chunks.size())
      {
        logger(INFO) << "Checkpoint from " << source << " at height " 
                     << checkpoint_height << " (chunk " << chunk_index 
                     << ") is beyond our chunks (have " << m_chunks.size() 
                     << " chunks). Will validate when we reach this height.";
        continue; // Skip this checkpoint, try next one
      }
      
      logger(INFO) << "Validating checkpoint from " << source << " at height " 
                          << checkpoint_height << " (chunk " << chunk_index << ") against checkpoint.dat";
      
      // SIMPLIFIED: Compute chunk boundaries (all chunks uniform, block 0 excluded)
      uint32_t chunk_start_height = chunk_index * m_chunk_size + 1;
      uint32_t chunk_end_height = (chunk_index + 1) * m_chunk_size;
      uint32_t blocks_in_chunk = m_chunk_size;
      
      // Get block IDs for this chunk from blockchain
      std::vector<crypto::Hash> chunk_block_ids = getBlockIdsFunc(chunk_start_height, blocks_in_chunk);
      
      if (chunk_block_ids.size() != blocks_in_chunk)
      {
        logger(WARNING) << "Cannot validate chunk " << chunk_index 
                               << ": failed to get all block IDs (got " << chunk_block_ids.size() 
                               << ", expected " << blocks_in_chunk << ")";
        continue; // Skip this checkpoint, try next one
      }
      
      // Apply priority order: CryptoNoteConfig.h > DNS > blockchain.dat
      // Build a map of checkpoints to apply (same logic as chunk generation)
      std::map<uint32_t, std::pair<crypto::Hash, std::string>> checkpoints_to_apply;
      
      // PRIORITY 1: CryptoNoteConfig.h checkpoints in this chunk
      for (const auto& checkpoint : m_old_checkpoint_hashes)
      {
        uint32_t cp_height = checkpoint.first;
        if (cp_height >= chunk_start_height && cp_height <= chunk_end_height)
        {
          checkpoints_to_apply[cp_height] = std::make_pair(checkpoint.second, "CryptoNoteConfig.h");
        }
      }
      
      // PRIORITY 2: DNS checkpoints in this chunk (only if not in CryptoNoteConfig.h)
      for (const auto& checkpoint : m_dns_checkpoint_hashes)
      {
        uint32_t cp_height = checkpoint.first;
        if (cp_height >= chunk_start_height && cp_height <= chunk_end_height)
        {
          if (checkpoints_to_apply.find(cp_height) == checkpoints_to_apply.end())
          {
            checkpoints_to_apply[cp_height] = std::make_pair(checkpoint.second, "DNS");
          }
        }
      }
      
      // Apply checkpoints with validation
      for (const auto& cp_entry : checkpoints_to_apply)
      {
        uint32_t cp_height = cp_entry.first;
        const crypto::Hash& cp_hash = cp_entry.second.first;
        const std::string& cp_source = cp_entry.second.second;
        
        uint32_t index_in_chunk = cp_height - chunk_start_height;
        
        // Validate that blockchain.dat matches the checkpoint
        const crypto::Hash& blockchain_hash = chunk_block_ids[index_in_chunk];
        
        if (blockchain_hash != cp_hash)
        {
          logger(ERROR) << "CHECKPOINT VALIDATION FAILED: "
                                 << "Checkpoint from " << cp_source << " at height " << cp_height 
                                 << " in chunk " << chunk_index << " does not match blockchain.dat!"
                                 << " Expected: " << cp_hash
                                 << ", Got: " << blockchain_hash;
          result.is_valid = false;
          result.first_mismatched_chunk_index = chunk_index;
          return result;
        }
        
        // Replace with checkpoint hash (using priority order)
        chunk_block_ids[index_in_chunk] = cp_hash;
      }
      
      // Compute chunk hash using priority order
      crypto::Hash computed_chunk_hash = crypto::cn_fast_hash(
        chunk_block_ids.data(), 
        chunk_block_ids.size() * sizeof(crypto::Hash)
      );
      
      const crypto::Hash& stored_chunk_hash = m_chunks[chunk_index];
      
      if (computed_chunk_hash != stored_chunk_hash)
      {
        logger(ERROR) << "CHUNK VALIDATION FAILED: "
                               << "Chunk " << chunk_index << " (contains checkpoint from " << source 
                               << " at height " << checkpoint_height << ") hash mismatch!"
                               << " Computed (using priority: CryptoNoteConfig.h > DNS > blockchain.dat): " << computed_chunk_hash
                               << ", Stored in checkpoint.dat: " << stored_chunk_hash
                               << ". This indicates checkpoint.dat is out of sync.";
        result.is_valid = false;
        result.first_mismatched_chunk_index = chunk_index;
        return result;
      }
      
      logger(INFO) << "Successfully validated checkpoint from " << source 
                          << " at height " << checkpoint_height << " (chunk " << chunk_index 
                          << ") against checkpoint.dat";
    }
    
    return result;
  }
  
  /**
   * Truncate checkpoint.dat to a specific chunk index
   * 
   * This removes all chunks after last_valid_chunk_index from both the file and memory.
   * Used when validation fails and we need to rollback.
   * 
   * @param last_valid_chunk_index The last chunk index to keep (inclusive)
   * @return true if truncation was successful
   */
  bool CheckpointList::truncate_checkpoint_file(uint32_t last_valid_chunk_index)
  {
    std::vector<crypto::Hash> chunks_to_save;
    std::string file_to_save;
    uint32_t chunk_size;
    
    {
      const std::lock_guard<std::mutex> lock(m_chunks_lock);
      
      // Ensure we don't truncate beyond what we have
      if (last_valid_chunk_index >= m_chunks.size())
      {
        logger(WARNING) << "Cannot truncate to chunk " << last_valid_chunk_index 
                                 << ": only have " << m_chunks.size() << " chunks";
        return false;
      }
      
      // Calculate how many chunks we're removing (before truncating)
      uint32_t original_chunk_count = static_cast<uint32_t>(m_chunks.size());
      uint32_t chunks_to_remove = (last_valid_chunk_index < original_chunk_count) 
                                   ? (original_chunk_count - (last_valid_chunk_index + 1))
                                   : 0;
      
      // Truncate chunks in memory
      m_chunks.resize(last_valid_chunk_index + 1);
      
      // Update confirmed chunks (remove chunks beyond truncation point)
      std::unordered_set<uint32_t> new_confirmed_chunks;
      for (uint32_t chunk_index : m_confirmed_chunks)
      {
        if (chunk_index <= last_valid_chunk_index)
        {
          new_confirmed_chunks.insert(chunk_index);
        }
      }
      m_confirmed_chunks = std::move(new_confirmed_chunks);
      
      logger(WARNING) << "Truncating checkpoint.dat to chunk " << last_valid_chunk_index 
                               << " (removed " << chunks_to_remove << " chunks)";
      
      // Copy data before releasing lock
      chunks_to_save = m_chunks;
      file_to_save = m_save_file;
      chunk_size = m_chunk_size;
    } // Lock released here
    
    // Save truncated chunks to file
    bool success = save_checkpoints_impl(chunks_to_save, file_to_save, chunk_size);
    
    if (success)
    {
      uint32_t covered_height = ((last_valid_chunk_index + 1) * chunk_size) - 1;
      logger(INFO) << "Successfully truncated checkpoint.dat to chunk " 
                            << last_valid_chunk_index << " (covers up to height " << covered_height << ")";
    }
    
    return success;
  }
  
  /**
   * Verify chunk with peer consensus (with second chance retry)
   * 
   * This implements the "second chance" mechanism for noisy environments:
   * 1. First attempt: Sample M peers from K, check if all agree with local hash
   * 2. If mismatch: Re-sample a fresh M peers from K (second chance)
   * 3. If still mismatch: Consensus failed
   * 
   * This helps avoid rollback due to flaky peers in noisy network environments.
   * 
   * @param chunk_index The chunk index to verify
   * @param local_chunk_hash The locally computed chunk hash
   * @param getPeerChunkHashFunc Function to get chunk hash from a peer
   * @param available_peers List of available peer IDs that meet requirements
   * @param getPeerNetwork16Func Function to get /16 network for a peer
   * @return ConsensusResult with consensus status
   */
  CheckpointList::ConsensusResult CheckpointList::verify_chunk_with_peer_consensus(
    uint32_t chunk_index,
    const crypto::Hash& local_chunk_hash,
    std::function<crypto::Hash(uint64_t peer_id)> getPeerChunkHashFunc,
    const std::vector<uint64_t>& available_peers,
    std::function<uint32_t(uint64_t peer_id)> getPeerNetwork16Func) const
  {
    ConsensusResult result;
    result.consensus_reached = false;
    result.agreements_first_attempt = 0;
    result.agreements_second_attempt = 0;
    result.used_second_chance = false;
    
    if (available_peers.empty())
    {
      logger(WARNING) << "No available peers for chunk " << chunk_index << " consensus";
      return result;
    }
    
    // Calculate consensus requirements
    ConsensusRequirements req = calculate_consensus_requirements(available_peers.size());
    
    // Ensure we have enough peers
    if (available_peers.size() < req.min_peers)
    {
      logger(WARNING) << "Not enough peers for chunk " << chunk_index 
                               << " consensus (have " << available_peers.size() 
                               << ", need " << req.min_peers << ")";
      return result;
    }
    
    // Helper function to sample peers and check consensus
    auto attempt_consensus = [&](const std::string& attempt_name) -> uint32_t {
      // Randomly sample M peers from available peers, ensuring network diversity
      std::vector<uint64_t> sampled_peers;
      std::map<uint32_t, uint32_t> network_votes; // network_16 -> vote_count (capped at 1)
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<size_t> dis(0, available_peers.size() - 1);
      
      // Sample K peers (req.min_peers) with network diversity, need M agreements (req.min_agreements)
      while (sampled_peers.size() < req.min_peers)
      {
        // Try to find a peer from a different network
        size_t attempts = 0;
        const size_t max_attempts = available_peers.size() * 2; // Prevent infinite loop
        
        while (attempts < max_attempts && sampled_peers.size() < req.min_peers)
        {
          size_t idx = dis(gen);
          uint64_t peer_id = available_peers[idx];
          
          // Check if we already sampled this peer
          if (std::find(sampled_peers.begin(), sampled_peers.end(), peer_id) != sampled_peers.end())
          {
            attempts++;
            continue;
          }
          
          // Check network diversity (max 1 vote per /16 network)
          uint32_t net16 = getPeerNetwork16Func(peer_id);
          if (network_votes[net16] >= 1 && sampled_peers.size() < available_peers.size())
          {
            // Already have a vote from this network, try another peer
            attempts++;
            continue;
          }
          
          // Accept this peer
          sampled_peers.push_back(peer_id);
          network_votes[net16] = std::min(network_votes[net16] + 1, 1U);
          break;
        }
        
        // If we couldn't find enough diverse peers, relax diversity requirement
        if (sampled_peers.size() < req.min_peers && attempts >= max_attempts)
        {
          // Fall back to any available peer (diversity requirement relaxed)
          for (uint64_t peer_id : available_peers)
          {
            if (std::find(sampled_peers.begin(), sampled_peers.end(), peer_id) == sampled_peers.end())
            {
              sampled_peers.push_back(peer_id);
              if (sampled_peers.size() >= req.min_peers)
                break;
            }
          }
        }
      }
      
      // Check consensus: need M agreements from K sampled peers
      uint32_t agreements = 0;
      for (uint64_t peer_id : sampled_peers)
      {
        crypto::Hash peer_hash = getPeerChunkHashFunc(peer_id);
        
        if (peer_hash == NULL_HASH)
        {
          // Peer didn't respond, timed out, or doesn't have this chunk in memory
          // This is not necessarily a failure - the peer might not have created this chunk yet
          logger(INFO) << "Peer " << peer_id 
                                     << " returned NULL_HASH for chunk " << chunk_index 
                                     << " (" << attempt_name << ") - peer may not have this chunk in memory yet";
          continue; // Don't count as agreement or disagreement - peer doesn't have the chunk
        }
        
        if (peer_hash == local_chunk_hash)
        {
          agreements++;
          logger(INFO) << "Peer " << peer_id << " agrees with local chunk " << chunk_index 
                       << " hash (" << attempt_name << ")";
        }
        else
        {
          logger(WARNING) << "Peer " << peer_id 
                                   << " chunk hash mismatch for chunk " << chunk_index 
                                   << " (" << attempt_name << "): local=" << local_chunk_hash 
                                   << ", peer=" << peer_hash;
        }
      }
      
      // Verify we have at least n diverse networks
      uint32_t diverse_networks = static_cast<uint32_t>(network_votes.size());
      if (diverse_networks < req.min_diverse_networks)
      {
        logger(WARNING) << "Chunk " << chunk_index 
                                 << " consensus " << attempt_name << ": insufficient network diversity "
                                 << "(have " << diverse_networks << " networks, need n=" << req.min_diverse_networks << ")";
        // Return 0 agreements if diversity requirement not met (consensus fails)
        return 0;
      }
      
      logger(INFO) << "Chunk " << chunk_index 
                            << " consensus " << attempt_name << ": " << agreements 
                            << "/" << sampled_peers.size() << " peers agreed (need M=" 
                            << req.min_agreements << " from K=" << req.min_peers 
                            << ", have n=" << diverse_networks << " diverse networks)";
      
      return agreements;
    };
    
    // First attempt
    result.agreements_first_attempt = attempt_consensus("first attempt");
    
    if (result.agreements_first_attempt >= req.min_agreements)
    {
      // Consensus reached on first attempt
      result.consensus_reached = true;
      logger(INFO) << "Chunk " << chunk_index 
                            << " consensus reached on first attempt (" 
                            << result.agreements_first_attempt << " agreements)";
      return result;
    }
    
    // First attempt failed - use second chance
    logger(WARNING) << "Chunk " << chunk_index 
                             << " consensus failed on first attempt (" 
                             << result.agreements_first_attempt << " agreements, need " 
                             << req.min_agreements << "). Re-sampling peers for second chance...";
    
    result.used_second_chance = true;
    result.agreements_second_attempt = attempt_consensus("second attempt");
    
    if (result.agreements_second_attempt >= req.min_agreements)
    {
      // Consensus reached on second attempt
      result.consensus_reached = true;
      logger(INFO) << "Chunk " << chunk_index 
                            << " consensus reached on second attempt (" 
                            << result.agreements_second_attempt << " agreements)";
    }
    else
    {
      // Consensus failed even after second chance
      logger(ERROR) << "Chunk " << chunk_index 
                              << " consensus failed after second attempt (first: " 
                              << result.agreements_first_attempt << ", second: " 
                              << result.agreements_second_attempt << ", need " 
                              << req.min_agreements << "). Local blockchain may be wrong.";
    }
    
    return result;
  }
}

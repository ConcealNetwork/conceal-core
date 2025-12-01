// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once
#include <map>
#include <vector>
#include <unordered_set>
#include <functional>

#include "CryptoNoteBasicImpl.h"
#include <Logging/LoggerRef.h>
#include "../CryptoNoteConfig.h" // For P2P_CHECKPOINT_PEER_MIN_UPTIME_BLOCKS constants

namespace cn
{
  class CheckpointList
  {
  public:
    // Chunk size: number of blocks per checkpoint chunk
    // Mainnet: 10,000 blocks per chunk (matches checkpoint frequency in CryptoNoteConfig.h)
    // Testnet: 25,000 blocks per chunk (matches checkpoint frequency in CryptoNoteConfig.h)
    static constexpr uint32_t CHUNK_SIZE_MAINNET = 10000;
    static constexpr uint32_t CHUNK_SIZE_TESTNET = 25000;

    explicit CheckpointList(logging::ILogger& log) :  logger(log, "checkpoint_list"), m_chunk_size(CHUNK_SIZE_MAINNET) {}

    void init_targets(bool is_testnet, const std::string& save_file);
    
    // Convert old-style checkpoints (individual block hashes) to new-style (list hashes)
    // This allows the same checkpoint data in CryptoNoteConfig.h to work with both systems
    // getBlockIdsFunc: function that returns block IDs from genesis (0) to the specified height
    void convert_old_checkpoints_to_list_hashes(
      std::function<std::vector<crypto::Hash>(uint32_t startHeight, uint32_t maxCount)> getBlockIdsFunc);
  
    // Chunked checkpoint methods
    // Generate checkpoint chunks from block IDs (used during initialization)
    bool generate_chunks_from_block_ids(
      std::function<std::vector<crypto::Hash>(uint32_t startHeight, uint32_t maxCount)> getBlockIdsFunc,
      uint32_t max_height);
    
    // Add a single chunk (called when a new chunk_size blocks are added)
    // This computes the hash for the latest chunk and stores it in memory only
    // NOTE: Chunk is NOT saved to checkpoint.dat until peer consensus is reached
    // After peer consensus, call add_verified_chunk_to_file() to save it
    bool add_chunk_from_block_ids(
      std::function<std::vector<crypto::Hash>(uint32_t startHeight, uint32_t maxCount)> getBlockIdsFunc,
      uint32_t chunk_start_height);
    
    // Add a verified chunk to checkpoint.dat (marks it as confirmed)
    // This is called after peer consensus is reached (M out of K peers agree)
    // Adding to checkpoint.dat IS the confirmation - file only contains verified chunks
    bool add_verified_chunk_to_file(uint32_t chunk_index);
    
    // Get chunk hash for a specific chunk index
    crypto::Hash get_chunk_hash(uint32_t chunk_index) const;
    
    // Get all chunk hashes (for P2P comparison)
    std::vector<crypto::Hash> get_all_chunk_hashes() const;
    
    // Set chunk hashes (from P2P or file)
    bool set_chunk_hashes(std::vector<crypto::Hash>&& chunk_hashes);
    
    // Validate a specific chunk against expected hash
    bool validate_chunk(uint32_t chunk_index, const crypto::Hash& expected_hash) const;
    
    // Chunk confirmation methods
    // NOTE: Adding a chunk to checkpoint.dat IS the confirmation
    // mark_chunk_confirmed() is a convenience method that also saves to file
    // Peer consensus must include only peers with uptime > get_min_peer_uptime_blocks() (network-specific: 12k mainnet, 6k testnet)
    // and must sample from different buckets (seed, white list, different IP ranges/ASNs)
    void mark_chunk_confirmed(uint32_t chunk_index);
    
    // Check if a chunk is confirmed
    bool is_chunk_confirmed(uint32_t chunk_index) const;
    
    // Check if node can answer checkpoint requests (requires confirmed chunks)
    bool can_answer_checkpoint_requests() const;
    
    // Get the highest confirmed chunk index (for rollback safety)
    // Only confirmed chunks can be used as rollback anchors
    uint32_t get_highest_confirmed_chunk() const;
    
    // Get chunks that are in memory but not yet verified (not in checkpoint.dat)
    // These are chunks that need peer consensus before being added to file
    // Returns: vector of chunk indices that need verification
    std::vector<uint32_t> get_unverified_chunks() const;
    
    // Consensus requirements: Simple fixed M/K/n values
    // M = minimum agreements required (all M peers must agree)
    // K = total peers to sample
    // n = minimum distinct /16 networks required
    // 
    // USAGE IN P2P HANDLER:
    // 1. Count available peers with uptime > get_min_peer_uptime_blocks() (network-specific)
    // 2. Call calculate_consensus_requirements(available_peer_count) to get M, K, n
    // 3. Sample K peers ensuring:
    //    - At least n distinct /16 networks (use get_network_16)
    //    - Maximum 1 vote per /16 network per chunk (diversity capping)
    // 4. Build consensus: require M agreements from K sampled peers
    // 5. If consensus reached, call mark_chunk_confirmed()
    struct ConsensusRequirements {
      uint32_t min_agreements;        // M: minimum peers that must agree
      uint32_t min_peers;              // K: total peers to sample
      uint32_t min_diverse_networks;   // n: minimum distinct /16 networks required
    };
    ConsensusRequirements calculate_consensus_requirements(size_t available_peers) const;
    
    // Get minimum peer uptime requirement for checkpoint verification (network-specific)
    // Mainnet: 12,000 blocks (~16.7 days)
    // Testnet: 6,000 blocks (~8.3 days, relaxed for smaller network)
    uint32_t get_min_peer_uptime_blocks() const
    {
      return m_testnet ? cn::P2P_CHECKPOINT_PEER_MIN_UPTIME_BLOCKS_TESTNET 
                       : cn::P2P_CHECKPOINT_PEER_MIN_UPTIME_BLOCKS;
    }
    
    // Network diversity tracking for consensus
    // Extract /16 network from IP address (for diversity capping)
    // 
    // USAGE: When building consensus:
    //   std::map<uint32_t, uint32_t> network_votes; // network_16 -> vote_count
    //   for (each peer response) {
    //     uint32_t net16 = CheckpointList::get_network_16(peer_ip);
    //     network_votes[net16] = std::min(network_votes[net16] + 1, 1U); // Cap at 1 vote per /16
    //   }
    static uint32_t get_network_16(uint32_t ip);
    
    // Health metrics for monitoring
    struct HealthMetrics {
      uint32_t total_chunks;
      uint32_t confirmed_chunks;
      double confirmation_ratio;        // confirmed / total
      uint32_t highest_confirmed;
      uint32_t highest_total;
      uint32_t unconfirmed_count;       // chunks beyond hardcoded checkpoints
    };
    HealthMetrics get_health_metrics() const;
    
    // Validate chunks in checkpoint.dat against checkpoints from CryptoNoteConfig.h
    // OPTIMIZATION: Only validates the LAST (highest) checkpoint for fast startup validation
    // Previous checkpoints were already validated when they were added
    // This is called on startup to detect if maintainer added new checkpoints
    // Returns: {is_valid, first_mismatched_chunk_index}
    // If invalid, rollback blockchain and truncate checkpoint.dat to last_valid_chunk_index
    struct ValidationResult {
      bool is_valid;
      uint32_t first_mismatched_chunk_index;  // Only valid if is_valid == false
    };
    ValidationResult validate_chunks_against_checkpoints(
      std::function<std::vector<crypto::Hash>(uint32_t startHeight, uint32_t maxCount)> getBlockIdsFunc,
      uint32_t current_blockchain_height) const;
    
    // Truncate checkpoint.dat to a specific chunk index (removes all chunks after this index)
    // Used when validation fails and we need to rollback
    // @param last_valid_chunk_index The last chunk index to keep (inclusive)
    // @return true if truncation was successful
    bool truncate_checkpoint_file(uint32_t last_valid_chunk_index);
    
    // Verify chunk with peer consensus (with second chance retry)
    // This implements the "second chance" mechanism: if first attempt fails, re-sample peers once
    // 
    // @param chunk_index The chunk index to verify
    // @param local_chunk_hash The locally computed chunk hash
    // @param getPeerChunkHashFunc Function to get chunk hash from a peer (peer_id -> chunk_hash or NULL_HASH if failed)
    // @param available_peers List of available peer IDs that meet requirements (uptime > 12k blocks, etc.)
    // @param getPeerNetwork16Func Function to get /16 network for a peer (peer_id -> network_16)
    // @return ConsensusResult with consensus status and details
    struct ConsensusResult {
      bool consensus_reached;           // true if M peers agreed
      uint32_t agreements_first_attempt;  // Number of agreements in first attempt
      uint32_t agreements_second_attempt; // Number of agreements in second attempt (if retried)
      bool used_second_chance;          // true if second attempt was made
      crypto::Hash consensus_hash_first_attempt;   // Hash that M/K peers agreed on in attempt 1 (if different from local, NULL_HASH otherwise)
      crypto::Hash consensus_hash_second_attempt; // Hash that M/K peers agreed on in attempt 2 (if different from local, NULL_HASH otherwise)
    };
    ConsensusResult verify_chunk_with_peer_consensus(
      uint32_t chunk_index,
      const crypto::Hash& local_chunk_hash,
      std::function<crypto::Hash(uint64_t peer_id)> getPeerChunkHashFunc,
      const std::vector<uint64_t>& available_peers,
      std::function<uint32_t(uint64_t peer_id)> getPeerNetwork16Func) const;
    
    // Legacy methods (for backward compatibility during transition)
    bool add_checkpoint_list(uint32_t start_height, std::vector<crypto::Hash>& points);
    bool set_checkpoint_list(std::vector<crypto::Hash>&& points);
    bool load_checkpoints_from_file();
    
    // Test helper to add checkpoint targets (for unit testing only)
    bool add_checkpoint_target_for_test(uint32_t height, const std::string& hash_str);

    // Get the number of chunks (not individual blocks)
    uint32_t get_chunk_count() const
    {
      const std::lock_guard<std::mutex> lock(m_chunks_lock);
      return static_cast<uint32_t>(m_chunks.size());
    }
    
    // Get the highest block height covered by chunks
    uint32_t get_covered_height() const
    {
      const std::lock_guard<std::mutex> lock(m_chunks_lock);
      if (m_chunks.empty())
        return 0;
      
      // SIMPLIFIED: All chunks are uniform (chunk_size blocks each)
      // Block 0 (genesis) is NOT included in chunks
      // chunk[0]: covers blocks 1 to chunk_size
      // chunk[1]: covers blocks (chunk_size + 1) to (2 * chunk_size)
      // chunk[n]: covers blocks (n * chunk_size + 1) to ((n + 1) * chunk_size)
      // 
      // Formula: covered_height = num_chunks * chunk_size
      uint32_t num_chunks = static_cast<uint32_t>(m_chunks.size());
      return num_chunks * m_chunk_size;
    }
    
    // Get chunk size (blocks per chunk)
    uint32_t get_chunk_size() const
    {
      return m_chunk_size;
    }
    
    // Legacy method for backward compatibility
    uint32_t get_points_size() const
    {
      // Return covered height + 1 (number of blocks, not chunks)
      return get_covered_height() + 1;
    }

    uint32_t get_greatest_target_height() const
    {
      if (m_targets.empty())
        return 0;
      return m_targets.rbegin()->first - 1;
    }

    bool is_ready() const
    { 
      // Check if we have chunks covering up to the greatest target height
      return get_covered_height() >= get_greatest_target_height();
    }

    bool is_in_checkpoint_zone(uint32_t height) const
    {
      // A height is in checkpoint zone if we don't have chunks covering it yet
      return get_covered_height() < height;
    }

    enum check_rt
    {
      is_out_of_zone,
      is_in_zone_failed,
      is_checkpointed
    };

    // Check if a block at given height matches the checkpoint
    // 
    // BACKWARD COMPATIBILITY: This method supports both old (version 1) and new (version 2+) checkpoint systems:
    // - Version 1 (old): Validates individual blocks against hardcoded checkpoints from CryptoNoteConfig.h
    //   Uses m_old_checkpoint_hashes to verify block hash matches expected checkpoint hash
    // - Version 2+ (chunked): Uses chunk-based validation
    //   Individual block validation isn't possible with chunks alone, but chunk integrity ensures chain correctness
    check_rt check_checkpoint(uint32_t height, const crypto::Hash& hv) const
    {
      // FIRST: Check if this height is a hardcoded checkpoint (from CryptoNoteConfig.h)
      // This works for BOTH version 1 and version 2 systems
      // m_old_checkpoint_hashes contains individual block hashes for each checkpoint height
      // This is read-only access, so no lock needed (m_old_checkpoint_hashes is only written during init)
      {
        const std::lock_guard<std::mutex> lock(m_chunks_lock); // Use existing lock for thread safety
        auto old_checkpoint_it = m_old_checkpoint_hashes.find(height);
        if (old_checkpoint_it != m_old_checkpoint_hashes.end())
        {
          // This is a checkpoint height - validate the block hash matches the expected checkpoint
          const crypto::Hash& expected_hash = old_checkpoint_it->second;
          if (hv != expected_hash)
          {
            // Block hash doesn't match hardcoded checkpoint - validation failed
            logger(logging::ERROR) << "<< CheckpointList.cpp << " << "Checkpoint validation FAILED at height " 
                                   << height << "! Expected (from CryptoNoteConfig.h): " << expected_hash
                                   << ", Got (from block): " << hv;
            return is_in_zone_failed;
          }
          // Block hash matches checkpoint - validation passed
          return is_checkpointed;
        }
        
        // Not a checkpoint height - check if we're in checkpoint zone
        // For version 2+ (chunked system), we check if chunks cover this height
        // For version 1 (old system), we check if we have checkpoint list covering this height
        
        // If we have chunks, use chunk-based zone checking
        if (!m_chunks.empty())
        {
          // SIMPLIFIED: Calculate which chunk this height belongs to
          // Block 0 (genesis) is not in any chunk, so we need to handle it separately
          if (height == 0)
            return is_checkpointed; // Genesis is always valid (checked separately) in blockchain.cpp line 847
          
          // For heights >= 1: chunk_index = (height - 1) / chunk_size
          uint32_t chunk_index = (height - 1) / m_chunk_size;
          
          // If we don't have chunks covering this height, it's out of checkpoint zone
          if (chunk_index >= m_chunks.size())
            return is_out_of_zone;
          
          // We have chunks covering this height
          // Individual block validation isn't possible with chunks alone,
          // but chunk integrity ensures the chain is correct
          return is_checkpointed;
        }
      }
      
      // Version 1 (old system): Check if we have checkpoint list covering this height
      {
        const std::lock_guard<std::mutex> lock(m_points_lock);
        if (m_points.size() > height)
        {
          // We have checkpoint list covering this height
          return is_checkpointed;
        }
      }
      
      // No checkpoint coverage for this height
      return is_out_of_zone;
    }
    
    std::vector<std::pair<uint32_t, crypto::Hash>> get_checkpoint_targets() const
    {
      std::vector<std::pair<uint32_t, crypto::Hash>> rv;
      rv.reserve(m_targets.size());
      for(auto it = m_targets.rbegin(); it != m_targets.rend(); ++it)
        rv.emplace_back(it->first-1, it->second);
      return rv;
    }

    // Legacy method for old checkpoint list system (backward compatibility)
    struct t_get_incomplete_checkpoint_target_rv {
      crypto::Hash target_hash = NULL_HASH;
      uint32_t start_height;
      uint32_t end_height;
    };

    t_get_incomplete_checkpoint_target_rv get_incomplete_checkpoint_target() const
    {
      t_get_incomplete_checkpoint_target_rv rv;
      
      // REFACTORED: Now works with chunks instead of full checkpoint lists
      // Get current coverage from chunks
      uint32_t covered_height = get_covered_height();
      uint32_t covered_size = covered_height + 1; // Convert height to size
      
      if (m_targets.empty())
        return rv;
      
      uint32_t greatest_target_size = m_targets.rbegin()->first;
      
      // If we already have chunks covering the greatest target, nothing needed
      if(covered_size >= greatest_target_size)
        return rv;
      
      // Find the first target we don't have chunks for
      rv.start_height = 0;
      for(const auto& p : m_targets)
      {
        if(covered_size < p.first)
        {
          rv.end_height = p.first - 1; // Convert size to height
          rv.target_hash = p.second;
          return rv;
        }
        else
        {
          rv.start_height = p.first - 1; // Convert size to height
        }
      }
      return rv;
    }
    
    // New method for chunk-based system: returns which chunks we need
    // Returns vector of chunk indices that we don't have yet
    std::vector<uint32_t> get_incomplete_chunks() const
    {
      std::vector<uint32_t> incomplete_chunks;
      
      const std::lock_guard<std::mutex> lock(m_chunks_lock);
      
      // Get current coverage
      uint32_t covered_height = get_covered_height();
      uint32_t greatest_target_height = get_greatest_target_height();
      
      // If we have chunks covering all targets, nothing needed
      if (covered_height >= greatest_target_height)
        return incomplete_chunks;
      
      // Calculate which chunks we need
      // Start from the first chunk we don't have, up to the chunk containing the greatest target
      uint32_t start_chunk_index = height_to_chunk_index(covered_height + 1);
      uint32_t end_chunk_index = height_to_chunk_index(greatest_target_height);
      
      for (uint32_t chunk_index = start_chunk_index; chunk_index <= end_chunk_index; chunk_index++)
      {
        // Check if we have this chunk
        if (chunk_index >= m_chunks.size() || m_chunks[chunk_index] == NULL_HASH)
        {
          incomplete_chunks.push_back(chunk_index);
        }
      }
      
      return incomplete_chunks;
    }

    
  private:
    bool m_testnet;
    logging::LoggerRef logger;
    std::string m_save_file;
    uint32_t m_chunk_size; // Blocks per chunk (CHUNK_SIZE_MAINNET or CHUNK_SIZE_TESTNET)

    // Chunked checkpoint storage: each element is the hash of a chunk of blocks
    // SIMPLIFIED: Block 0 (genesis) is NOT included in chunks
    // chunk[0] = hash(blocks 1 to chunk_size)
    // chunk[1] = hash(blocks chunk_size+1 to 2*chunk_size)
    // chunk[n] = hash(blocks n*chunk_size+1 to (n+1)*chunk_size)
    mutable std::mutex m_chunks_lock;
    std::vector<crypto::Hash> m_chunks; // Chunk hashes (one per chunk_size blocks)
    
    // Chunk confirmation tracking: only confirmed chunks can be used for rollback
    // and for answering checkpoint requests
    // Key: chunk_index, Value: true if confirmed via peer consensus
    std::unordered_set<uint32_t> m_confirmed_chunks;
    
    // Legacy storage for backward compatibility (will be removed after migration)
    mutable std::mutex m_points_lock;
    std::vector<crypto::Hash> m_points; // Full block hash list (deprecated, kept for transition)
    
    // Validation targets: expected hashes at specific checkpoint heights
    // Key is height+1 (size), value is the expected hash of blocks 0 to height
    std::map<uint32_t, crypto::Hash> m_targets;
    std::unordered_set<uint32_t> m_valid_point_sizes; // Valid checkpoint sizes (heights+1)
    
    // Old checkpoint block hashes (individual block hashes from CryptoNoteConfig.h)
    // Used for validation during chunk generation to ensure backward compatibility
    // Key is height, value is the expected block hash at that height
    // PRIORITY 1: Highest priority - hardcoded in source code
    std::map<uint32_t, crypto::Hash> m_old_checkpoint_hashes;
    
    // DNS checkpoint block hashes (individual block hashes from DNS TXT records)
    // Used as secondary trusted source during chunk generation
    // Key is height, value is the expected block hash at that height
    // PRIORITY 2: Secondary priority - fetched from DNS (trusted but can be updated)
    std::map<uint32_t, crypto::Hash> m_dns_checkpoint_hashes;
    
    // Consensus configuration (defined in CryptoNoteConfig.h)
    // Using constants from cn namespace for network-wide configuration

    // Save/load chunked checkpoints
    bool save_checkpoints();
    bool save_checkpoints_impl(const std::vector<crypto::Hash>& chunks, const std::string& file_path, uint32_t chunk_size); // Private: doesn't lock
    bool load_checkpoints_from_file_chunked();
    
    // Legacy save/load (for backward compatibility)
    bool save_checkpoints_legacy();
    bool load_checkpoints_from_file_legacy();
    
    // Migration: Convert legacy format (individual block hashes) to chunked format
    bool convert_legacy_to_chunked_format();
    
    bool add_checkpoint_target(uint32_t height, const std::string &hash_str);
    
    // Helper: Calculate which chunk a block height belongs to
    // SIMPLIFIED: All chunks are uniform (block 0/genesis excluded)
    // chunk[0]: blocks 1 to chunk_size (inclusive)
    // chunk[1]: blocks (chunk_size + 1) to (2 * chunk_size) (inclusive)
    // chunk[n]: blocks (n * chunk_size + 1) to ((n + 1) * chunk_size) (inclusive)
    // 
    // Formula: chunk_index = (height - 1) / chunk_size
    uint32_t height_to_chunk_index(uint32_t height) const
    {
      return (height - 1) / m_chunk_size;
    }
    
    // Helper: Calculate the start height of a chunk
    // SIMPLIFIED: All chunks start at (chunk_index * chunk_size + 1)
    // chunk[0]: starts at 1
    // chunk[1]: starts at chunk_size + 1
    // chunk[n]: starts at n * chunk_size + 1
    // 
    // Formula: start_height = chunk_index * chunk_size + 1
    uint32_t chunk_index_to_start_height(uint32_t chunk_index) const
    {
      return chunk_index * m_chunk_size + 1;
    }
    
    // Legacy validation (for backward compatibility during transition)
    bool is_fsize_valid(uint32_t fsize)
    {
      if(fsize % sizeof(crypto::Hash) != 0)
        return false;
      fsize /= sizeof(crypto::Hash);
      
      return m_valid_point_sizes.find(fsize) != m_valid_point_sizes.end();
    }
  };
}

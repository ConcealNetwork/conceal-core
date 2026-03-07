// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <vector>
#include <atomic>

#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "CryptoNoteCore/CheckpointList.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/ICore.h"
#include "P2p/NetNodeCommon.h"
#include <Logging/LoggerRef.h>

namespace cn
{

  /**
   * ChunkValidationManager
   * 
   * Handles asynchronous chunk validation via P2P consensus.
   * Manages pending chunk hash requests, responses, and consensus checking.
   */
  class ChunkValidationManager
  {
  public:
    ChunkValidationManager(ICore& core, 
                          IP2pEndpoint* p2p,
                          const Currency& currency,
                          logging::ILogger& log,
                          std::atomic<uint64_t>& peersCount,
                          std::atomic<bool>& stop);
    
    // Update P2P endpoint (called when endpoint is set/updated)
    void set_p2p_endpoint(IP2pEndpoint* p2p);
    
    // Validate unverified chunks in chronological order (oldest first)
    // This ensures we catch divergences at the root cause and rollback to the correct point
    // Only validates when peers meet uptime requirements
    // @return true if validation was attempted (even if it failed)
    bool validate_unverified_chunks();
    
    // Check pending chunk validations for consensus (called periodically)
    // Checks if we have M identical hashes within the time window (3 minutes)
    // If consensus reached, processes it; if timeout, schedules retry
    void check_pending_chunk_validations();
    
    // Send chunk hash request asynchronously (non-blocking)
    // Returns true if request was sent successfully
    bool send_chunk_hash_request_async(uint64_t peer_id, uint32_t chunk_index);
    
    // Store chunk hash response (called from message handler)
    void store_chunk_hash_response(uint64_t peer_id, uint32_t chunk_index, const crypto::Hash& hash);
    
    // Check if chunk is currently being validated
    bool is_chunk_being_validated(uint32_t chunk_index) const;

  private:
    // Pending chunk hash responses: (peer_id, chunk_index) -> (hash, timestamp)
    // Used for asynchronous consensus mechanism
    struct ChunkHashResponse {
      crypto::Hash hash;
      uint64_t timestamp;  // When response was received
    };
    
    // Pending chunk validation attempts: chunk_index -> validation state
    struct PendingChunkValidation {
      uint32_t chunk_index;
      uint64_t request_timestamp;      // When requests were sent
      uint64_t attempt_start_time;     // When this attempt started
      uint32_t attempt_number;          // 1 or 2
      std::vector<uint64_t> requested_peers;  // Peers we requested from
      crypto::Hash local_hash;
      bool is_first_attempt;
    };
    
    ICore& m_core;
    IP2pEndpoint* m_p2p;
    const Currency& m_currency;
    logging::LoggerRef logger;
    std::atomic<uint64_t>& m_peersCount;
    std::atomic<bool>& m_stop;
    
    // Pending chunk hash responses: (peer_id, chunk_index) -> (hash, timestamp)
    mutable std::mutex m_pending_chunk_hashes_mutex;
    std::map<std::pair<uint64_t, uint32_t>, ChunkHashResponse> m_pending_chunk_hashes;
    
    // Pending chunk validation attempts: chunk_index -> validation state
    mutable std::mutex m_pending_validations_mutex;
    std::map<uint32_t, PendingChunkValidation> m_pending_validations;  // chunk_index -> validation state
    
    // Chunk validation state: track which chunk we're currently validating
    // This prevents duplicate validation attempts and ensures chronological order
    mutable std::mutex m_chunk_validation_mutex;
    uint32_t m_current_validating_chunk_index;  // Currently validating chunk (or UINT32_MAX if none)
    uint64_t m_last_chunk_validation_attempt;  // Last time we attempted validation (to avoid spamming)
  };

} // namespace cn


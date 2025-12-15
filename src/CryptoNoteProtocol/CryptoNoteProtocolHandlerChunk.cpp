// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "CryptoNoteProtocolHandlerChunk.h"

#include <algorithm>
#include <map>
#include <random>
#include <set>
#include <unordered_map>
#include <climits>
#include <ctime>

#include <boost/functional/hash.hpp>

#include "CryptoNoteCore/CheckpointList.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolDefinitions.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "P2p/ConnectionContext.h"
#include "P2p/LevinProtocol.h"

using namespace logging;
using namespace common;

namespace cn
{

namespace
{
  // Helper template function to send notifications (same as in CryptoNoteProtocolHandler.cpp)
  template <class t_parametr>
  bool post_notify(IP2pEndpoint &p2p, typename t_parametr::request &arg, const CryptoNoteConnectionContext &context)
  {
    return p2p.invoke_notify_to_peer(t_parametr::ID, LevinProtocol::encode(arg), context);
  }
} // namespace

ChunkValidationManager::ChunkValidationManager(ICore& core, 
                                                IP2pEndpoint* p2p,
                                                const Currency& currency,
                                                logging::ILogger& log,
                                                std::atomic<uint64_t>& peersCount,
                                                std::atomic<bool>& stop)
  : m_core(core)
  , m_p2p(p2p)
  , m_currency(currency)
  , logger(log, "chunk_validation")
  , m_peersCount(peersCount)
  , m_stop(stop)
  , m_current_validating_chunk_index(UINT32_MAX)
  , m_last_chunk_validation_attempt(0)
{
}

void ChunkValidationManager::set_p2p_endpoint(IP2pEndpoint* p2p)
{
  if (p2p)
  {
    m_p2p = p2p;
    logger(INFO) << "Updated P2P endpoint in ChunkValidationManager";
  }
  else
  {
    logger(WARNING) << "Attempted to set null P2P endpoint in ChunkValidationManager";
  }
}

bool ChunkValidationManager::send_chunk_hash_request_async(uint64_t peer_id, uint32_t chunk_index)
{
  // Find the peer connection
  CryptoNoteConnectionContext* peer_context = nullptr;
  m_p2p->for_each_connection([&peer_context, peer_id](CryptoNoteConnectionContext& ctx, uint64_t id) {
    if (id == peer_id) {
      peer_context = &ctx;
    }
  });
  
  if (!peer_context) {
    logger(WARNING) << "Cannot send async chunk hash request to peer " << peer_id << ": peer not found";
    return false;
  }
  
  // Send request (non-blocking)
  NOTIFY_REQUEST_CHUNK_HASH::request req;
  req.chunk_index = chunk_index;
  
  logger(INFO) << "Sending async chunk hash request for chunk " << chunk_index << " to peer " << peer_id 
               << " " << *peer_context;
  
  bool sent = post_notify<NOTIFY_REQUEST_CHUNK_HASH>(*m_p2p, req, *peer_context);
  if (!sent) {
    logger(WARNING) << "Failed to send async chunk hash request to peer " << peer_id << " " << *peer_context 
                    << " for chunk " << chunk_index;
    return false;
  }
  
  logger(INFO) << "Successfully sent async chunk hash request for chunk " << chunk_index 
               << " to peer " << peer_id << " " << *peer_context;
  return true;
}

void ChunkValidationManager::store_chunk_hash_response(uint64_t peer_id, uint32_t chunk_index, const crypto::Hash& hash)
{
  uint64_t response_time = time(nullptr);
  {
    std::lock_guard<std::mutex> lock(m_pending_chunk_hashes_mutex);
    auto key = std::make_pair(peer_id, chunk_index);
    ChunkHashResponse response;
    response.hash = hash;
    response.timestamp = response_time;
    m_pending_chunk_hashes[key] = response;
  }
}

bool ChunkValidationManager::is_chunk_being_validated(uint32_t chunk_index) const
{
  std::lock_guard<std::mutex> lock(m_chunk_validation_mutex);
  return (m_current_validating_chunk_index == chunk_index);
}

bool ChunkValidationManager::validate_unverified_chunks()
{
  // Check if we have unverified chunks first (fast check)
  std::vector<uint32_t> unverified_chunks = m_core.getCheckpointList().get_unverified_chunks();
  if (unverified_chunks.empty())
  {
    return false; // No unverified chunks - nothing to validate
  }
  
  // We need at least some peers to validate (but don't require full synchronization)
  // Validation can happen during sync as long as we have peers
  if (m_peersCount.load() == 0)
  {
    logger(DEBUGGING) << "Cannot validate chunks: no peers connected yet";
    return false;
  }
  
  // Rate limit: don't attempt validation more than once every P2P_CHECKPOINT_LIST_RE_REQUEST seconds (5 minutes)
  // EXCEPTION: Allow immediate attempt if this is the first time we have unverified chunks
  // (bypass rate limit on first attempt after chunks are created)
  uint64_t time_now = time(nullptr);
  bool bypass_rate_limit = false;
  {
    std::lock_guard<std::mutex> lock(m_chunk_validation_mutex);
    
    // Check if this is the first validation attempt (m_last_chunk_validation_attempt == 0)
    // or if enough time has passed
    uint64_t time_since_last = time_now - m_last_chunk_validation_attempt;
    
    if (m_last_chunk_validation_attempt == 0)
    {
      // First validation attempt - allow it immediately
      bypass_rate_limit = true;
      logger(INFO) << "First chunk validation attempt - bypassing rate limit";
    }
    else if (time_since_last < cn::P2P_CHECKPOINT_LIST_RE_REQUEST)
    {
      // Too soon since last attempt
      logger(DEBUGGING) << "Chunk validation rate limited: " 
                        << (cn::P2P_CHECKPOINT_LIST_RE_REQUEST - time_since_last) 
                        << " seconds remaining";
      return false;
    }
    
    m_last_chunk_validation_attempt = time_now;
  }
  
  // Sort to ensure chronological order (oldest first)
  std::sort(unverified_chunks.begin(), unverified_chunks.end());
  
  logger(INFO) << "Attempting to validate " << unverified_chunks.size() 
                        << " unverified chunk(s) via P2P consensus";
  
  // Check if we're already validating a chunk
  {
    std::lock_guard<std::mutex> lock(m_chunk_validation_mutex);
    if (m_current_validating_chunk_index != UINT32_MAX)
    {
      // Already validating a chunk - wait for it to complete
      return false;
    }
  }
  
  // Get current blockchain height to calculate peer uptime in blocks
  uint32_t current_height;
  crypto::Hash blockId;
  m_core.get_blockchain_top(current_height, blockId);
  uint32_t block_time = m_currency.difficultyTarget(); // Block time in seconds (120 for mainnet/testnet)
  uint32_t min_uptime_blocks = m_core.getCheckpointList().get_min_peer_uptime_blocks();
  
  // Find eligible peers (meet uptime requirement and support chunk-based checkpoints)
  std::vector<uint64_t> eligible_peers;
  std::map<uint64_t, uint32_t> peer_network_16; // peer_id -> /16 network
  
  // First, log all connected peers for debugging
  uint32_t total_connected = 0;
  m_p2p->for_each_connection([&](CryptoNoteConnectionContext& ctx, uint64_t peer_id) {
    total_connected++;
  });
  logger(INFO) << "Checking " << total_connected << " connected peer(s) for chunk validation eligibility";
  
  m_p2p->for_each_connection([&](CryptoNoteConnectionContext& ctx, uint64_t peer_id) {
    // Only consider peers that support chunk-based checkpoints
    if (ctx.version < cn::P2P_CHECKPOINT_LIST_VERSION)
    {
      logger(DEBUGGING) << "Peer " << peer_id << " " << ctx << " filtered: version " 
                        << static_cast<int>(ctx.version) << " < " << cn::P2P_CHECKPOINT_LIST_VERSION;
      return; // Skip old version peers
    }
    
    // Accept peers in normal state (synchronized) OR synchronizing state
    // We can validate chunks even during sync, as long as peers are connected
    if (ctx.m_state != CryptoNoteConnectionContext::state_normal && 
        ctx.m_state != CryptoNoteConnectionContext::state_synchronizing)
    {
      logger(DEBUGGING) << "Peer " << peer_id << " " << ctx << " filtered: state " 
                        << get_protocol_state_string(ctx.m_state) << " (need normal or synchronizing)";
      return; // Skip peers that aren't in a usable state
    }
    
    // Calculate peer uptime in blocks
    // Uptime = (current_time - connection_start_time) / block_time
    // This is an approximation - ideally we'd track peer's blockchain height when they first connected
    time_t connection_duration = time_now - ctx.m_started;
    if (connection_duration < 0)
    {
      logger(DEBUGGING) << "Peer " << peer_id << " " << ctx << " filtered: invalid connection time";
      return; // Invalid connection time
    }
    
    uint32_t peer_uptime_blocks = static_cast<uint32_t>(connection_duration / block_time);
    
    // Check if peer meets minimum uptime requirement
    if (peer_uptime_blocks < min_uptime_blocks)
    {
      logger(DEBUGGING) << "Peer " << peer_id << " " << ctx << " filtered: uptime " 
                        << peer_uptime_blocks << " blocks < " << min_uptime_blocks << " blocks required";
      return; // Peer doesn't meet uptime requirement
    }
    
    // Peer is eligible
    logger(DEBUGGING) << "Peer " << peer_id << " " << ctx << " is ELIGIBLE: version " 
                      << static_cast<int>(ctx.version) << ", state " 
                      << get_protocol_state_string(ctx.m_state) << ", uptime " 
                      << peer_uptime_blocks << " blocks";
    eligible_peers.push_back(peer_id);
    peer_network_16[peer_id] = CheckpointList::get_network_16(ctx.m_remote_ip);
  });
  
  if (eligible_peers.empty())
  {
    logger(INFO) << "No eligible peers for chunk validation (need uptime > " 
                               << min_uptime_blocks << " blocks, version 2+, and in normal/synchronizing state)";
    logger(INFO) << "Note: Only ACTIVE CONNECTIONS are considered, not peers in peerlist. "
                 << "Use 'print_cn' command to see active connections. "
                 << "To force connection to a peer, use --add-priority-node <IP>:<PORT>";
    logger(INFO) << "Chunk validation will be retried when eligible peers become available";
    return false;
  }
  
  logger(INFO) << "Found " << eligible_peers.size() << " eligible peer(s) for chunk validation "
                        << "(uptime > " << min_uptime_blocks << " blocks, support version 2+)";
  
  // Validate chunks in chronological order (oldest first)
  // This ensures we catch divergences at the root cause
  for (uint32_t chunk_index : unverified_chunks)
  {
    // Check if we should stop (node shutting down)
    if (m_stop.load())
    {
      break;
    }
    
    // Mark this chunk as being validated
    {
      std::lock_guard<std::mutex> lock(m_chunk_validation_mutex);
      if (m_current_validating_chunk_index != UINT32_MAX)
      {
        // Another thread started validation
        break;
      }
      m_current_validating_chunk_index = chunk_index;
    }
    
    // Get local chunk hash
    crypto::Hash local_chunk_hash = m_core.getCheckpointList().get_chunk_hash(chunk_index);
    if (local_chunk_hash == NULL_HASH)
    {
      logger(WARNING) << "Cannot validate chunk " << chunk_index << ": chunk hash not found in memory";
      {
        std::lock_guard<std::mutex> lock(m_chunk_validation_mutex);
        m_current_validating_chunk_index = UINT32_MAX;
      }
      continue;
    }
    
    logger(INFO) << "Validating chunk " << chunk_index << " (oldest unverified chunk) "
                          << "with " << eligible_peers.size() << " eligible peer(s)";
    
    // Check if this chunk is already being validated asynchronously
    {
      std::lock_guard<std::mutex> lock(m_pending_validations_mutex);
      if (m_pending_validations.find(chunk_index) != m_pending_validations.end())
      {
        logger(DEBUGGING) << "Chunk " << chunk_index << " is already being validated asynchronously, skipping";
        {
          std::lock_guard<std::mutex> lock2(m_chunk_validation_mutex);
          m_current_validating_chunk_index = UINT32_MAX;
        }
        continue;
      }
    }
    
    // Calculate consensus requirements (M, K, n)
    CheckpointList::ConsensusRequirements req = m_core.getCheckpointList().calculate_consensus_requirements(eligible_peers.size());
    logger(INFO) << "Consensus requirements (testnet): M=" << req.min_agreements 
                 << ", K=" << req.min_peers << ", n=" << req.min_diverse_networks 
                 << " (have " << eligible_peers.size() << " available peers)";
    
    // Sample K peers with network diversity using shared utility function
    auto getPeerNetwork16 = [&peer_network_16](uint64_t peer_id) -> uint32_t {
      auto it = peer_network_16.find(peer_id);
      return (it != peer_network_16.end()) ? it->second : 0;
    };
    
    CheckpointList::PeerSamplingResult sampling_result = CheckpointList::sample_peers_with_diversity(
      eligible_peers,
      getPeerNetwork16,
      req.min_peers);
    
    const std::vector<uint64_t>& sampled_peers = sampling_result.sampled_peers;
    const std::map<uint32_t, uint32_t>& network_votes = sampling_result.network_votes;
    
    if (sampled_peers.empty())
    {
      logger(WARNING) << "Could not sample any peers for chunk " << chunk_index << " validation";
      {
        std::lock_guard<std::mutex> lock(m_chunk_validation_mutex);
        m_current_validating_chunk_index = UINT32_MAX;
      }
      continue;
    }
    
    // Verify we have at least n distinct networks (network diversity requirement)
    uint32_t distinct_networks = static_cast<uint32_t>(network_votes.size());
    
    if (distinct_networks < req.min_diverse_networks)
    {
      logger(WARNING) << "Could not achieve network diversity for chunk " << chunk_index 
                      << " validation: have " << distinct_networks 
                      << " distinct networks, need " << req.min_diverse_networks
                      << ". Sampled " << sampled_peers.size() << " peer(s).";
      {
        std::lock_guard<std::mutex> lock(m_chunk_validation_mutex);
        m_current_validating_chunk_index = UINT32_MAX;
      }
      continue;
    }
    
    logger(INFO) << "Sampled " << sampled_peers.size() << " peer(s) from " << distinct_networks 
                 << " distinct network(s) (requirement: " << req.min_diverse_networks << " networks)";
    
    // Send async requests to sampled peers
    uint64_t request_time = time(nullptr);
    uint32_t requests_sent = 0;
    for (uint64_t peer_id : sampled_peers)
    {
      if (send_chunk_hash_request_async(peer_id, chunk_index))
      {
        requests_sent++;
      }
    }
    
    if (requests_sent == 0)
    {
      logger(WARNING) << "Failed to send any async requests for chunk " << chunk_index;
      {
        std::lock_guard<std::mutex> lock(m_chunk_validation_mutex);
        m_current_validating_chunk_index = UINT32_MAX;
      }
      continue;
    }
    
    // Store pending validation state
    {
      std::lock_guard<std::mutex> lock(m_pending_validations_mutex);
      PendingChunkValidation pending;
      pending.chunk_index = chunk_index;
      pending.request_timestamp = request_time;
      pending.attempt_start_time = request_time;
      pending.attempt_number = 1;
      pending.requested_peers = sampled_peers;
      pending.local_hash = local_chunk_hash;
      pending.is_first_attempt = true;
      m_pending_validations[chunk_index] = pending;
    }
    
    logger(INFO) << "Sent async chunk hash requests for chunk " << chunk_index 
                 << " to " << requests_sent << " peer(s). "
                 << "Will check for consensus after 2 minutes.";
    
    // Clear validation state (validation is now async - will be checked in check_pending_chunk_validations)
    {
      std::lock_guard<std::mutex> lock(m_chunk_validation_mutex);
      m_current_validating_chunk_index = UINT32_MAX;
    }
    
    // Don't process next chunk until this one is validated (file must be sequential)
    // The check_pending_chunk_validations() function will handle consensus checking
    break;
  }
  
  return true;
}

void ChunkValidationManager::check_pending_chunk_validations()
{
  uint64_t time_now = time(nullptr);
  const uint64_t CONSENSUS_WAIT_SECONDS = 120;  // 2 minutes
  const uint64_t RETRY_DELAY_SECONDS = 60;      // 1 minute delay before retry
  
  std::lock_guard<std::mutex> lock(m_pending_validations_mutex);
  
  // Iterate through pending validations
  for (auto it = m_pending_validations.begin(); it != m_pending_validations.end();)
  {
    uint32_t chunk_index = it->first;
    PendingChunkValidation& pending = it->second;
    
    uint64_t elapsed = time_now - pending.request_timestamp;
    
    // Check if 2 minutes have passed since requests were sent
    if (elapsed < CONSENSUS_WAIT_SECONDS)
    {
      // Not enough time has passed yet, skip this validation
      ++it;
      continue;
    }
    
    // 2 minutes have passed - check for consensus
    logger(INFO) << "Checking consensus for chunk " << chunk_index 
                 << " (elapsed: " << elapsed << " seconds, attempt " << pending.attempt_number << ")";
    
    // Collect responses from requested peers
    std::unordered_map<crypto::Hash, uint32_t, boost::hash<crypto::Hash>> hash_votes;  // hash -> vote count
    uint32_t agreements = 0;
    uint32_t null_hash_responses = 0;
    uint32_t responses_received = 0;
    
    {
      std::lock_guard<std::mutex> lock2(m_pending_chunk_hashes_mutex);
      
      for (uint64_t peer_id : pending.requested_peers)
      {
        auto response_it = m_pending_chunk_hashes.find(std::make_pair(peer_id, chunk_index));
        if (response_it != m_pending_chunk_hashes.end())
        {
          crypto::Hash peer_hash = response_it->second.hash;
          responses_received++;
          
          if (peer_hash == NULL_HASH)
          {
            null_hash_responses++;
            continue;
          }
          
          // Count votes for this hash
          hash_votes[peer_hash]++;
          
          if (peer_hash == pending.local_hash)
          {
            agreements++;
          }
        }
      }
    }
    
    // Calculate consensus requirements
    CheckpointList::ConsensusRequirements req = m_core.getCheckpointList().calculate_consensus_requirements(pending.requested_peers.size());
    
    logger(INFO) << "Chunk " << chunk_index << " consensus check: received " << responses_received 
                 << " response(s) from " << pending.requested_peers.size() << " requested peer(s). "
                 << "Agreements: " << agreements << " (need M=" << req.min_agreements << "), "
                 << "NULL_HASH responses: " << null_hash_responses;
    
    // Check if we have M agreements (consensus reached)
    if (agreements >= req.min_agreements)
    {
      // Consensus reached - save to checkpoint.dat
      logger(INFO, BRIGHT_GREEN) << "Chunk " << chunk_index 
                                  << " validated via peer consensus (" << agreements 
                                  << " agreements, need M=" << req.min_agreements << ")";
      
      if (m_core.getCheckpointList().add_verified_chunk_to_file(chunk_index))
      {
        logger(INFO, BRIGHT_GREEN) << "Chunk " << chunk_index 
                                    << " saved to checkpoint.dat";
        
        // Remove pending validation
        it = m_pending_validations.erase(it);
        
        // Clean up responses for this chunk
        {
          std::lock_guard<std::mutex> lock2(m_pending_chunk_hashes_mutex);
          for (uint64_t peer_id : pending.requested_peers)
          {
            m_pending_chunk_hashes.erase(std::make_pair(peer_id, chunk_index));
          }
        }
        continue;
      }
      else
      {
        logger(ERROR) << "Failed to save validated chunk " << chunk_index << " to checkpoint.dat";
        // Remove pending validation anyway (we'll retry later)
        it = m_pending_validations.erase(it);
        continue;
      }
    }
    
    // Check if we have M peers agreeing on a DIFFERENT hash (actual divergence)
    crypto::Hash consensus_hash = NULL_HASH;
    uint32_t max_votes = 0;
    for (const auto& vote : hash_votes)
    {
      if (vote.first != pending.local_hash && vote.second > max_votes)
      {
        max_votes = vote.second;
        consensus_hash = vote.first;
      }
    }
    
    bool has_divergence = (consensus_hash != NULL_HASH && max_votes >= req.min_agreements);
    
    // If all responses are NULL_HASH or missing, peers don't have this chunk yet (not a divergence)
    if (responses_received == 0 || (responses_received == null_hash_responses && !has_divergence))
    {
      logger(INFO) << "Chunk " << chunk_index 
                   << " validation: No peers have this chunk in memory yet (all returned NULL_HASH or no response). "
                   << "This is normal if: (1) peers are using version 1 (don't support chunk checkpoints), "
                   << "or (2) peers haven't created this chunk yet. "
                   << "Will retry validation once peers create this chunk.";
      
      // Remove pending validation - we'll retry later when peers have the chunk
      it = m_pending_validations.erase(it);
      
      // Clean up responses
      {
        std::lock_guard<std::mutex> lock2(m_pending_chunk_hashes_mutex);
        for (uint64_t peer_id : pending.requested_peers)
        {
          m_pending_chunk_hashes.erase(std::make_pair(peer_id, chunk_index));
        }
      }
      continue;
    }
    
    // Consensus not reached - check if we should retry
    if (pending.is_first_attempt)
    {
      // First attempt failed - wait 1 more minute (3 minutes total) before retry
      uint64_t total_elapsed = time_now - pending.attempt_start_time;
      if (total_elapsed < (CONSENSUS_WAIT_SECONDS + RETRY_DELAY_SECONDS))
      {
        // Still waiting for retry delay
        ++it;
        continue;
      }
      
      // Retry delay passed - start second attempt
      logger(INFO) << "Chunk " << chunk_index 
                   << " consensus failed on first attempt. Starting second attempt...";
      
      // Send new requests to same peers (or get new eligible peers)
      // For now, reuse same peers
      uint64_t retry_time = time_now;
      uint32_t requests_sent = 0;
      for (uint64_t peer_id : pending.requested_peers)
      {
        if (send_chunk_hash_request_async(peer_id, chunk_index))
        {
          requests_sent++;
        }
      }
      
      if (requests_sent > 0)
      {
        // Update pending validation for second attempt
        pending.request_timestamp = retry_time;
        pending.attempt_number = 2;
        pending.is_first_attempt = false;
        
        logger(INFO) << "Sent second attempt async requests for chunk " << chunk_index 
                     << " to " << requests_sent << " peer(s). "
                     << "Will check for consensus after 2 minutes.";
      }
      else
      {
        logger(WARNING) << "Failed to send second attempt requests for chunk " << chunk_index;
        // Remove pending validation
        it = m_pending_validations.erase(it);
        continue;
      }
    }
    else
    {
      // Second attempt also failed - check if it's actual divergence or just no responses
      
      // Check if we have M peers agreeing on a DIFFERENT hash (actual divergence)
      crypto::Hash consensus_hash_second = NULL_HASH;
      uint32_t max_votes_second = 0;
      for (const auto& vote : hash_votes)
      {
        if (vote.first != pending.local_hash && vote.second > max_votes_second)
        {
          max_votes_second = vote.second;
          consensus_hash_second = vote.first;
        }
      }
      
      bool has_divergence_second = (consensus_hash_second != NULL_HASH && max_votes_second >= req.min_agreements);
      
      // If all responses are NULL_HASH or missing, peers don't have this chunk yet (not a divergence)
      if (responses_received == 0 || (responses_received == null_hash_responses && !has_divergence_second))
      {
        logger(INFO) << "Chunk " << chunk_index 
                     << " validation: No peers have this chunk in memory yet after 2 attempts "
                     << "(all returned NULL_HASH or no response). "
                     << "This is normal if: (1) peers are using version 1 (don't support chunk checkpoints), "
                     << "or (2) peers haven't created this chunk yet. "
                     << "Will retry validation once peers create this chunk.";
        
        // Remove pending validation - we'll retry later when peers have the chunk
        it = m_pending_validations.erase(it);
        
        // Clean up responses
        {
          std::lock_guard<std::mutex> lock2(m_pending_chunk_hashes_mutex);
          for (uint64_t peer_id : pending.requested_peers)
          {
            m_pending_chunk_hashes.erase(std::make_pair(peer_id, chunk_index));
          }
        }
        continue;
      }
      
      // Actual divergence: M peers agree on different hash
      if (has_divergence_second)
      {
        logger(ERROR, BRIGHT_RED) << "Chunk " << chunk_index 
                                    << " validation FAILED: peer consensus did not agree with local chunk hash "
                                    << "after 2 attempts. M peers (" << max_votes_second 
                                    << ") agree on different hash: " << consensus_hash_second 
                                    << " (local: " << pending.local_hash << "). "
                                    << "This indicates blockchain divergence.";
        
        // Calculate rollback height (bottom of the chunk)
        uint32_t chunk_size = m_core.getCheckpointList().get_chunk_size();
        uint32_t rollback_height = chunk_index * chunk_size;
        
        logger(ERROR, BRIGHT_RED) << "Rolling back blockchain to height " << rollback_height 
                                  << " (chunk " << chunk_index << " boundary) due to peer consensus divergence";
        
        // Truncate checkpoint.dat to the previous chunk (chunk_index - 1)
        uint32_t last_valid_chunk = (chunk_index > 0) ? (chunk_index - 1) : 0;
        if (!m_core.getCheckpointList().truncate_checkpoint_file(last_valid_chunk))
        {
          logger(ERROR, BRIGHT_RED) << "Failed to truncate checkpoint.dat to chunk " << last_valid_chunk;
        }
        else
        {
          logger(INFO) << "Truncated checkpoint.dat to chunk " << last_valid_chunk;
        }
        
        // Rollback blockchain to the chunk boundary
        if (!m_core.rollback_chain_to(rollback_height))
        {
          logger(ERROR, BRIGHT_RED) << "Failed to rollback blockchain to height " << rollback_height 
                                    << " - node may be in inconsistent state";
        }
        else
        {
          logger(INFO, BRIGHT_GREEN) << "Successfully rolled back blockchain to height " << rollback_height;
        }
        
        // Remove pending validation
        it = m_pending_validations.erase(it);
        
        // Clean up responses
        {
          std::lock_guard<std::mutex> lock2(m_pending_chunk_hashes_mutex);
          for (uint64_t peer_id : pending.requested_peers)
          {
            m_pending_chunk_hashes.erase(std::make_pair(peer_id, chunk_index));
          }
        }
        continue;
      }
      
      // Mixed responses (some NULL_HASH, some mismatches, but not M agreements on different hash)
      logger(WARNING) << "Chunk " << chunk_index 
                      << " validation: Inconsistent results after 2 attempts. "
                      << "Some peers returned NULL_HASH, some returned different hashes, "
                      << "but no M peers agreed on a single different hash. "
                      << "This may indicate network issues or peers still syncing. "
                      << "Will retry validation later.";
      
      // Remove pending validation - we'll retry later
      it = m_pending_validations.erase(it);
      
      // Clean up responses
      {
        std::lock_guard<std::mutex> lock2(m_pending_chunk_hashes_mutex);
        for (uint64_t peer_id : pending.requested_peers)
        {
          m_pending_chunk_hashes.erase(std::make_pair(peer_id, chunk_index));
        }
      }
      continue;
    }
    
    ++it;
  }
}

} // namespace cn


// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2026 Conceal Network & Conceal Devs
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
  
  bool sent = post_notify<NOTIFY_REQUEST_CHUNK_HASH>(*m_p2p, req, *peer_context);
  if (!sent) {
    logger(WARNING) << "[Chunk Validation] Failed to send chunk " << chunk_index << " hash request to peer " << peer_id;
    return false;
  }
  
  return true;
}

bool ChunkValidationManager::store_chunk_hash_response(uint64_t peer_id, uint32_t chunk_index, const crypto::Hash& hash)
{
  uint64_t response_time = time(nullptr);
  {
    std::lock_guard<std::mutex> validation_lock(m_pending_validations_mutex);
    auto pending_it = m_pending_validations.find(chunk_index);
    if (pending_it == m_pending_validations.end())
    {
      return false;
    }

    const std::vector<uint64_t>& requested_peers = pending_it->second.requested_peers;
    if (std::find(requested_peers.begin(), requested_peers.end(), peer_id) == requested_peers.end())
    {
      return false;
    }

    std::lock_guard<std::mutex> lock(m_pending_chunk_hashes_mutex);
    auto key = std::make_pair(peer_id, chunk_index);
    ChunkHashResponse response;
    response.hash = hash;
    response.timestamp = response_time;
    m_pending_chunk_hashes[key] = response;
  }

  return true;
}

void ChunkValidationManager::cleanup_chunk_responses(uint32_t chunk_index, const std::vector<uint64_t>& peer_ids)
{
  std::lock_guard<std::mutex> lock(m_pending_chunk_hashes_mutex);
  for (uint64_t peer_id : peer_ids)
  {
    m_pending_chunk_hashes.erase(std::make_pair(peer_id, chunk_index));
  }
}

bool ChunkValidationManager::is_chunk_being_validated(uint32_t chunk_index) const
{
  // Check if chunk is in pending validations (actual validation state)
  // m_current_validating_chunk_index is cleared immediately after sending requests,
  // but validation is still pending for 3 minutes, so we need to check m_pending_validations
  std::lock_guard<std::mutex> lock(m_pending_validations_mutex);
  return (m_pending_validations.find(chunk_index) != m_pending_validations.end());
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
    logger(DEBUGGING) << "[Chunk Validation] No peers available for validation";
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
  logger(INFO) << "[Chunk Validation] Checking " << total_connected << " connected peers for eligibility";
  
  m_p2p->for_each_connection([&](CryptoNoteConnectionContext& ctx, uint64_t peer_id) {
    // Only consider peers that support chunk-based checkpoints
    if (ctx.version < cn::P2P_CHECKPOINT_LIST_VERSION)
    {
      logger(DEBUGGING) << "[Chunk Validation] Peer " << peer_id << " filtered: P2P version " << ctx.version << " < required version";
      return; // Skip old version peers
    }
    
    // Accept peers in normal state (synchronized), idle state, OR synchronizing state
    // We can validate chunks even during sync, as long as peers are connected
    if (ctx.m_state != CryptoNoteConnectionContext::state_normal && 
        ctx.m_state != CryptoNoteConnectionContext::state_idle &&
        ctx.m_state != CryptoNoteConnectionContext::state_synchronizing)
    {
      logger(DEBUGGING) << "[Chunk Validation] Peer " << peer_id << " filtered: state " << get_protocol_state_string(ctx.m_state) << " (need normal, idle, or synchronizing)";
      return; // Skip peers that aren't in a usable state
    }
    
    // Calculate peer uptime in blocks
    // Uptime = (current_time - connection_start_time) / block_time
    // This is an approximation - ideally we'd track peer's blockchain height when they first connected
    time_t connection_duration = time_now - ctx.m_started;
    if (connection_duration < 0)
    {
      logger(DEBUGGING) << "[Chunk Validation] Peer " << peer_id << " filtered: invalid connection time";
      return; // Invalid connection time
    }
    
    uint32_t peer_uptime_blocks = static_cast<uint32_t>(connection_duration / block_time);
    
    // Check if peer meets minimum uptime requirement
    if (peer_uptime_blocks < min_uptime_blocks)
    {
      logger(DEBUGGING) << "[Chunk Validation] Peer " << peer_id << " filtered: uptime " << peer_uptime_blocks << " blocks < required";
      return; // Peer doesn't meet uptime requirement
    }
    
    // Peer is eligible
    logger(DEBUGGING) << "[Chunk Validation] Peer " << peer_id << " is ELIGIBLE (version: " << ctx.version << ", state: "
                  << get_protocol_state_string(ctx.m_state) << ", uptime: " << peer_uptime_blocks << " blocks)";
    eligible_peers.push_back(peer_id);
    peer_network_16[peer_id] = CheckpointList::get_network_16(ctx.m_remote_ip);
  });
  
  if (eligible_peers.empty())
  {
    logger(INFO) << "[Chunk Validation] No eligible peers found (need uptime > " << min_uptime_blocks << " blocks)";
    logger(INFO) << "Note: Only ACTIVE CONNECTIONS are considered, not peers in peerlist. "
                 << "Use 'print_cn' command to see active connections. "
                 << "To force connection to a peer, use --add-priority-node <IP>:<PORT>";
    logger(INFO) << "Chunk validation will be retried when eligible peers become available";
    return false;
  }
  
  logger(INFO) << "[Chunk Validation] Found " << eligible_peers.size() << " eligible peers (uptime > " << min_uptime_blocks << " blocks)";
  
  // Calculate consensus requirements (M, K, n) based on available eligible peers
  CheckpointList::ConsensusRequirements req = m_core.getCheckpointList().calculate_consensus_requirements(eligible_peers.size());
  
  // STEP 1: Check if we have enough eligible peers to meet K requirement
  // If not enough eligible peers, break and retry later when more peers become eligible
  if (eligible_peers.size() < req.min_peers)
  {
    logger(INFO) << "[Chunk Validation] Not enough eligible peers: have " << eligible_peers.size() 
                << ", need K=" << req.min_peers << ". Will retry when more peers become available.";
    return false; // Will retry via rate limiting mechanism (5 minute wait)
  }

  
  // Log all eligible peers for debugging
  logger(DEBUGGING) << "[Chunk Validation] Eligible peers list:";
  m_p2p->for_each_connection([&](CryptoNoteConnectionContext& ctx, uint64_t peer_id) {
    if (std::find(eligible_peers.begin(), eligible_peers.end(), peer_id) != eligible_peers.end()) {
      logger(DEBUGGING) << "  - Peer " << peer_id << " (version: " << ctx.version << ")";
    }
  });
  
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
      logger(WARNING) << "[Chunk Validation] Cannot validate chunk " << chunk_index << ": chunk hash not found in memory";
      {
        std::lock_guard<std::mutex> lock(m_chunk_validation_mutex);
        m_current_validating_chunk_index = UINT32_MAX;
      }
      continue;
    }
    
    logger(INFO) << "[Chunk Validation] Validating chunk " << chunk_index << " with " << eligible_peers.size() << " eligible peers";
    
    // Check if this chunk is already being validated asynchronously
    {
      std::lock_guard<std::mutex> lock(m_pending_validations_mutex);
      if (m_pending_validations.find(chunk_index) != m_pending_validations.end())
      {
        logger(DEBUGGING) << "[Chunk Validation] Chunk " << chunk_index << " is already being validated, skipping";
        {
          std::lock_guard<std::mutex> lock2(m_chunk_validation_mutex);
          m_current_validating_chunk_index = UINT32_MAX;
        }
        continue;
      }
    }
    
    // Calculate consensus requirements (M, K, n)
    CheckpointList::ConsensusRequirements req = m_core.getCheckpointList().calculate_consensus_requirements(eligible_peers.size());
    
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
      logger(WARNING) << "[Chunk Validation] Could not sample any peers for chunk " << chunk_index << " validation";
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
      logger(WARNING) << "[Chunk Validation] Could not achieve network diversity for chunk " << chunk_index
                    << ": have " << distinct_networks << " distinct networks, need " << req.min_diverse_networks;
      {
        std::lock_guard<std::mutex> lock(m_chunk_validation_mutex);
        m_current_validating_chunk_index = UINT32_MAX;
      }
      continue;
    }
    
    logger(INFO) << "[Chunk Validation] Sampled " << sampled_peers.size() << " peers from " << distinct_networks
                 << " distinct networks (requirement: " << req.min_diverse_networks << " networks)";
    
    // Log which peers were selected for debugging
    logger(DEBUGGING) << "[Chunk Validation] Sampled peers for chunk " << chunk_index << ":";
    for (uint64_t peer_id : sampled_peers) {
      logger(DEBUGGING) << "  - Selected peer " << peer_id;
    }
    
    // Send async requests to sampled peers
    uint64_t request_time = time(nullptr);
    std::vector<uint64_t> sent_peers;
    std::map<uint64_t, uint32_t> sent_peer_networks;
    for (uint64_t peer_id : sampled_peers)
    {
      if (send_chunk_hash_request_async(peer_id, chunk_index))
      {
        sent_peers.push_back(peer_id);
        sent_peer_networks[peer_id] = getPeerNetwork16(peer_id);
      }
    }
    
    if (sent_peers.size() < req.min_peers)
    {
      logger(WARNING) << "[Chunk Validation] Only sent " << sent_peers.size()
                      << " async request(s) for chunk " << chunk_index
                      << ", need K=" << req.min_peers << " for consensus";
      cleanup_chunk_responses(chunk_index, sent_peers);
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
      pending.requested_peers = sent_peers;
      pending.peer_network_16 = sent_peer_networks;
      pending.local_hash = local_chunk_hash;
      pending.is_first_attempt = true;
      m_pending_validations[chunk_index] = pending;
    }
    
    logger(INFO) << "[Chunk Validation] Sent chunk " << chunk_index << " hash requests to " << sent_peers.size() << " peers. Checking consensus in 3 minutes.";
    
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
  const uint64_t CONSENSUS_WAIT_SECONDS = 180;  // 3 minutes (increased from 2 to handle network latency and peer processing)
  const uint64_t RETRY_DELAY_SECONDS = 60;      // 1 minute delay before retry

  struct PendingAction
  {
    enum Type
    {
      ADD_VERIFIED_CHUNK,
      ROLLBACK_DIVERGENT_CHUNK
    };

    Type type;
    uint32_t chunk_index;
    uint32_t rollback_height;
    uint32_t last_valid_chunk;
  };

  std::vector<PendingAction> actions;
  
  {
    std::lock_guard<std::mutex> lock(m_pending_validations_mutex);

    // Iterate through pending validations
    for (auto it = m_pending_validations.begin(); it != m_pending_validations.end();)
    {
      uint32_t chunk_index = it->first;
      PendingChunkValidation& pending = it->second;
      
      uint64_t elapsed = time_now - pending.request_timestamp;
      
      // Check if 3 minutes have passed since requests were sent
      if (elapsed < CONSENSUS_WAIT_SECONDS)
      {
        // Not enough time has passed yet, skip this validation
        ++it;
        continue;
      }
      
      // 3 minutes have passed - check for consensus
      logger(INFO) << "[Chunk Validation] Checking consensus for chunk " << chunk_index << " (elapsed: " << elapsed << " seconds, attempt " << pending.attempt_number << ")";
      
      // Collect fresh responses from requested peers
      std::vector<CheckpointList::ConsensusVote> votes;
      {
        std::lock_guard<std::mutex> lock2(m_pending_chunk_hashes_mutex);
        
        for (uint64_t peer_id : pending.requested_peers)
        {
          auto response_it = m_pending_chunk_hashes.find(std::make_pair(peer_id, chunk_index));
          if (response_it != m_pending_chunk_hashes.end())
          {
            CheckpointList::ConsensusVote vote;
            vote.peer_id = peer_id;
            vote.hash = response_it->second.hash;
            vote.timestamp = response_it->second.timestamp;
            votes.push_back(vote);
          }
        }
      }
      
      // Calculate consensus requirements
      CheckpointList::ConsensusRequirements req = m_core.getCheckpointList().calculate_consensus_requirements(pending.requested_peers.size());
      CheckpointList::ConsensusVoteResult vote_result = CheckpointList::evaluate_consensus_votes(
        votes,
        pending.local_hash,
        pending.peer_network_16,
        pending.request_timestamp,
        req);
      
      logger(INFO) << "[Chunk Validation] Chunk " << chunk_index << " consensus check: received " << vote_result.responses_received
                   << " fresh response(s) from " << pending.requested_peers.size() << " requested peer(s). "
                   << "Agreements: " << vote_result.agreements << " (need M=" << req.min_agreements
                   << " from n=" << req.min_diverse_networks << " networks, got n="
                   << vote_result.local_diverse_networks << "), "
                   << "NULL_HASH responses: " << vote_result.null_hash_responses;
      
      // Check if we have M agreements from enough networks (consensus reached)
      if (vote_result.local_consensus)
      {
        logger(INFO, BRIGHT_GREEN) << "[Chunk Validation] Chunk " << chunk_index
                                   << " validated via peer consensus (" << vote_result.agreements
                                   << " agreements from " << vote_result.local_diverse_networks
                                   << " networks, need M=" << req.min_agreements << ")";

        std::vector<uint64_t> peers_to_cleanup = pending.requested_peers;
        it = m_pending_validations.erase(it);
        cleanup_chunk_responses(chunk_index, peers_to_cleanup);

        PendingAction action;
        action.type = PendingAction::ADD_VERIFIED_CHUNK;
        action.chunk_index = chunk_index;
        action.rollback_height = 0;
        action.last_valid_chunk = 0;
        actions.push_back(action);
        continue;
      }
      
      // If all responses are NULL_HASH or missing, peers don't have this chunk yet (not a divergence)
      if (vote_result.responses_received == 0 ||
          (vote_result.responses_received == vote_result.null_hash_responses && !vote_result.divergent_consensus))
      {
        logger(INFO) << "Chunk " << chunk_index 
                     << " validation: No peers have this chunk in memory yet (all returned NULL_HASH or no response). "
                     << "This is normal if: (1) peers are using version 1 (don't support chunk checkpoints), "
                     << "or (2) peers haven't created this chunk yet. "
                     << "Will retry validation once peers create this chunk.";
        
        std::vector<uint64_t> peers_to_cleanup = pending.requested_peers;
        it = m_pending_validations.erase(it);
        cleanup_chunk_responses(chunk_index, peers_to_cleanup);
        continue;
      }
      
      // Consensus not reached - check if we should retry
      if (pending.is_first_attempt)
      {
        // First attempt failed - wait 1 more minute before retry
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
        
        std::vector<uint64_t> previous_peers = pending.requested_peers;
        std::map<uint64_t, uint32_t> previous_peer_networks = pending.peer_network_16;
        cleanup_chunk_responses(chunk_index, previous_peers);

        uint64_t retry_time = time_now;
        std::vector<uint64_t> sent_peers;
        std::map<uint64_t, uint32_t> sent_peer_networks;
        for (uint64_t peer_id : previous_peers)
        {
          if (send_chunk_hash_request_async(peer_id, chunk_index))
          {
            sent_peers.push_back(peer_id);
            auto network_it = previous_peer_networks.find(peer_id);
            sent_peer_networks[peer_id] = (network_it != previous_peer_networks.end()) ? network_it->second : 0;
          }
        }
        
        if (sent_peers.size() >= req.min_peers)
        {
          // Update pending validation for second attempt
          pending.request_timestamp = retry_time;
          pending.attempt_number = 2;
          pending.requested_peers = sent_peers;
          pending.peer_network_16 = sent_peer_networks;
          pending.is_first_attempt = false;
          
          logger(INFO) << "Sent second attempt async requests for chunk " << chunk_index 
                       << " to " << sent_peers.size() << " peer(s). "
                       << "Will check for consensus after 3 minutes.";
        }
        else
        {
          logger(WARNING) << "Failed to send enough second attempt requests for chunk " << chunk_index
                          << ": sent " << sent_peers.size() << ", need K=" << req.min_peers;
          it = m_pending_validations.erase(it);
          cleanup_chunk_responses(chunk_index, sent_peers);
          continue;
        }
      }
      else
      {
        // Second attempt also failed - check if it's actual divergence or just no responses
        if (vote_result.responses_received == 0 ||
            (vote_result.responses_received == vote_result.null_hash_responses && !vote_result.divergent_consensus))
        {
          logger(INFO) << "Chunk " << chunk_index 
                       << " validation: No peers have this chunk in memory yet after 2 attempts "
                       << "(all returned NULL_HASH or no response). "
                       << "This is normal if: (1) peers are using version 1 (don't support chunk checkpoints), "
                       << "or (2) peers haven't created this chunk yet. "
                       << "Will retry validation once peers create this chunk.";
          
          std::vector<uint64_t> peers_to_cleanup = pending.requested_peers;
          it = m_pending_validations.erase(it);
          cleanup_chunk_responses(chunk_index, peers_to_cleanup);
          continue;
        }
        
        // Actual divergence: M peers agree on a different hash from enough networks
        if (vote_result.divergent_consensus)
        {
          logger(ERROR, BRIGHT_RED) << "Chunk " << chunk_index 
                                    << " validation FAILED: peer consensus did not agree with local chunk hash "
                                    << "after 2 attempts. M peers (" << vote_result.consensus_hash_votes 
                                    << ") from " << vote_result.consensus_hash_diverse_networks
                                    << " networks agree on different hash: " << vote_result.consensus_hash 
                                    << " (local: " << pending.local_hash << "). "
                                    << "This indicates blockchain divergence.";
          
          // Calculate rollback height (bottom of the chunk)
          uint32_t chunk_size = m_core.getCheckpointList().get_chunk_size();
          uint32_t rollback_height = chunk_index * chunk_size;
          
          logger(ERROR, BRIGHT_RED) << "Rolling back blockchain to height " << rollback_height 
                                    << " (chunk " << chunk_index << " boundary) due to peer consensus divergence";
          
          PendingAction action;
          action.type = PendingAction::ROLLBACK_DIVERGENT_CHUNK;
          action.chunk_index = chunk_index;
          action.rollback_height = rollback_height;
          action.last_valid_chunk = (chunk_index > 0) ? (chunk_index - 1) : 0;
          actions.push_back(action);
          
          std::vector<uint64_t> peers_to_cleanup = pending.requested_peers;
          it = m_pending_validations.erase(it);
          cleanup_chunk_responses(chunk_index, peers_to_cleanup);
          continue;
        }
        
        // Mixed responses (some NULL_HASH, some mismatches, but no diverse M agreement on a single different hash)
        logger(WARNING) << "Chunk " << chunk_index 
                        << " validation: Inconsistent results after 2 attempts. "
                        << "Some peers returned NULL_HASH, some returned different hashes, "
                        << "but no M peers from enough networks agreed on a single different hash. "
                        << "This may indicate network issues or peers still syncing. "
                        << "Will retry validation later.";
        
        std::vector<uint64_t> peers_to_cleanup = pending.requested_peers;
        it = m_pending_validations.erase(it);
        cleanup_chunk_responses(chunk_index, peers_to_cleanup);
        continue;
      }
      
      ++it;
    }
  }

  for (const PendingAction& action : actions)
  {
    if (action.type == PendingAction::ADD_VERIFIED_CHUNK)
    {
      if (!m_core.getCheckpointList().add_verified_chunk_to_file(action.chunk_index))
      {
        logger(ERROR) << "Failed to save validated chunk " << action.chunk_index << " to checkpoint.dat";
      }
    }
    else
    {
      if (!m_core.getCheckpointList().truncate_checkpoint_file(action.last_valid_chunk))
      {
        logger(ERROR, BRIGHT_RED) << "Failed to truncate checkpoint.dat to chunk " << action.last_valid_chunk;
      }
      else
      {
        logger(INFO) << "Truncated checkpoint.dat to chunk " << action.last_valid_chunk;
      }
      
      if (!m_core.rollback_chain_to(action.rollback_height))
      {
        logger(ERROR, BRIGHT_RED) << "Failed to rollback blockchain to height " << action.rollback_height
                                  << " - node may be in inconsistent state";
      }
      else
      {
        logger(INFO, BRIGHT_GREEN) << "Successfully rolled back blockchain to height " << action.rollback_height;
      }
    }
  }
}

} // namespace cn


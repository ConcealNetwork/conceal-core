// Copyright (c) 2012-2017 The Cryptonote developers
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest/gtest.h"

#include "CryptoNoteCore/CheckpointList.h"
#include "CryptoNoteCore/Blockchain.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/TransactionPool.h"
#include "CryptoNoteCore/ITransactionValidator.h"
#include "CryptoNoteCore/ITimeProvider.h"
#include <Logging/LoggerGroup.h>
#include <Common/StringTools.h>
#include "crypto/hash.h"
#include <map>
#include <string>

using namespace cn;

// Stub implementation for testing
class TestTransactionValidator : public cn::ITransactionValidator {
public:
  bool checkTransactionInputs(const cn::Transaction&, BlockInfo&) override { return true; }
  bool checkTransactionInputs(const cn::Transaction&, BlockInfo&, BlockInfo&) override { return true; }
  bool haveSpentKeyImages(const cn::Transaction&) override { return false; }
  bool checkTransactionSize(size_t) override { return true; }
};

namespace {

crypto::Hash testHash(const std::string& hex) {
  crypto::Hash hash = NULL_HASH;
  bool parsed = common::podFromHex(hex, hash);
  EXPECT_TRUE(parsed);
  return hash;
}

CheckpointList::ConsensusRequirements testConsensusRequirements() {
  CheckpointList::ConsensusRequirements req;
  req.min_agreements = 2;
  req.min_peers = 2;
  req.min_diverse_networks = 2;
  return req;
}

CheckpointList::ConsensusVote testVote(uint64_t peer_id, const crypto::Hash& hash, uint64_t timestamp) {
  CheckpointList::ConsensusVote vote;
  vote.peer_id = peer_id;
  vote.hash = hash;
  vote.timestamp = timestamp;
  return vote;
}

}

TEST(checkpoints_consensus_votes, ignores_stale_responses)
{
  crypto::Hash local_hash = testHash("1111111111111111111111111111111111111111111111111111111111111111");
  std::vector<CheckpointList::ConsensusVote> votes;
  votes.push_back(testVote(1, local_hash, 10));
  votes.push_back(testVote(2, local_hash, 10));
  
  std::map<uint64_t, uint32_t> peer_networks;
  peer_networks[1] = 0x0101;
  peer_networks[2] = 0x0202;
  
  CheckpointList::ConsensusVoteResult result = CheckpointList::evaluate_consensus_votes(
    votes, local_hash, peer_networks, 20, testConsensusRequirements());
  
  ASSERT_EQ(0, result.responses_received);
  ASSERT_EQ(0, result.agreements);
  ASSERT_FALSE(result.local_consensus);
}

TEST(checkpoints_consensus_votes, requires_diverse_networks_for_local_consensus)
{
  crypto::Hash local_hash = testHash("2222222222222222222222222222222222222222222222222222222222222222");
  std::vector<CheckpointList::ConsensusVote> votes;
  votes.push_back(testVote(1, local_hash, 30));
  votes.push_back(testVote(2, local_hash, 30));
  
  std::map<uint64_t, uint32_t> peer_networks;
  peer_networks[1] = 0x0101;
  peer_networks[2] = 0x0101;
  
  CheckpointList::ConsensusVoteResult result = CheckpointList::evaluate_consensus_votes(
    votes, local_hash, peer_networks, 20, testConsensusRequirements());
  
  ASSERT_EQ(2, result.responses_received);
  ASSERT_EQ(2, result.agreements);
  ASSERT_EQ(1, result.local_diverse_networks);
  ASSERT_FALSE(result.local_consensus);
}

TEST(checkpoints_consensus_votes, accepts_diverse_local_consensus)
{
  crypto::Hash local_hash = testHash("3333333333333333333333333333333333333333333333333333333333333333");
  std::vector<CheckpointList::ConsensusVote> votes;
  votes.push_back(testVote(1, local_hash, 30));
  votes.push_back(testVote(2, local_hash, 30));
  
  std::map<uint64_t, uint32_t> peer_networks;
  peer_networks[1] = 0x0101;
  peer_networks[2] = 0x0202;
  
  CheckpointList::ConsensusVoteResult result = CheckpointList::evaluate_consensus_votes(
    votes, local_hash, peer_networks, 20, testConsensusRequirements());
  
  ASSERT_EQ(2, result.responses_received);
  ASSERT_EQ(2, result.agreements);
  ASSERT_EQ(2, result.local_diverse_networks);
  ASSERT_TRUE(result.local_consensus);
}

TEST(checkpoints_consensus_votes, detects_diverse_divergent_consensus)
{
  crypto::Hash local_hash = testHash("4444444444444444444444444444444444444444444444444444444444444444");
  crypto::Hash peer_hash = testHash("5555555555555555555555555555555555555555555555555555555555555555");
  std::vector<CheckpointList::ConsensusVote> votes;
  votes.push_back(testVote(1, peer_hash, 30));
  votes.push_back(testVote(2, peer_hash, 30));
  
  std::map<uint64_t, uint32_t> peer_networks;
  peer_networks[1] = 0x0101;
  peer_networks[2] = 0x0202;
  
  CheckpointList::ConsensusVoteResult result = CheckpointList::evaluate_consensus_votes(
    votes, local_hash, peer_networks, 20, testConsensusRequirements());
  
  ASSERT_FALSE(result.local_consensus);
  ASSERT_TRUE(result.divergent_consensus);
  ASSERT_EQ(peer_hash, result.consensus_hash);
  ASSERT_EQ(2, result.consensus_hash_votes);
  ASSERT_EQ(2, result.consensus_hash_diverse_networks);
}

TEST(checkpoints_is_alternative_block_allowed, handles_empty_checkpoints)
{
  logging::LoggerGroup logger;
  cn::Currency currency = cn::CurrencyBuilder(logger).testnet(true).currency();
  TestTransactionValidator validator;
  cn::RealTimeProvider timeProvider;
  cn::tx_memory_pool tx_pool(currency, validator, timeProvider, logger);
  Blockchain blockchain(currency, tx_pool, logger, false, false);
  
  // Initialize with no checkpoints
  blockchain.getCheckpointList().init_targets(true, "");

  ASSERT_FALSE(blockchain.is_alternative_block_allowed(0, 0));

  ASSERT_TRUE(blockchain.is_alternative_block_allowed(1, 1));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(1, 9));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(9, 1));
}

TEST(checkpoints_is_alternative_block_allowed, handles_one_checkpoint)
{
  logging::LoggerGroup logger;
  cn::Currency currency = cn::CurrencyBuilder(logger).testnet(true).currency();
  TestTransactionValidator validator;
  cn::RealTimeProvider timeProvider;
  cn::tx_memory_pool tx_pool(currency, validator, timeProvider, logger);
  Blockchain blockchain(currency, tx_pool, logger, false, false);
  
  blockchain.getCheckpointList().init_targets(true, "");
  // Reset to a clean slate so only our test targets exist
  blockchain.getCheckpointList().clear_targets_for_test();

  // Build a vector of 6 zero hashes (heights 0-5), mirroring old checkpoint.dat
  std::vector<crypto::Hash> checkpoint_list(6, NULL_HASH);
  crypto::Hash list_hash = crypto::cn_fast_hash(
    checkpoint_list.data(), checkpoint_list.size() * sizeof(crypto::Hash));

  // Add checkpoint target at height 5 using the real list hash
  blockchain.getCheckpointList().add_checkpoint_target_for_test(5, common::podToHex(list_hash));

  // Load the checkpoint list into m_points (mirrors loading old checkpoint.dat)
  ASSERT_TRUE(blockchain.getCheckpointList().set_checkpoint_list(std::move(checkpoint_list)));

  ASSERT_EQ(blockchain.getCheckpointList().get_greatest_target_height(), 5u);

  ASSERT_FALSE(blockchain.is_alternative_block_allowed(0, 0));

  ASSERT_TRUE(blockchain.is_alternative_block_allowed(1, 1));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(1, 4));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(1, 5));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(1, 6));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(1, 9));

  ASSERT_TRUE(blockchain.is_alternative_block_allowed(4, 1));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(4, 4));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(4, 5));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(4, 6));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(4, 9));

  // When blockchain_height >= checkpoint_height, blocks at or below checkpoint are not allowed
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(5, 1));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(5, 4));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(5, 5));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(5, 6));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(5, 9));

  ASSERT_FALSE(blockchain.is_alternative_block_allowed(6, 1));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(6, 4));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(6, 5));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(6, 6));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(6, 9));

  ASSERT_FALSE(blockchain.is_alternative_block_allowed(9, 1));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(9, 4));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(9, 5));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(9, 6));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(9, 9));
}

TEST(checkpoints_is_alternative_block_allowed, handles_two_and_more_checkpoints)
{
  logging::LoggerGroup logger;
  cn::Currency currency = cn::CurrencyBuilder(logger).testnet(true).currency();
  TestTransactionValidator validator;
  cn::RealTimeProvider timeProvider;
  cn::tx_memory_pool tx_pool(currency, validator, timeProvider, logger);
  Blockchain blockchain(currency, tx_pool, logger, false, false);
  
  blockchain.getCheckpointList().init_targets(true, "");
  // Reset to a clean slate so only our test targets exist
  blockchain.getCheckpointList().clear_targets_for_test();

  // Build vectors of zero hashes mirroring old checkpoint.dat for each checkpoint
  // Checkpoint at height 5: 6 hashes (heights 0-5)
  std::vector<crypto::Hash> list_5(6, NULL_HASH);
  crypto::Hash hash_5 = crypto::cn_fast_hash(list_5.data(), list_5.size() * sizeof(crypto::Hash));
  blockchain.getCheckpointList().add_checkpoint_target_for_test(5, common::podToHex(hash_5));

  // Checkpoint at height 9: 10 hashes (heights 0-9)
  std::vector<crypto::Hash> list_9(10, NULL_HASH);
  crypto::Hash hash_9 = crypto::cn_fast_hash(list_9.data(), list_9.size() * sizeof(crypto::Hash));
  blockchain.getCheckpointList().add_checkpoint_target_for_test(9, common::podToHex(hash_9));

  // Load the full checkpoint list into m_points (mirrors loading final checkpoint.dat)
  ASSERT_TRUE(blockchain.getCheckpointList().set_checkpoint_list(std::move(list_9)));

  ASSERT_EQ(blockchain.getCheckpointList().get_greatest_target_height(), 9u);

  ASSERT_FALSE(blockchain.is_alternative_block_allowed(0, 0));

  ASSERT_TRUE(blockchain.is_alternative_block_allowed(1, 1));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(1, 4));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(1, 5));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(1, 6));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(1, 8));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(1, 9));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(1, 10));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(1, 11));

  ASSERT_TRUE(blockchain.is_alternative_block_allowed(4, 1));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(4, 4));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(4, 5));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(4, 6));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(4, 8));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(4, 9));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(4, 10));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(4, 11));

  ASSERT_FALSE(blockchain.is_alternative_block_allowed(5, 1));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(5, 4));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(5, 5));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(5, 6));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(5, 8));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(5, 9));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(5, 10));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(5, 11));

  ASSERT_FALSE(blockchain.is_alternative_block_allowed(6, 1));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(6, 4));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(6, 5));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(6, 6));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(6, 8));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(6, 9));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(6, 10));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(6, 11));

  ASSERT_FALSE(blockchain.is_alternative_block_allowed(8, 1));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(8, 4));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(8, 5));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(8, 6));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(8, 8));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(8, 9));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(8, 10));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(8, 11));

  ASSERT_FALSE(blockchain.is_alternative_block_allowed(9, 1));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(9, 4));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(9, 5));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(9, 6));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(9, 8));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(9, 9));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(9, 10));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(9, 11));

  ASSERT_FALSE(blockchain.is_alternative_block_allowed(10, 1));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(10, 4));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(10, 5));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(10, 6));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(10, 8));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(10, 9));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(10, 10));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(10, 11));

  ASSERT_FALSE(blockchain.is_alternative_block_allowed(11, 1));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(11, 4));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(11, 5));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(11, 6));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(11, 8));
  ASSERT_FALSE(blockchain.is_alternative_block_allowed(11, 9));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(11, 10));
  ASSERT_TRUE(blockchain.is_alternative_block_allowed(11, 11));
}

#include <cstdint>

#include <gtest/gtest.h>
#include <boost/utility/value_init.hpp>

#include "ChainAudit/ChainAudit.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/Currency.h"
#include "Logging/ConsoleLogger.h"

namespace
{
  cn::Currency makeCurrency()
  {
    static logging::ConsoleLogger logger;
    return cn::CurrencyBuilder(logger).currency();
  }

  cn::Transaction makeCoinbase(uint32_t height, uint64_t amount)
  {
    cn::Transaction tx;
    tx.version = cn::TRANSACTION_VERSION_1;
    tx.unlockTime = height + cn::parameters::CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW;

    cn::BaseInput input;
    input.blockIndex = height;
    tx.inputs.push_back(input);

    cn::TransactionOutput output;
    output.amount = amount;
    output.target = cn::KeyOutput();
    tx.outputs.push_back(output);
    return tx;
  }

  cn::Block makeBlock(uint32_t height, uint64_t amount)
  {
    cn::Block block;
    block.majorVersion = cn::BLOCK_MAJOR_VERSION_1;
    block.minorVersion = 0;
    block.timestamp = height + 1;
    block.nonce = 0;
    block.baseTransaction = makeCoinbase(height, amount);
    return block;
  }
}

TEST(ChainAudit, FlagsCoinbaseMultisigDepositOutput)
{
  cn::Currency currency = makeCurrency();
  cn::Block block = makeBlock(1, 6000000);
  block.baseTransaction.version = cn::TRANSACTION_VERSION_2;

  cn::MultisignatureOutput deposit;
  deposit.requiredSignatureCount = 1;
  deposit.term = UINT32_MAX;
  deposit.keys.push_back(cn::NULL_PUBLIC_KEY);
  block.baseTransaction.outputs[0].target = deposit;

  std::vector<cn::Transaction> txs;
  cn::chain_audit::BlockAuditContext context;
  context.height = 1;
  context.blockHash = cn::get_block_hash(block);
  context.previousGeneratedCoins = 0;
  context.storedGeneratedCoins = 0;
  context.medianBlockSize = 0;
  context.cumulativeBlockSize = cn::chain_audit::cumulativeBlockSize(block, txs);
  context.inCheckpointZone = false;

  std::vector<cn::chain_audit::Finding> findings;
  cn::chain_audit::auditBlock(currency, block, txs, context, findings);

  bool sawCoinbase = false;
  bool sawInvalidTerm = false;
  for (const auto &finding : findings)
  {
    sawCoinbase = sawCoinbase || finding.code == "COINBASE_NON_STANDARD_OUTPUT";
    sawInvalidTerm = sawInvalidTerm || finding.code == "MSIG_OUTPUT_INVALID_TERM_OR_AMOUNT";
  }

  ASSERT_TRUE(sawCoinbase);
  ASSERT_TRUE(sawInvalidTerm);
}

TEST(ChainAudit, ClassifiesToleratedCoinbaseRewardDustAsInfo)
{
  cn::Currency currency = makeCurrency();
  cn::Block block = makeBlock(2, cn::MAX_BLOCK_REWARD_V1);

  std::vector<cn::Transaction> txs;
  uint64_t expectedReward = 0;
  int64_t emissionChange = 0;
  ASSERT_TRUE(currency.getBlockReward(0, cn::chain_audit::cumulativeBlockSize(block, txs),
                                      0, 0, 2, expectedReward, emissionChange));
  block.baseTransaction.outputs[0].amount = expectedReward + 2;

  cn::chain_audit::BlockAuditContext context;
  context.height = 2;
  context.blockHash = cn::get_block_hash(block);
  context.previousGeneratedCoins = 0;
  context.storedGeneratedCoins = static_cast<uint64_t>(emissionChange);
  context.medianBlockSize = 0;
  context.cumulativeBlockSize = cn::chain_audit::cumulativeBlockSize(block, txs);
  context.inCheckpointZone = false;

  std::vector<cn::chain_audit::Finding> findings;
  cn::chain_audit::auditBlock(currency, block, txs, context, findings);

  bool sawOverclaim = false;
  const cn::chain_audit::Finding *rewardFinding = nullptr;
  for (const auto &finding : findings)
  {
    if (finding.code == "COINBASE_REWARD_TOLERATED_DUST")
    {
      sawOverclaim = true;
      rewardFinding = &finding;
    }
  }

  ASSERT_TRUE(sawOverclaim);
  ASSERT_NE(nullptr, rewardFinding);
  ASSERT_EQ(cn::chain_audit::Severity::Info, rewardFinding->severity);
  ASSERT_TRUE(rewardFinding->hasAmountDelta);
  ASSERT_EQ(UINT64_C(2), rewardFinding->deltaAmount);
  ASSERT_EQ(UINT64_C(10), rewardFinding->consensusTolerance);
  ASSERT_TRUE(rewardFinding->consensusAccepted);
}

TEST(ChainAudit, FlagsConsensusRejectableCoinbaseRewardOverclaim)
{
  cn::Currency currency = makeCurrency();
  cn::Block block = makeBlock(2, cn::MAX_BLOCK_REWARD_V1);

  std::vector<cn::Transaction> txs;
  uint64_t expectedReward = 0;
  int64_t emissionChange = 0;
  ASSERT_TRUE(currency.getBlockReward(0, cn::chain_audit::cumulativeBlockSize(block, txs),
                                      0, 0, 2, expectedReward, emissionChange));
  block.baseTransaction.outputs[0].amount = expectedReward + 11;

  cn::chain_audit::BlockAuditContext context;
  context.height = 2;
  context.blockHash = cn::get_block_hash(block);
  context.previousGeneratedCoins = 0;
  context.storedGeneratedCoins = static_cast<uint64_t>(emissionChange);
  context.medianBlockSize = 0;
  context.cumulativeBlockSize = cn::chain_audit::cumulativeBlockSize(block, txs);
  context.inCheckpointZone = false;

  std::vector<cn::chain_audit::Finding> findings;
  cn::chain_audit::auditBlock(currency, block, txs, context, findings);

  const cn::chain_audit::Finding *rewardFinding = nullptr;
  for (const auto &finding : findings)
  {
    if (finding.code == "COINBASE_REWARD_OVERCLAIM")
    {
      rewardFinding = &finding;
    }
  }

  ASSERT_NE(nullptr, rewardFinding);
  ASSERT_EQ(cn::chain_audit::Severity::Critical, rewardFinding->severity);
  ASSERT_TRUE(rewardFinding->hasAmountDelta);
  ASSERT_EQ(UINT64_C(11), rewardFinding->deltaAmount);
  ASSERT_EQ(UINT64_C(10), rewardFinding->consensusTolerance);
  ASSERT_FALSE(rewardFinding->consensusAccepted);
}

TEST(ChainAudit, FlagsCheckpointZoneClassicalSpendForReferencePass)
{
  cn::Currency currency = makeCurrency();
  cn::Block block = makeBlock(10, cn::MAX_BLOCK_REWARD_V1);

  cn::Transaction tx;
  tx.version = cn::TRANSACTION_VERSION_1;
  cn::KeyInput input = boost::value_initialized<cn::KeyInput>();
  input.amount = 100;
  input.outputIndexes.push_back(0);
  tx.inputs.push_back(input);
  cn::TransactionOutput output;
  output.amount = 90;
  output.target = cn::KeyOutput();
  tx.outputs.push_back(output);

  std::vector<cn::Transaction> txs;
  txs.push_back(tx);

  cn::chain_audit::BlockAuditContext context;
  context.height = 10;
  context.blockHash = cn::get_block_hash(block);
  context.previousGeneratedCoins = 0;
  context.storedGeneratedCoins = 0;
  context.medianBlockSize = 0;
  context.cumulativeBlockSize = cn::chain_audit::cumulativeBlockSize(block, txs);
  context.inCheckpointZone = true;

  std::vector<cn::chain_audit::Finding> findings;
  cn::chain_audit::auditBlock(currency, block, txs, context, findings);

  bool sawCheckpointClassical = false;
  for (const auto &finding : findings)
  {
    sawCheckpointClassical = sawCheckpointClassical ||
                             finding.code == "CHECKPOINT_ZONE_CLASSICAL_SPEND";
  }

  ASSERT_TRUE(sawCheckpointClassical);
}

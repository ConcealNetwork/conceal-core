// Copyright (c) 2018-2026 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license.

#include "gtest/gtest.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>

#include <boost/filesystem.hpp>
#include <boost/utility/value_init.hpp>

#include "SecureTempDirectory.h"

#include "Common/StringTools.h"
#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/Checkpoints.h"
#include "CryptoNoteCore/Core.h"
#include "CryptoNoteCore/CoreConfig.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/MinerConfig.h"
#include "Logging/ConsoleLogger.h"

namespace
{
  const char ZERO_HASH_HEX[] =
      "0000000000000000000000000000000000000000000000000000000000000000";

  class L1MonetarySafety : public ::testing::Test
  {
  protected:
    L1MonetarySafety()
        : currency(cn::CurrencyBuilder(logger).currency())
    {
    }

    void SetUp() override
    {
      ASSERT_NO_THROW(
          dataDir = unit_test::createSecureTempDirectory("ccx-l1-money-safety-"));

      miner.generate();

      cn::CoreConfig config;
      config.configFolder = dataDir.string();
      config.configFolderDefaulted = false;
      config.testnet = false;

      cn::MinerConfig minerConfig;
      node.reset(new cn::core(currency, nullptr, logger, false, false));
      ASSERT_TRUE(node->init(config, minerConfig, false));
    }

    void TearDown() override
    {
      if (node)
      {
        EXPECT_TRUE(node->deinit());
        node.reset();
      }

      boost::system::error_code ec;
      boost::filesystem::remove_all(dataDir, ec);
    }

    bool makeBlockTemplate(cn::Block &block)
    {
      cn::difficulty_type difficulty = 0;
      uint32_t height = 0;
      return node->get_block_template(
          block, miner.getAccountKeys().address, difficulty, height,
          cn::BinaryArray());
    }

    void setCheckpoints(uint32_t exactHeight, const crypto::Hash &exactHash,
                        uint32_t futureHeight = 0)
    {
      cn::Checkpoints checkpoints(logger);
      ASSERT_TRUE(checkpoints.add_checkpoint(
          exactHeight, common::podToHex(exactHash)));
      if (futureHeight != 0)
      {
        ASSERT_TRUE(checkpoints.add_checkpoint(futureHeight, ZERO_HASH_HEX));
      }
      node->set_checkpoints(std::move(checkpoints));
    }

    bool submitBlock(const cn::Block &block,
                     cn::block_verification_context &bvc)
    {
      bvc = boost::value_initialized<cn::block_verification_context>();
      return node->handle_incoming_block_blob(
          cn::toBinaryArray(block), bvc, false, false);
    }

    bool acceptedMainChainBlock(
        bool handled, const cn::block_verification_context &bvc) const
    {
      return handled && bvc.m_added_to_main_chain &&
             !bvc.m_verification_failed;
    }

    logging::ConsoleLogger logger;
    cn::Currency currency;
    boost::filesystem::path dataDir;
    cn::AccountBase miner;
    std::unique_ptr<cn::core> node;
  };
}

TEST_F(L1MonetarySafety, RejectsInvalidCoinbaseDepositOutput)
{
  cn::Block block;
  ASSERT_TRUE(makeBlockTemplate(block));
  ASSERT_FALSE(block.baseTransaction.outputs.empty());
  ASSERT_EQ(1u, cn::get_block_height(block));

  cn::TransactionOutput &firstOutput = block.baseTransaction.outputs.front();
  ASSERT_EQ(typeid(cn::KeyOutput), firstOutput.target.type());

  cn::MultisignatureOutput malformedDeposit;
  malformedDeposit.keys.push_back(
      boost::get<cn::KeyOutput>(firstOutput.target).key);
  malformedDeposit.requiredSignatureCount = 1;
  malformedDeposit.term = std::numeric_limits<uint32_t>::max();

  block.baseTransaction.version = cn::TRANSACTION_VERSION_2;
  firstOutput.target = malformedDeposit;
  setCheckpoints(1, cn::get_block_hash(block));

  cn::block_verification_context bvc;
  const bool handled = submitBlock(block, bvc);

  EXPECT_FALSE(acceptedMainChainBlock(handled, bvc));
  EXPECT_TRUE(bvc.m_verification_failed);
}

TEST_F(L1MonetarySafety, RejectsForgedKeyInputInsideCheckpointZone)
{
  cn::Checkpoints futureCheckpoint(logger);
  ASSERT_TRUE(futureCheckpoint.add_checkpoint(100, ZERO_HASH_HEX));
  node->set_checkpoints(std::move(futureCheckpoint));

  cn::AccountBase recipient;
  recipient.generate();

  const uint64_t inputAmount = currency.minimumFee() + 5000;
  const uint64_t outputAmount = inputAmount - currency.minimumFee();

  cn::KeyInput forgedInput;
  forgedInput.amount = inputAmount;
  forgedInput.outputIndexes.push_back(0);
  std::memset(&forgedInput.keyImage, 0x42, sizeof(forgedInput.keyImage));

  cn::KeyOutput outputKey;
  outputKey.key = recipient.getAccountKeys().address.spendPublicKey;

  cn::Transaction forgedTx;
  forgedTx.version = cn::TRANSACTION_VERSION_1;
  forgedTx.inputs.push_back(forgedInput);
  forgedTx.outputs.push_back(cn::TransactionOutput{outputAmount, outputKey});
  forgedTx.signatures.resize(1);
  forgedTx.signatures[0].resize(1);

  cn::tx_verification_context tvc =
      boost::value_initialized<cn::tx_verification_context>();
  const bool txHandled =
      node->handle_incoming_tx(cn::toBinaryArray(forgedTx), tvc, false);

  if (!txHandled || tvc.m_verification_failed)
  {
    SUCCEED() << "forged checkpoint-zone transaction rejected at admission";
    return;
  }

  ASSERT_TRUE(tvc.m_added_to_pool);

  const crypto::Hash forgedTxHash = cn::getObjectHash(forgedTx);
  cn::Block block;
  ASSERT_TRUE(makeBlockTemplate(block));
  ASSERT_NE(block.transactionHashes.end(),
            std::find(block.transactionHashes.begin(),
                      block.transactionHashes.end(), forgedTxHash));

  setCheckpoints(1, cn::get_block_hash(block), 100);

  cn::block_verification_context bvc;
  const bool handled = submitBlock(block, bvc);

  EXPECT_FALSE(acceptedMainChainBlock(handled, bvc));
  EXPECT_TRUE(bvc.m_verification_failed);
}

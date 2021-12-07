// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "Chaingen.h"

const uint64_t UNDEF_HEIGHT = static_cast<uint64_t>(cn::UpgradeDetectorBase::UNDEF_HEIGHT);

class CheckBlockPurged : public test_chain_unit_base {
public:
  CheckBlockPurged(size_t invalidBlockIdx, uint8_t blockMajorVersion) :
    m_invalidBlockIdx(invalidBlockIdx),
    m_blockMajorVersion(blockMajorVersion) {
    assert(blockMajorVersion == cn::BLOCK_MAJOR_VERSION_1 || blockMajorVersion == cn::BLOCK_MAJOR_VERSION_2);

    cn::CurrencyBuilder currencyBuilder(m_logger);
    currencyBuilder.upgradeHeightV2(blockMajorVersion == cn::BLOCK_MAJOR_VERSION_1 ? UNDEF_HEIGHT : UINT64_C(0));
    m_currency = currencyBuilder.currency();

    REGISTER_CALLBACK("check_block_purged", CheckBlockPurged::check_block_purged);
    REGISTER_CALLBACK("markInvalidBlock", CheckBlockPurged::markInvalidBlock);
  }

  bool check_block_verification_context(const cn::block_verification_context& bvc, size_t eventIdx, const cn::Block& /*blk*/) {
    if (m_invalidBlockIdx == eventIdx) {
      return bvc.m_verification_failed;
    } else {
      return !bvc.m_verification_failed;
    }
  }

  bool check_block_purged(cn::core& c, size_t eventIdx, const std::vector<test_event_entry>& events) {
    DEFINE_TESTS_ERROR_CONTEXT("CheckBlockPurged::check_block_purged");

    CHECK_TEST_CONDITION(m_invalidBlockIdx < eventIdx);
    CHECK_EQ(0, c.get_pool_transactions_count());
    CHECK_EQ(m_invalidBlockIdx, c.get_current_blockchain_height());

    return true;
  }

  bool markInvalidBlock(cn::core& c, size_t eventIdx, const std::vector<test_event_entry>& events) {
    m_invalidBlockIdx = eventIdx + 1;
    return true;
  }

protected:
  size_t m_invalidBlockIdx;
  const uint8_t m_blockMajorVersion;
};


struct CheckBlockAccepted : public test_chain_unit_base {
  CheckBlockAccepted(size_t expectedBlockchainHeight, uint8_t blockMajorVersion) :
    m_expectedBlockchainHeight(expectedBlockchainHeight),
    m_blockMajorVersion(blockMajorVersion) {
    assert(blockMajorVersion == cn::BLOCK_MAJOR_VERSION_1 || blockMajorVersion == cn::BLOCK_MAJOR_VERSION_2);

    cn::CurrencyBuilder currencyBuilder(m_logger);
    currencyBuilder.upgradeHeightV2(blockMajorVersion == cn::BLOCK_MAJOR_VERSION_1 ? UNDEF_HEIGHT : UINT64_C(0));
    m_currency = currencyBuilder.currency();

    REGISTER_CALLBACK("check_block_accepted", CheckBlockAccepted::check_block_accepted);
  }

  bool check_block_accepted(cn::core& c, size_t /*eventIdx*/, const std::vector<test_event_entry>& /*events*/) {
    DEFINE_TESTS_ERROR_CONTEXT("CheckBlockAccepted::check_block_accepted");

    CHECK_EQ(0, c.get_pool_transactions_count());
    CHECK_EQ(m_expectedBlockchainHeight, c.get_current_blockchain_height());

    return true;
  }

protected:
  size_t m_expectedBlockchainHeight;
  const uint8_t m_blockMajorVersion;
};


struct TestBlockMajorVersionAccepted : public CheckBlockAccepted {
  TestBlockMajorVersionAccepted(uint8_t blockMajorVersion) :
    CheckBlockAccepted(2, blockMajorVersion) {}

  bool generate(std::vector<test_event_entry>& events) const;
};

struct TestBlockMajorVersionRejected : public CheckBlockPurged {
  TestBlockMajorVersionRejected(uint8_t blockAcceptedVersion, uint8_t blockGeneratedVersion) :
    CheckBlockPurged(1, blockAcceptedVersion), m_blockGeneratedVersion(blockGeneratedVersion) {}

  const uint8_t m_blockGeneratedVersion;

  bool generate(std::vector<test_event_entry>& events) const;
};

struct TestBlockBigMinorVersion : public CheckBlockAccepted {

  TestBlockBigMinorVersion(uint8_t blockMajorVersion)
    : CheckBlockAccepted(2, blockMajorVersion) {}

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_ts_not_checked : public CheckBlockAccepted
{
  gen_block_ts_not_checked(uint8_t blockMajorVersion)
    : CheckBlockAccepted(0, blockMajorVersion) {
    m_expectedBlockchainHeight = m_currency.timestampCheckWindow();
  }

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_ts_in_past : public CheckBlockPurged
{
  gen_block_ts_in_past(uint8_t blockMajorVersion)
    : CheckBlockPurged(0, blockMajorVersion) {
    m_invalidBlockIdx = m_currency.timestampCheckWindow();
  }

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_ts_in_future_rejected : public CheckBlockPurged
{
  gen_block_ts_in_future_rejected(uint8_t blockMajorVersion)
    : CheckBlockPurged(1, blockMajorVersion) {}

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_ts_in_future_accepted : public CheckBlockAccepted
{
  gen_block_ts_in_future_accepted(uint8_t blockMajorVersion)
    : CheckBlockAccepted(2, blockMajorVersion) {}

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_invalid_prev_id : public CheckBlockPurged
{
  gen_block_invalid_prev_id(uint8_t blockMajorVersion)
    : CheckBlockPurged(1, blockMajorVersion) {}

  bool generate(std::vector<test_event_entry>& events) const;
  bool check_block_verification_context(const cn::block_verification_context& bvc, size_t event_idx, const cn::Block& /*blk*/);
};

struct gen_block_invalid_nonce : public CheckBlockPurged
{
  gen_block_invalid_nonce(uint8_t blockMajorVersion)
    : CheckBlockPurged(3, blockMajorVersion) {}

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_no_miner_tx : public CheckBlockPurged
{
  gen_block_no_miner_tx(uint8_t blockMajorVersion)
    : CheckBlockPurged(1, blockMajorVersion) {}

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_unlock_time_is_low : public CheckBlockPurged
{
  gen_block_unlock_time_is_low(uint8_t blockMajorVersion)
    : CheckBlockPurged(1, blockMajorVersion) {}

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_unlock_time_is_high : public CheckBlockPurged
{
  gen_block_unlock_time_is_high(uint8_t blockMajorVersion)
    : CheckBlockPurged(1, blockMajorVersion) {}

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_unlock_time_is_timestamp_in_past : public CheckBlockPurged
{
  gen_block_unlock_time_is_timestamp_in_past(uint8_t blockMajorVersion)
    : CheckBlockPurged(1, blockMajorVersion) {}

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_unlock_time_is_timestamp_in_future : public CheckBlockPurged
{
  gen_block_unlock_time_is_timestamp_in_future(uint8_t blockMajorVersion)
    : CheckBlockPurged(1, blockMajorVersion) {}

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_height_is_low : public CheckBlockPurged
{
  gen_block_height_is_low(uint8_t blockMajorVersion)
    : CheckBlockPurged(1, blockMajorVersion) {}

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_height_is_high : public CheckBlockPurged
{
  gen_block_height_is_high(uint8_t blockMajorVersion)
    : CheckBlockPurged(1, blockMajorVersion) {}

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_miner_tx_has_2_tx_gen_in : public CheckBlockPurged
{
  gen_block_miner_tx_has_2_tx_gen_in(uint8_t blockMajorVersion)
    : CheckBlockPurged(1, blockMajorVersion) {}

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_miner_tx_has_2_in : public CheckBlockPurged
{
  gen_block_miner_tx_has_2_in(uint8_t blockMajorVersion)
    : CheckBlockPurged(0, blockMajorVersion) {
    m_invalidBlockIdx = m_currency.minedMoneyUnlockWindow() + 1;
  }

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_miner_tx_with_txin_to_key : public CheckBlockPurged
{
  gen_block_miner_tx_with_txin_to_key(uint8_t blockMajorVersion)
    : CheckBlockPurged(0, blockMajorVersion) {
    m_invalidBlockIdx = m_currency.minedMoneyUnlockWindow() + 2;
  }

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_miner_tx_out_is_small : public CheckBlockPurged
{
  gen_block_miner_tx_out_is_small(uint8_t blockMajorVersion)
    : CheckBlockPurged(1, blockMajorVersion) {}

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_miner_tx_out_is_big : public CheckBlockPurged
{
  gen_block_miner_tx_out_is_big(uint8_t blockMajorVersion)
    : CheckBlockPurged(1, blockMajorVersion) {}

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_miner_tx_has_no_out : public CheckBlockPurged
{
  gen_block_miner_tx_has_no_out(uint8_t blockMajorVersion)
    : CheckBlockPurged(1, blockMajorVersion) {}

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_miner_tx_has_out_to_alice : public CheckBlockAccepted
{
  gen_block_miner_tx_has_out_to_alice(uint8_t blockMajorVersion)
    : CheckBlockAccepted(2, blockMajorVersion) {}

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_has_invalid_tx : public CheckBlockPurged
{
  gen_block_has_invalid_tx(uint8_t blockMajorVersion)
    : CheckBlockPurged(1, blockMajorVersion) {}

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_is_too_big : public CheckBlockPurged
{
  gen_block_is_too_big(uint8_t blockMajorVersion)
      : CheckBlockPurged(1, blockMajorVersion) {
    cn::CurrencyBuilder currencyBuilder(m_logger);
    currencyBuilder.upgradeHeightV2(blockMajorVersion == cn::BLOCK_MAJOR_VERSION_1 ? UNDEF_HEIGHT : UINT64_C(0));
    currencyBuilder.maxBlockSizeInitial(std::numeric_limits<size_t>::max() / 2);
    m_currency = currencyBuilder.currency();
  }

  bool generate(std::vector<test_event_entry>& events) const;
};

struct TestBlockCumulativeSizeExceedsLimit : public CheckBlockPurged {
  TestBlockCumulativeSizeExceedsLimit(uint8_t blockMajorVersion)
    : CheckBlockPurged(std::numeric_limits<size_t>::max(), blockMajorVersion) {
  }

  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_block_invalid_binary_format : public test_chain_unit_base
{
  gen_block_invalid_binary_format(uint8_t blockMajorVersion);

  bool generate(std::vector<test_event_entry>& events) const;
  bool check_block_verification_context(const cn::block_verification_context& bvc, size_t event_idx, const cn::Block& /*blk*/);
  bool check_all_blocks_purged(cn::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool corrupt_blocks_boundary(cn::core& c, size_t ev_index, const std::vector<test_event_entry>& events);

private:
  const uint8_t m_blockMajorVersion;
  size_t m_corrupt_blocks_begin_idx;
};

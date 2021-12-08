// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <list>
#include <vector>
#include <unordered_map>

#include "crypto/hash.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/Difficulty.h"


class test_generator
{
public:
  struct BlockInfo {
    BlockInfo()
      : previousBlockHash()
      , alreadyGeneratedCoins(0)
      , blockSize(0) {
    }

    BlockInfo(crypto::Hash aPrevId, uint64_t anAlreadyGeneratedCoins, size_t aBlockSize)
      : previousBlockHash(aPrevId)
      , alreadyGeneratedCoins(anAlreadyGeneratedCoins)
      , blockSize(aBlockSize) {
    }

    crypto::Hash previousBlockHash;
    uint64_t alreadyGeneratedCoins;
    size_t blockSize;
  };

  enum BlockFields {
    bf_none      = 0,
    bf_major_ver = 1 << 0,
    bf_minor_ver = 1 << 1,
    bf_timestamp = 1 << 2,
    bf_prev_id   = 1 << 3,
    bf_miner_tx  = 1 << 4,
    bf_tx_hashes = 1 << 5,
    bf_diffic    = 1 << 6
  };

  test_generator(const cn::Currency& currency, uint8_t majorVersion = cn::BLOCK_MAJOR_VERSION_1,
                 uint8_t minorVersion = cn::BLOCK_MINOR_VERSION_0)
    : m_currency(currency), defaultMajorVersion(majorVersion), defaultMinorVersion(minorVersion) {
  }


  uint8_t defaultMajorVersion;
  uint8_t defaultMinorVersion;

  const cn::Currency& currency() const { return m_currency; }

  void getBlockchain(std::vector<BlockInfo>& blockchain, const crypto::Hash& head, size_t n) const;
  void getLastNBlockSizes(std::vector<size_t>& blockSizes, const crypto::Hash& head, size_t n) const;
  uint64_t getAlreadyGeneratedCoins(const crypto::Hash& blockId) const;
  uint64_t getAlreadyGeneratedCoins(const cn::Block& blk) const;

  void addBlock(const cn::Block& blk, size_t tsxSize, uint64_t fee, std::vector<size_t>& blockSizes,
    uint64_t alreadyGeneratedCoins);
  bool constructBlock(cn::Block& blk, uint32_t height, const crypto::Hash& previousBlockHash,
    const cn::AccountBase& minerAcc, uint64_t timestamp, uint64_t alreadyGeneratedCoins,
    std::vector<size_t>& blockSizes, const std::list<cn::Transaction>& txList);
  bool constructBlock(cn::Block& blk, const cn::AccountBase& minerAcc, uint64_t timestamp);
  bool constructBlock(cn::Block& blk, const cn::Block& blkPrev, const cn::AccountBase& minerAcc,
    const std::list<cn::Transaction>& txList = std::list<cn::Transaction>());

  bool constructBlockManually(cn::Block& blk, const cn::Block& prevBlock,
    const cn::AccountBase& minerAcc, int actualParams = bf_none, uint8_t majorVer = 0,
    uint8_t minorVer = 0, uint64_t timestamp = 0, const crypto::Hash& previousBlockHash = crypto::Hash(),
    const cn::difficulty_type& diffic = 1, const cn::Transaction& baseTransaction = cn::Transaction(),
    const std::vector<crypto::Hash>& transactionHashes = std::vector<crypto::Hash>(), size_t txsSizes = 0, uint64_t fee = 0);
  bool constructBlockManuallyTx(cn::Block& blk, const cn::Block& prevBlock,
    const cn::AccountBase& minerAcc, const std::vector<crypto::Hash>& transactionHashes, size_t txsSize);
  bool constructMaxSizeBlock(cn::Block& blk, const cn::Block& blkPrev,
    const cn::AccountBase& minerAccount, size_t medianBlockCount = 0,
    const std::list<cn::Transaction>& txList = std::list<cn::Transaction>());

private:
  const cn::Currency& m_currency;
  std::unordered_map<crypto::Hash, BlockInfo> m_blocksInfo;
};

inline cn::difficulty_type getTestDifficulty() { return 1; }
void fillNonce(cn::Block& blk, const cn::difficulty_type& diffic);

bool constructMinerTxManually(const cn::Currency& currency, uint32_t height, uint64_t alreadyGeneratedCoins,
  const cn::AccountPublicAddress& minerAddress, cn::Transaction& tx, uint64_t fee,
  cn::KeyPair* pTxKey = 0);
bool constructMinerTxBySize(const cn::Currency& currency, cn::Transaction& baseTransaction, uint32_t height,
  uint64_t alreadyGeneratedCoins, const cn::AccountPublicAddress& minerAddress,
  std::vector<size_t>& blockSizes, size_t targetTxSize, size_t targetBlockSize, uint64_t fee = 0);

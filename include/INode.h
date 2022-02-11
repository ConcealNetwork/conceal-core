// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2022 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <functional>
#include <system_error>
#include <vector>

#include "crypto/crypto.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolDefinitions.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"

#include "BlockchainExplorerData.h"
#include "ITransaction.h"

namespace cn {

class INodeObserver {
public:
  virtual ~INodeObserver() {}
  virtual void peerCountUpdated(size_t count) {}
  virtual void localBlockchainUpdated(uint32_t height) {}
  virtual void lastKnownBlockHeightUpdated(uint32_t height) {}
  virtual void poolChanged() {}
  virtual void blockchainSynchronized(uint32_t topHeight) {}
};

struct OutEntry {
  uint32_t outGlobalIndex;
  crypto::PublicKey outKey;
};

struct OutsForAmount {
  uint64_t amount;
  std::vector<OutEntry> outs;
};

struct TransactionShortInfo {
  crypto::Hash txId;
  TransactionPrefix txPrefix;
};

struct BlockShortEntry {
  crypto::Hash blockHash;
  bool hasBlock;
  cn::Block block;
  std::vector<TransactionShortInfo> txsShortInfo;
};

class INode {
public:
  typedef std::function<void(std::error_code)> Callback;

  virtual ~INode() {}
  virtual bool addObserver(INodeObserver* observer) = 0;
  virtual bool removeObserver(INodeObserver* observer) = 0;

  virtual void init(const Callback& callback) = 0;
  virtual bool shutdown() = 0;

  virtual size_t getPeerCount() const = 0;
  virtual uint32_t getLastLocalBlockHeight() const = 0;
  virtual uint32_t getLastKnownBlockHeight() const = 0;
  virtual uint32_t getLocalBlockCount() const = 0;
  virtual uint32_t getKnownBlockCount() const = 0;
  virtual uint64_t getLastLocalBlockTimestamp() const = 0;

  virtual void relayTransaction(const Transaction& transaction, const Callback& callback) = 0;
  virtual void getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount, std::vector<cn::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback) = 0;
  virtual void getNewBlocks(std::vector<crypto::Hash>&& knownBlockIds, std::vector<cn::block_complete_entry>& newBlocks, uint32_t& startHeight, const Callback& callback) = 0;
  virtual void getTransactionOutsGlobalIndices(const crypto::Hash& transactionHash, std::vector<uint32_t>& outsGlobalIndices, const Callback& callback) = 0;
  virtual void queryBlocks(std::vector<crypto::Hash>&& knownBlockIds, uint64_t timestamp, std::vector<BlockShortEntry>& newBlocks, uint32_t& startHeight, const Callback& callback) = 0;
  virtual void getPoolSymmetricDifference(std::vector<crypto::Hash>&& knownPoolTxIds, crypto::Hash knownBlockId, bool& isBcActual, std::vector<std::unique_ptr<ITransactionReader>>& newTxs, std::vector<crypto::Hash>& deletedTxIds, const Callback& callback) = 0;
  virtual void getMultisignatureOutputByGlobalIndex(uint64_t amount, uint32_t gindex, MultisignatureOutput& out, const Callback& callback) = 0;
  virtual void getTransaction(const crypto::Hash &transactionHash, cn::Transaction &transaction, const Callback &callback) = 0;
  virtual void getBlocks(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks, const Callback& callback) = 0;
  virtual void getBlocks(const std::vector<crypto::Hash>& blockHashes, std::vector<BlockDetails>& blocks, const Callback& callback) = 0;
  virtual void getBlocks(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<BlockDetails>& blocks, uint32_t& blocksNumberWithinTimestamps, const Callback& callback) = 0;
  virtual void getTransactions(const std::vector<crypto::Hash>& transactionHashes, std::vector<TransactionDetails>& transactions, const Callback& callback) = 0;
  virtual void getTransactionsByPaymentId(const crypto::Hash& paymentId, std::vector<TransactionDetails>& transactions, const Callback& callback) = 0;
  virtual void getPoolTransactions(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<TransactionDetails>& transactions, uint64_t& transactionsNumberWithinTimestamps, const Callback& callback) = 0;
  virtual void isSynchronized(bool& syncStatus, const Callback& callback) = 0;
};

}

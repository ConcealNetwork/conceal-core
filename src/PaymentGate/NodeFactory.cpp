// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "NodeFactory.h"

#include "NodeRpcProxy/NodeRpcProxy.h"
#include <memory>
#include <future>

namespace payment_service {

class NodeRpcStub: public cn::INode {
public:
  ~NodeRpcStub() override = default;
  bool addObserver(cn::INodeObserver* observer) override { return true; }
  bool removeObserver(cn::INodeObserver* observer) override { return true; }

  void init(const Callback& callback) override { }
  bool shutdown() override { return true; }

  size_t getPeerCount() const override { return 0; }
  uint32_t getLastLocalBlockHeight() const override { return 0; }
  uint32_t getLastKnownBlockHeight() const override { return 0; }
  uint32_t getLocalBlockCount() const override { return 0; }
  uint32_t getKnownBlockCount() const override { return 0; }
  uint64_t getLastLocalBlockTimestamp() const override { return 0; }

  void relayTransaction(const cn::Transaction& transaction, const Callback& callback) override { callback(std::error_code()); }
  void getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount,
    std::vector<cn::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback) override {
  }
  void getNewBlocks(std::vector<crypto::Hash>&& knownBlockIds, std::vector<cn::block_complete_entry>& newBlocks, uint32_t& startHeight, const Callback& callback) override {
    startHeight = 0;
    callback(std::error_code());
  }
  void getTransactionOutsGlobalIndices(const crypto::Hash& transactionHash, std::vector<uint32_t>& outsGlobalIndices, const Callback& callback) override { }

  void queryBlocks(std::vector<crypto::Hash>&& knownBlockIds, uint64_t timestamp, std::vector<cn::BlockShortEntry>& newBlocks,
    uint32_t& startHeight, const Callback& callback) override {
    startHeight = 0;
    callback(std::error_code());
  };

  void getPoolSymmetricDifference(std::vector<crypto::Hash>&& knownPoolTxIds, crypto::Hash knownBlockId, bool& isBcActual,
          std::vector<std::unique_ptr<cn::ITransactionReader>>& newTxs, std::vector<crypto::Hash>& deletedTxIds, const Callback& callback) override {
    isBcActual = true;
    callback(std::error_code());
  }

  void getBlocks(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<cn::BlockDetails>>& blocks,
    const Callback& callback) override { }

  void getBlocks(const std::vector<crypto::Hash>& blockHashes, std::vector<cn::BlockDetails>& blocks,
    const Callback& callback) override { }

  void getBlocks(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<cn::BlockDetails>& blocks, uint32_t& blocksNumberWithinTimestamps,
    const Callback& callback) override { }

  void getTransactions(const std::vector<crypto::Hash>& transactionHashes, std::vector<cn::TransactionDetails>& transactions,
    const Callback& callback) override { }
  void getTransaction(const crypto::Hash &transactionHash, cn::Transaction &transaction, const Callback &callback) override {}

  void getPoolTransactions(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<cn::TransactionDetails>& transactions, uint64_t& transactionsNumberWithinTimestamps,
    const Callback& callback) override { }

  void getTransactionsByPaymentId(const crypto::Hash& paymentId, std::vector<cn::TransactionDetails>& transactions, 
    const Callback& callback) override { }

  void getMultisignatureOutputByGlobalIndex(uint64_t amount, uint32_t gindex, cn::MultisignatureOutput& out,
    const Callback& callback) override { }

  void isSynchronized(bool& syncStatus, const Callback& callback) override { }

};


class NodeInitObserver {
public:
  NodeInitObserver() {
    initFuture = initPromise.get_future();
  }

  void initCompleted(std::error_code result) {
    initPromise.set_value(result);
  }

  void waitForInitEnd() {
    std::error_code ec = initFuture.get();
    if (ec) {
      throw std::system_error(ec);
    }
    return;
  }

private:
  std::promise<std::error_code> initPromise;
  std::future<std::error_code> initFuture;
};

NodeFactory::NodeFactory() = default;

NodeFactory::~NodeFactory() = default;

cn::INode* NodeFactory::createNode(const std::string& daemonAddress, uint16_t daemonPort) {
  std::unique_ptr<cn::INode> node(new cn::NodeRpcProxy(daemonAddress, daemonPort));

  NodeInitObserver initObserver;
  node->init(std::bind(&NodeInitObserver::initCompleted, &initObserver, std::placeholders::_1));
  initObserver.waitForInitEnd();

  return node.release();
}

cn::INode* NodeFactory::createNodeStub() {
  return new NodeRpcStub();
}

}

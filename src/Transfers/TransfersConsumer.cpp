// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "TransfersConsumer.h"

#include <numeric>
#include <future>

#include "CommonTypes.h"
#include "Common/StringTools.h"
#include "Common/BlockingQueue.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/TransactionApi.h"
#include "CryptoNoteCore/TransactionExtra.h"

#include "IWallet.h"
#include "INode.h"


using namespace Crypto;
using namespace Logging;
using namespace Common;

std::unordered_set<Crypto::Hash> transactions_hash_seen;
std::unordered_set<Crypto::PublicKey> public_keys_seen;
std::mutex seen_mutex;

namespace {

using namespace CryptoNote;

void checkOutputKey(
  const KeyDerivation& derivation,
  const PublicKey& key,
  uint64_t keyIndex,
  uint64_t outputIndex,
  const std::unordered_set<PublicKey>& spendKeys,
  std::unordered_map<PublicKey, std::vector<uint32_t>>& outputs) {

  PublicKey spendKey;
  underive_public_key(derivation, keyIndex, key, spendKey);

  if (spendKeys.find(spendKey) != spendKeys.end()) {
    outputs[spendKey].push_back(static_cast<uint32_t>(outputIndex));
  }

}

void findMyOutputs(
  const ITransactionReader& tx,
  const SecretKey& viewSecretKey,
  const std::unordered_set<PublicKey>& spendKeys,
  std::unordered_map<PublicKey, std::vector<uint32_t>>& outputs) {

  auto txPublicKey = tx.getTransactionPublicKey();
  KeyDerivation derivation;

  if (!generate_key_derivation( txPublicKey, viewSecretKey, derivation)) {
    return;
  }

  uint64_t keyIndex = 0;
  uint64_t outputCount = tx.getOutputCount();

  for (uint64_t idx = 0; idx < outputCount; ++idx) {

    auto outType = tx.getOutputType(uint64_t(idx));

    if (outType == TransactionTypes::OutputType::Key) {

      uint64_t amount;
      KeyOutput out;
      tx.getOutput(idx, out, amount);
      checkOutputKey(derivation, out.key, keyIndex, idx, spendKeys, outputs);
      ++keyIndex;

    } else if (outType == TransactionTypes::OutputType::Multisignature) {

      uint64_t amount;
      MultisignatureOutput out;
      tx.getOutput(idx, out, amount);
      for (const auto& key : out.keys) {
        checkOutputKey(derivation, key, idx, idx, spendKeys, outputs);
        ++keyIndex;
     }
    }
  }
}

std::vector<Crypto::Hash> getBlockHashes(const CryptoNote::CompleteBlock* blocks, uint64_t count) {
  std::vector<Crypto::Hash> result;
  result.reserve(count);

  for (uint64_t i = 0; i < count; ++i) {
    result.push_back(blocks[i].blockHash);
  }

  return result;
}

}

namespace CryptoNote {

TransfersConsumer::TransfersConsumer(const CryptoNote::Currency& currency, INode& node, Logging::ILogger& logger, const SecretKey& viewSecret) :
  m_node(node), m_viewSecret(viewSecret), m_currency(currency), m_logger(logger, "TransfersConsumer") {
  updateSyncStart();
}

ITransfersSubscription& TransfersConsumer::addSubscription(const AccountSubscription& subscription) {
  if (subscription.keys.viewSecretKey != m_viewSecret) {
    throw std::runtime_error("TransfersConsumer: view secret key mismatch");
  }

  auto& res = m_subscriptions[subscription.keys.address.spendPublicKey];

  if (res.get() == nullptr) {
    res.reset(new TransfersSubscription(m_currency, subscription));
    m_spendKeys.insert(subscription.keys.address.spendPublicKey);
    updateSyncStart();
  }

  return *res;
}

bool TransfersConsumer::removeSubscription(const AccountPublicAddress& address) {
  m_subscriptions.erase(address.spendPublicKey);
  m_spendKeys.erase(address.spendPublicKey);
  updateSyncStart();
  return m_subscriptions.empty();
}

ITransfersSubscription* TransfersConsumer::getSubscription(const AccountPublicAddress& acc) {
  auto it = m_subscriptions.find(acc.spendPublicKey);
  return it == m_subscriptions.end() ? nullptr : it->second.get();
}

void TransfersConsumer::getSubscriptions(std::vector<AccountPublicAddress>& subscriptions) {
  for (const auto& kv : m_subscriptions) {
    subscriptions.push_back(kv.second->getAddress());
  }
}

void TransfersConsumer::initTransactionPool(const std::unordered_set<Crypto::Hash>& uncommitedTransactions) {
  for (auto itSubscriptions = m_subscriptions.begin(); itSubscriptions != m_subscriptions.end(); ++itSubscriptions) {
    std::vector<Crypto::Hash> unconfirmedTransactions;
    itSubscriptions->second->getContainer().getUnconfirmedTransactions(unconfirmedTransactions);

    for (auto itTransactions = unconfirmedTransactions.begin(); itTransactions != unconfirmedTransactions.end(); ++itTransactions) {
      if (uncommitedTransactions.count(*itTransactions) == 0) {
        m_poolTxs.insert(*itTransactions);
      }
    }
  }
}

void TransfersConsumer::updateSyncStart() {
  SynchronizationStart start;

  start.height =   std::numeric_limits<uint64_t>::max();
  start.timestamp = std::numeric_limits<uint64_t>::max();

  for (const auto& kv : m_subscriptions) {
    auto subStart = kv.second->getSyncStart();
    start.height = std::min(start.height, subStart.height);
    start.timestamp = std::min(start.timestamp, subStart.timestamp);
  }

  m_syncStart = start;
}

SynchronizationStart TransfersConsumer::getSyncStart() {
  return m_syncStart;
}

void TransfersConsumer::onBlockchainDetach(uint32_t height) {
  m_observerManager.notify(&IBlockchainConsumerObserver::onBlockchainDetach, this, height);

  for (const auto& kv : m_subscriptions) {
    kv.second->onBlockchainDetach(height);
  }
}

bool TransfersConsumer::onNewBlocks(const CompleteBlock* blocks, uint32_t startHeight, uint32_t count) {
  assert(blocks);
  assert(count > 0);

  struct Tx {
    TransactionBlockInfo blockInfo;
    const ITransactionReader* tx;
  };

  struct PreprocessedTx : Tx, PreprocessInfo {};

  std::vector<PreprocessedTx> preprocessedTransactions;
  std::mutex preprocessedTransactionsMutex;

  uint64_t workers = std::thread::hardware_concurrency();
  if (workers == 0) {
    workers = 2;
  }

  BlockingQueue<Tx> inputQueue(workers * 2);

  std::atomic<bool> stopProcessing(false);

  auto pushingThread = std::async(std::launch::async, [&] {
    for( uint32_t i = 0; i < count && !stopProcessing; ++i) {
      const auto& block = blocks[i].block;

      if (!block.is_initialized()) {
        continue;
      }

      // filter by syncStartTimestamp
      if (m_syncStart.timestamp && block->timestamp < m_syncStart.timestamp) {
        continue;
      }

      TransactionBlockInfo blockInfo;
      blockInfo.height = startHeight + i;
      blockInfo.timestamp = block->timestamp;
      blockInfo.transactionIndex = 0; // position in block

      for (const auto& tx : blocks[i].transactions) {
        auto pubKey = tx->getTransactionPublicKey();
        if (pubKey == NULL_PUBLIC_KEY) {
          ++blockInfo.transactionIndex;
          continue;
        }

        Tx item = { blockInfo, tx.get() };
        inputQueue.push(item);
        ++blockInfo.transactionIndex;
      }
    }

    inputQueue.close();
  });

  auto processingFunction = [&] {
    Tx item;
    std::error_code ec;
    while (!stopProcessing && inputQueue.pop(item)) {
      PreprocessedTx output;
      static_cast<Tx&>(output) = item;

      ec = preprocessOutputs(item.blockInfo, *item.tx, output);
      if (ec) {
        stopProcessing = true;
        break;
      }

      std::lock_guard<std::mutex> lk(preprocessedTransactionsMutex);
      preprocessedTransactions.push_back(std::move(output));
    }
    return ec;
  };

  std::vector<std::future<std::error_code>> processingThreads;
  for (uint64_t i = 0; i < workers; ++i) {
    processingThreads.push_back(std::async(std::launch::async, processingFunction));
  }

  std::error_code processingError;
  for (auto& f : processingThreads) {
    try {
      std::error_code ec = f.get();
      if (!processingError && ec) {
        processingError = ec;
      }
    } catch (const std::system_error& e) {
      processingError = e.code();
    } catch (const std::exception&) {
      processingError = std::make_error_code(std::errc::operation_canceled);
    }
  }

  std::vector<Crypto::Hash> blockHashes = getBlockHashes(blocks, count);
  if (!processingError) {
    m_observerManager.notify(&IBlockchainConsumerObserver::onBlocksAdded, this, blockHashes);

    // sort by block height and transaction index in block
    std::sort(preprocessedTransactions.begin(), preprocessedTransactions.end(), [](const PreprocessedTx& a, const PreprocessedTx& b) {
      return std::tie(a.blockInfo.height, a.blockInfo.transactionIndex) < std::tie(b.blockInfo.height, b.blockInfo.transactionIndex);
    });

    for (const auto& tx : preprocessedTransactions) {
      processTransaction(tx.blockInfo, *tx.tx, tx);
    }
  } else {
    forEachSubscription([&](TransfersSubscription& sub) {
      sub.onError(processingError, startHeight);
    });

    return false;
  }

  auto newHeight = startHeight + count - 1;
  forEachSubscription([newHeight](TransfersSubscription& sub) {
    sub.advanceHeight(newHeight);
  });

  return true;
}

std::error_code TransfersConsumer::onPoolUpdated(const std::vector<std::unique_ptr<ITransactionReader>>& addedTransactions, const std::vector<Hash>& deletedTransactions) {
  TransactionBlockInfo unconfirmedBlockInfo;
  unconfirmedBlockInfo.timestamp = 0; 
  unconfirmedBlockInfo.height = WALLET_UNCONFIRMED_TRANSACTION_HEIGHT;

  std::error_code processingError;
  for (auto& cryptonoteTransaction : addedTransactions) {
    m_poolTxs.insert(cryptonoteTransaction->getTransactionHash());
    processingError = processTransaction(unconfirmedBlockInfo, *cryptonoteTransaction.get());
    if (processingError) {
      for (auto& sub : m_subscriptions) {
        sub.second->onError(processingError, WALLET_UNCONFIRMED_TRANSACTION_HEIGHT);
      }

      return processingError;
    }
  }
  
  for (auto& deletedTxHash : deletedTransactions) {
    m_poolTxs.erase(deletedTxHash);

    m_observerManager.notify(&IBlockchainConsumerObserver::onTransactionDeleteBegin, this, deletedTxHash);
    for (auto& sub : m_subscriptions) {
      sub.second->deleteUnconfirmedTransaction(*reinterpret_cast<const Hash*>(&deletedTxHash));
    }

    m_observerManager.notify(&IBlockchainConsumerObserver::onTransactionDeleteEnd, this, deletedTxHash);
  }

  return std::error_code();
}

const std::unordered_set<Crypto::Hash>& TransfersConsumer::getKnownPoolTxIds() const {
  return m_poolTxs;
}

std::error_code TransfersConsumer::addUnconfirmedTransaction(const ITransactionReader& transaction) {
  TransactionBlockInfo unconfirmedBlockInfo;
  unconfirmedBlockInfo.height = WALLET_UNCONFIRMED_TRANSACTION_HEIGHT;
  unconfirmedBlockInfo.timestamp = 0;
  unconfirmedBlockInfo.transactionIndex = 0;

  return processTransaction(unconfirmedBlockInfo, transaction);
}

void TransfersConsumer::removeUnconfirmedTransaction(const Crypto::Hash& transactionHash) {
  m_observerManager.notify(&IBlockchainConsumerObserver::onTransactionDeleteBegin, this, transactionHash);
  for (auto& subscription : m_subscriptions) {
    subscription.second->deleteUnconfirmedTransaction(transactionHash);
  }
  m_observerManager.notify(&IBlockchainConsumerObserver::onTransactionDeleteEnd, this, transactionHash);
}

void TransfersConsumer::addPublicKeysSeen(const Crypto::Hash& transactionHash, const Crypto::PublicKey& outputKey) {
    std::lock_guard<std::mutex> lk(seen_mutex);
    transactions_hash_seen.insert(transactionHash);
    public_keys_seen.insert(outputKey);
}	
	
std::error_code createTransfers(
  const AccountKeys& account,
  const TransactionBlockInfo& blockInfo,
  const ITransactionReader& tx,
  const std::vector<uint32_t>& outputs,
  const std::vector<uint32_t>& globalIdxs,
  std::vector<TransactionOutputInformationIn>& transfers) {

  auto txPubKey = tx.getTransactionPublicKey();
  auto txHash = tx.getTransactionHash();
  std::vector<PublicKey> temp_keys;	
  std::lock_guard<std::mutex> lk(seen_mutex);	

  for (auto idx : outputs) {

    if (idx >= tx.getOutputCount()) {
      return std::make_error_code(std::errc::argument_out_of_domain);
    }

    auto outType = tx.getOutputType(uint64_t(idx));

    if (
      outType != TransactionTypes::OutputType::Key &&
      outType != TransactionTypes::OutputType::Multisignature) {
      continue;
    }

    TransactionOutputInformationIn info;

    info.type = outType;
    info.transactionPublicKey = txPubKey;
    info.outputInTransaction = idx;
    info.globalOutputIndex = (blockInfo.height == WALLET_UNCONFIRMED_TRANSACTION_HEIGHT) ?
      UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX : globalIdxs[idx];

    if (outType == TransactionTypes::OutputType::Key) {
      uint64_t amount;
      KeyOutput out;
      tx.getOutput(idx, out, amount);

      CryptoNote::KeyPair in_ephemeral;
      CryptoNote::generate_key_image_helper(
        account,
        txPubKey,
        idx,
        in_ephemeral,
        info.keyImage);

      assert(out.key == reinterpret_cast<const PublicKey&>(in_ephemeral.publicKey));

      std::unordered_set<Crypto::Hash>::iterator it = transactions_hash_seen.find(tx.getTransactionHash());
	  if (it == transactions_hash_seen.end()) {
        std::unordered_set<Crypto::PublicKey>::iterator key_it = public_keys_seen.find(out.key);
        if (key_it != public_keys_seen.end()) {
          throw std::runtime_error("duplicate transaction output key is found");
          return std::error_code();
        }
        temp_keys.push_back(out.key);		
	  }   
	     

      info.amount = amount;
      info.outputKey = out.key;

    } else if (outType == TransactionTypes::OutputType::Multisignature) {
      uint64_t amount;
      MultisignatureOutput out;
      tx.getOutput(idx, out, amount);
	    
		  for (const auto& key : out.keys) {
        std::unordered_set<Crypto::Hash>::iterator it = transactions_hash_seen.find(txHash);
        if (it == transactions_hash_seen.end()) {
          std::unordered_set<Crypto::PublicKey>::iterator key_it = public_keys_seen.find(key);
          if (key_it != public_keys_seen.end()) {
			 // m_logger(ERROR, BRIGHT_RED) << "Failed to process transaction " << Common::podToHex(txHash) << ": duplicate multisignature output key is found";
            return std::error_code();
          }
          if (std::find(temp_keys.begin(), temp_keys.end(), key) != temp_keys.end()) {
          //  m_logger(ERROR, BRIGHT_RED) << "Failed to process transaction " << Common::podToHex(txHash) << ": the same multisignature output key is present more than once";
            return std::error_code();
          }
          temp_keys.push_back(key);
        }
      }
      info.amount = amount;
      info.requiredSignatures = out.requiredSignatureCount;
      info.term = out.term;
    }
    
   transfers.push_back(info);
  }

  transactions_hash_seen.insert(tx.getTransactionHash());
  std::copy(temp_keys.begin(), temp_keys.end(), std::inserter(public_keys_seen, public_keys_seen.end()));


 
  return std::error_code();
}

std::error_code TransfersConsumer::preprocessOutputs(const TransactionBlockInfo& blockInfo, const ITransactionReader& tx, PreprocessInfo& info) {
  std::unordered_map<PublicKey, std::vector<uint32_t>> outputs;
   try {
    findMyOutputs(tx, m_viewSecret, m_spendKeys, outputs);
  }
  catch (const std::exception& e) {
    m_logger(ERROR, BRIGHT_RED) << "Failed to process transaction: " << e.what() << ", transaction hash " << Common::podToHex(tx.getTransactionHash());
    return std::error_code();
  }
	
  if (outputs.empty()) {
    return std::error_code();
  }

  std::error_code errorCode;
  auto txHash = tx.getTransactionHash();
  if (blockInfo.height != WALLET_UNCONFIRMED_TRANSACTION_HEIGHT) {
    errorCode = getGlobalIndices(reinterpret_cast<const Hash&>(txHash), info.globalIdxs);
    if (errorCode) {
      return errorCode;
    }
  }

  for (const auto& kv : outputs) {
    auto it = m_subscriptions.find(kv.first);
    if (it != m_subscriptions.end()) {
      auto& transfers = info.outputs[kv.first];
       try {
		  errorCode = createTransfers(it->second->getKeys(), blockInfo, tx, kv.second, info.globalIdxs, transfers);
		  if (errorCode) {
			  return errorCode;
		  }
	  }
	  catch (const std::exception& e) {
	    m_logger(ERROR, BRIGHT_RED) << "Failed to process transaction: " << e.what() << ", transaction hash " << Common::podToHex(tx.getTransactionHash());
		  return std::error_code();
	  }
    }
  }

  return std::error_code();
}

std::error_code TransfersConsumer::processTransaction(const TransactionBlockInfo& blockInfo, const ITransactionReader& tx) {
  PreprocessInfo info;
  auto ec = preprocessOutputs(blockInfo, tx, info);
  if (ec) {
    return ec;
  }

  processTransaction(blockInfo, tx, info);
  return std::error_code();
}

void TransfersConsumer::processTransaction(const TransactionBlockInfo& blockInfo, const ITransactionReader& tx, const PreprocessInfo& info) {
  std::vector<TransactionOutputInformationIn> emptyOutputs;
  std::vector<ITransfersContainer*> transactionContainers;
  bool someContainerUpdated = false;
  for (auto& kv : m_subscriptions) {
    auto it = info.outputs.find(kv.first);
    auto& subscriptionOutputs = (it == info.outputs.end()) ? emptyOutputs : it->second;

    bool containerContainsTx;
    bool containerUpdated;
    processOutputs(blockInfo, *kv.second, tx, subscriptionOutputs, info.globalIdxs, containerContainsTx, containerUpdated);
    someContainerUpdated = someContainerUpdated || containerUpdated;
    if (containerContainsTx) {
      transactionContainers.push_back(&kv.second->getContainer());
    }
  }

  if (someContainerUpdated) {
    m_observerManager.notify(&IBlockchainConsumerObserver::onTransactionUpdated, this, tx.getTransactionHash(), transactionContainers);
  }
}

void TransfersConsumer::processOutputs(const TransactionBlockInfo& blockInfo, TransfersSubscription& sub, const ITransactionReader& tx,
  const std::vector<TransactionOutputInformationIn>& transfers, const std::vector<uint32_t>& globalIdxs, bool& contains, bool& updated) {

  TransactionInformation subscribtionTxInfo;
  contains = sub.getContainer().getTransactionInformation(tx.getTransactionHash(), subscribtionTxInfo);
  updated = false;

  if (contains) {
    if (subscribtionTxInfo.blockHeight == WALLET_UNCONFIRMED_TRANSACTION_HEIGHT && blockInfo.height != WALLET_UNCONFIRMED_TRANSACTION_HEIGHT) {
      // pool->blockchain
      sub.markTransactionConfirmed(blockInfo, tx.getTransactionHash(), globalIdxs);
      updated = true;
    } else {
      assert(subscribtionTxInfo.blockHeight == blockInfo.height);
    }
  } else {
    auto messages = get_messages_from_extra(tx.getExtra(), tx.getTransactionPublicKey(), &sub.getKeys().spendSecretKey);
    updated = sub.addTransaction(blockInfo, tx, transfers, std::move(messages));
    contains = updated;
  }
}

std::error_code TransfersConsumer::getGlobalIndices(const Hash& transactionHash, std::vector<uint32_t>& outsGlobalIndices) {  
  std::promise<std::error_code> prom;
  std::future<std::error_code> f = prom.get_future();

  INode::Callback cb = [&prom](std::error_code ec) { 
    std::promise<std::error_code> p(std::move(prom));
    p.set_value(ec);
  };

  outsGlobalIndices.clear();
  m_node.getTransactionOutsGlobalIndices(transactionHash, outsGlobalIndices, cb);

  return f.get();
}

}
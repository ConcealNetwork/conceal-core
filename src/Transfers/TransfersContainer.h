// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2021 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <unordered_map>
#include <mutex>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include "crypto/crypto.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "CryptoNoteCore/CryptoNoteSerialization.h"
#include "CryptoNoteCore/Currency.h"
#include "Serialization/ISerializer.h"
#include "Serialization/SerializationOverloads.h"

#include "ITransaction.h"
#include "ITransfersContainer.h"

namespace cn {

struct TransactionOutputInformationIn;

struct TransactionOutputKey {
  crypto::Hash transactionHash;
  uint32_t outputInTransaction;

  size_t hash() const;
  bool operator==(const TransactionOutputKey& rhs) const {
    if (transactionHash != rhs.transactionHash) {
      return false;
    }

    if (outputInTransaction != rhs.outputInTransaction) {
      return false;
    }

    return true;
  }

  void serialize(ISerializer& s) {
    s(transactionHash, "transactionHash");
    s(outputInTransaction, "outputInTransaction");
  }
};

struct TransactionOutputKeyHasher {
  size_t operator() (const TransactionOutputKey& outputId) const {
    return outputId.hash();
  }
};

class SpentOutputDescriptor {
public:
  SpentOutputDescriptor();
  SpentOutputDescriptor(const TransactionOutputInformationIn& transactionInfo);
  SpentOutputDescriptor(const crypto::KeyImage* keyImage);
  SpentOutputDescriptor(uint64_t amount, uint32_t globalOutputIndex);

  void assign(const crypto::KeyImage* keyImage);
  void assign(uint64_t amount, uint32_t globalOutputIndex);

  bool isValid() const;

  bool operator==(const SpentOutputDescriptor& other) const;
  size_t hash() const;

private:
  transaction_types::OutputType m_type;
  union {
    const crypto::KeyImage* m_keyImage;
    struct {
      uint64_t m_amount;
      uint32_t m_globalOutputIndex;
    };
  };
};

struct SpentOutputDescriptorHasher {
  size_t operator()(const SpentOutputDescriptor& descriptor) const {
    return descriptor.hash();
  }
};

struct TransactionOutputInformationIn : public TransactionOutputInformation {
  crypto::KeyImage keyImage;  //!< \attention Used only for transaction_types::OutputType::Key
};

struct TransactionOutputInformationEx : public TransactionOutputInformationIn {
  uint64_t unlockTime;
  uint32_t blockHeight;
  uint32_t transactionIndex;
  bool visible;

  SpentOutputDescriptor getSpentOutputDescriptor() const { return SpentOutputDescriptor(*this); }
  const crypto::Hash& getTransactionHash() const { return transactionHash; }

  TransactionOutputKey getTransactionOutputKey() const { return TransactionOutputKey {transactionHash, outputInTransaction}; }

  void serialize(cn::ISerializer& s) {
    s(reinterpret_cast<uint8_t&>(type), "type");
    s(amount, "");
    serializeGlobalOutputIndex(s, globalOutputIndex, "");
    s(outputInTransaction, "");
    s(transactionPublicKey, "");
    s(keyImage, "");
    s(unlockTime, "");
    serializeBlockHeight(s, blockHeight, "");
    s(transactionIndex, "");
    s(transactionHash, "");
    s(visible, "");

    if (type == transaction_types::OutputType::Key) {
      s(outputKey, "");
    } else if (type == transaction_types::OutputType::Multisignature) {
      s(requiredSignatures, "");
      s(term, "");
    }
  }

};

struct TransactionBlockInfo {
  uint32_t height;
  uint64_t timestamp;
  uint32_t transactionIndex;

  void serialize(ISerializer& s) {
    serializeBlockHeight(s, height, "height");
    s(timestamp, "timestamp");
    s(transactionIndex, "transactionIndex");
  }
};

struct SpentTransactionOutput : TransactionOutputInformationEx {
  TransactionBlockInfo spendingBlock;
  crypto::Hash spendingTransactionHash;
  uint32_t inputInTransaction;

  const crypto::Hash& getSpendingTransactionHash() const {
    return spendingTransactionHash;
  }

  void serialize(ISerializer& s) {
    TransactionOutputInformationEx::serialize(s);
    s(spendingBlock, "spendingBlock");
    s(spendingTransactionHash, "spendingTransactionHash");
    s(inputInTransaction, "inputInTransaction");
  }
};

struct TransferUnlockJob {
  uint32_t unlockHeight;
  TransactionOutputKey transactionOutputKey;

  crypto::Hash getTransactionHash() const { return transactionOutputKey.transactionHash; }

  void serialize(ISerializer& s) {
    s(unlockHeight, "unlockHeight");
    s(transactionOutputKey, "transactionOutputId");
  }
};

enum class KeyImageState {
  Unconfirmed,
  Confirmed,
  Spent
};

struct KeyOutputInfo {
  KeyImageState state;
  size_t count;
};

class TransfersContainer : public ITransfersContainer {

public:

  TransfersContainer(const cn::Currency& currency, size_t transactionSpendableAge);

  bool addTransaction(const TransactionBlockInfo& block, const ITransactionReader& tx,
                      const std::vector<TransactionOutputInformationIn>& transfers,
                      std::vector<std::string>&& messages, std::vector<TransactionOutputInformation>* unlockingTransfers = nullptr);
  bool deleteUnconfirmedTransaction(const crypto::Hash& transactionHash);
  bool markTransactionConfirmed(const TransactionBlockInfo& block, const crypto::Hash& transactionHash, const std::vector<uint32_t>& globalIndices);

  void detach(uint32_t height, std::vector<crypto::Hash>& deletedTransactions, std::vector<TransactionOutputInformation>& lockedTransfers);
  //returns outputs that are being unlocked
  std::vector<TransactionOutputInformation> advanceHeight(uint32_t height);

  // ITransfersContainer
  virtual size_t transfersCount() const override;
  virtual size_t transactionsCount() const override;
  virtual uint64_t balance(uint32_t flags) const override;
  virtual void getOutputs(std::vector<TransactionOutputInformation>& transfers, uint32_t flags) const override;
  virtual bool getTransactionInformation(const crypto::Hash& transactionHash, TransactionInformation& info,
    uint64_t* amountIn = nullptr, uint64_t* amountOut = nullptr) const override;
  virtual std::vector<TransactionOutputInformation> getTransactionOutputs(const crypto::Hash& transactionHash, uint32_t flags) const override;
  //only type flags are feasible for this function
  virtual std::vector<TransactionOutputInformation> getTransactionInputs(const crypto::Hash& transactionHash, uint32_t flags) const override;
  virtual void getUnconfirmedTransactions(std::vector<crypto::Hash>& transactions) const override;
  virtual std::vector<TransactionSpentOutputInformation> getSpentOutputs() const override;
  virtual bool getTransfer(const crypto::Hash& transactionHash, uint32_t outputInTransaction, TransactionOutputInformation& transfer, TransferState& transferState) const override;

  // IStreamSerializable
  virtual void save(std::ostream& os) override;
  virtual void load(std::istream& in) override;

private:
  struct ContainingTransactionIndex { };
  struct SpendingTransactionIndex { };
  struct SpentOutputDescriptorIndex { };
  struct TransferUnlockHeightIndex { };
  struct TransactionOutputKeyIndex { };

  typedef boost::multi_index_container<
    TransactionInformation,
    boost::multi_index::indexed_by<
    boost::multi_index::hashed_unique<BOOST_MULTI_INDEX_MEMBER(TransactionInformation, crypto::Hash, transactionHash)>,
    boost::multi_index::ordered_non_unique < BOOST_MULTI_INDEX_MEMBER(TransactionInformation, uint32_t, blockHeight) >
    >
  > TransactionMultiIndex;

  typedef boost::multi_index_container<
    TransactionOutputInformationEx,
    boost::multi_index::indexed_by<
      boost::multi_index::hashed_non_unique<
        boost::multi_index::tag<SpentOutputDescriptorIndex>,
        boost::multi_index::const_mem_fun<
          TransactionOutputInformationEx,
          SpentOutputDescriptor,
          &TransactionOutputInformationEx::getSpentOutputDescriptor>,
        SpentOutputDescriptorHasher
      >,
      boost::multi_index::hashed_non_unique<
        boost::multi_index::tag<ContainingTransactionIndex>,
        boost::multi_index::const_mem_fun<
          TransactionOutputInformationEx,
          const crypto::Hash&,
          &TransactionOutputInformationEx::getTransactionHash>
      >,
      boost::multi_index::hashed_unique <
        boost::multi_index::tag<TransactionOutputKeyIndex>,
        boost::multi_index::const_mem_fun <
          TransactionOutputInformationEx,
          TransactionOutputKey,
          &TransactionOutputInformationEx::getTransactionOutputKey>,
        TransactionOutputKeyHasher
      >
    >
  > UnconfirmedTransfersMultiIndex;

  typedef boost::multi_index_container<
    TransactionOutputInformationEx,
    boost::multi_index::indexed_by<
      boost::multi_index::hashed_non_unique<
        boost::multi_index::tag<SpentOutputDescriptorIndex>,
        boost::multi_index::const_mem_fun<
          TransactionOutputInformationEx,
          SpentOutputDescriptor,
          &TransactionOutputInformationEx::getSpentOutputDescriptor>,
        SpentOutputDescriptorHasher
      >,
      boost::multi_index::hashed_non_unique<
        boost::multi_index::tag<ContainingTransactionIndex>,
        boost::multi_index::const_mem_fun<
          TransactionOutputInformationEx,
          const crypto::Hash&,
          &TransactionOutputInformationEx::getTransactionHash>
      >,
      boost::multi_index::hashed_unique <
        boost::multi_index::tag<TransactionOutputKeyIndex>,
        boost::multi_index::const_mem_fun <
          TransactionOutputInformationEx,
          TransactionOutputKey,
          &TransactionOutputInformationEx::getTransactionOutputKey>,
        TransactionOutputKeyHasher
      >
    >
  > AvailableTransfersMultiIndex;

  typedef boost::multi_index_container<
    SpentTransactionOutput,
    boost::multi_index::indexed_by<
      boost::multi_index::hashed_unique<
        boost::multi_index::tag<SpentOutputDescriptorIndex>,
        boost::multi_index::const_mem_fun<
    TransactionOutputInformationEx,
          SpentOutputDescriptor,
          &TransactionOutputInformationEx::getSpentOutputDescriptor>,
        SpentOutputDescriptorHasher
      >,
      boost::multi_index::hashed_non_unique<
        boost::multi_index::tag<ContainingTransactionIndex>,
        boost::multi_index::const_mem_fun<
          TransactionOutputInformationEx,
          const crypto::Hash&,
          &SpentTransactionOutput::getTransactionHash>
      >,
      boost::multi_index::hashed_non_unique <
        boost::multi_index::tag<SpendingTransactionIndex>,
        boost::multi_index::const_mem_fun <
          SpentTransactionOutput,
          const crypto::Hash&,
          &SpentTransactionOutput::getSpendingTransactionHash>
      >,
      boost::multi_index::hashed_unique <
        boost::multi_index::tag<TransactionOutputKeyIndex>,
        boost::multi_index::const_mem_fun <
          TransactionOutputInformationEx,
          TransactionOutputKey,
          &TransactionOutputInformationEx::getTransactionOutputKey>,
        TransactionOutputKeyHasher
      >
    >
  > SpentTransfersMultiIndex;

  typedef boost::multi_index_container<
    TransferUnlockJob,
    boost::multi_index::indexed_by<
      boost::multi_index::ordered_non_unique<
        boost::multi_index::tag<TransferUnlockHeightIndex>,
        BOOST_MULTI_INDEX_MEMBER(TransferUnlockJob, uint32_t, unlockHeight)
      >,
      boost::multi_index::hashed_unique<
        boost::multi_index::tag<TransactionOutputKeyIndex>,
        BOOST_MULTI_INDEX_MEMBER(TransferUnlockJob, TransactionOutputKey, transactionOutputKey),
        TransactionOutputKeyHasher
      >
    >
  > TransfersUnlockMultiIndex;

private:
  void addTransaction(const TransactionBlockInfo& block, const ITransactionReader& tx, std::vector<std::string>&& messages);
  bool addTransactionOutputs(const TransactionBlockInfo& block, const ITransactionReader& tx,
                             const std::vector<TransactionOutputInformationIn>& transfers);
  bool addTransactionInputs(const TransactionBlockInfo& block, const ITransactionReader& tx);
  void deleteTransactionTransfers(const crypto::Hash& transactionHash);
  bool isSpendTimeUnlocked(const TransactionOutputInformationEx& info) const;
  bool isIncluded(const TransactionOutputInformationEx& info, uint32_t flags) const;
  static bool isIncluded(const TransactionOutputInformationEx& output, uint32_t state, uint32_t flags);
  void updateTransfersVisibility(const crypto::KeyImage& keyImage);
  void addUnlockJob(const TransactionOutputInformationEx& output);
  void deleteUnlockJob(const TransactionOutputInformationEx& output);
  std::vector<TransactionOutputInformation> getUnlockingTransfers(uint32_t prevHeight, uint32_t currentHeight);
  void getLockingTransfers(uint32_t prevHeight, uint32_t currentHeight,
    const std::vector<crypto::Hash>& deletedTransactions, std::vector<TransactionOutputInformation>& lockingTransfers);
  TransactionOutputInformation getAvailableOutput(const TransactionOutputKey& transactionOutputKey) const;

  void copyToSpent(const TransactionBlockInfo& block, const ITransactionReader& tx, size_t inputIndex, const TransactionOutputInformationEx& output);

  void rebuildTransfersUnlockJobs(TransfersUnlockMultiIndex& transfersUnlockJobs, const AvailableTransfersMultiIndex& availableTransfers,
                                  const SpentTransfersMultiIndex& spentTransfers);
  std::vector<TransactionOutputInformation> doAdvanceHeight(uint32_t height);

private:
  TransactionMultiIndex m_transactions;
  UnconfirmedTransfersMultiIndex m_unconfirmedTransfers;
  AvailableTransfersMultiIndex m_availableTransfers;
  SpentTransfersMultiIndex m_spentTransfers;
  TransfersUnlockMultiIndex m_transfersUnlockJobs;
  //std::unordered_map<KeyImage, KeyOutputInfo, boost::hash<KeyImage>> m_keyImages;

  uint32_t m_currentHeight; // current height is needed to check if a transfer is unlocked
  size_t m_transactionSpendableAge;
  const cn::Currency& m_currency;
  mutable std::mutex m_mutex;
};

}

// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "BlockchainExplorerDataSerialization.h"

#include <stdexcept>

#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>

#include "CryptoNoteCore/CryptoNoteSerialization.h"

#include "Serialization/SerializationOverloads.h"

namespace CryptoNote {

enum class SerializationTag : uint8_t { Base = 0xff, Key = 0x2, Multisignature = 0x3, Transaction = 0xcc, Block = 0xbb };

namespace {

struct BinaryVariantTagGetter: boost::static_visitor<uint8_t> {
  uint8_t operator()(const CryptoNote::BaseInputDetails) { return static_cast<uint8_t>(SerializationTag::Base); }
  uint8_t operator()(const CryptoNote::KeyInputDetails) { return static_cast<uint8_t>(SerializationTag::Key); }
  uint8_t operator()(const CryptoNote::MultisignatureInputDetails) { return static_cast<uint8_t>(SerializationTag::Multisignature); }
};

struct VariantSerializer : boost::static_visitor<> {
  VariantSerializer(CryptoNote::ISerializer& serializer, const std::string& name) : s(serializer), name(name) {}

  template <typename T>
  void operator() (T& param) { s(param, name); }

  CryptoNote::ISerializer& s;
  const std::string name;
};

void getVariantValue(CryptoNote::ISerializer& serializer, uint8_t tag, boost::variant<CryptoNote::BaseInputDetails,
                                                                                      CryptoNote::KeyInputDetails,
                                                                                      CryptoNote::MultisignatureInputDetails> in) {
  switch (static_cast<SerializationTag>(tag)) {
  case SerializationTag::Base: {
    CryptoNote::BaseInputDetails v;
    serializer(v, "data");
    in = v;
    break;
  }
  case SerializationTag::Key: {
    CryptoNote::KeyInputDetails v;
    serializer(v, "data");
    in = v;
    break;
  }
  case SerializationTag::Multisignature: {
    CryptoNote::MultisignatureInputDetails v;
    serializer(v, "data");
    in = v;
    break;
  }
  default:
    throw std::runtime_error("Unknown variant tag");
  }
}

template <typename T>
bool serializePod(T& v, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializer.binary(&v, sizeof(v), name);
}

} //namespace

//namespace CryptoNote {

void serialize(transactionOutputDetails2& output, ISerializer& serializer) {
  serializer(output.output, "output");
  serializer(output.globalIndex, "globalIndex");
}

void serialize(TransactionOutputReferenceDetails& outputReference, ISerializer& serializer) {
  serializePod(outputReference.transactionHash, "transactionHash", serializer);
  serializer(outputReference.number, "number");
}

void serialize(BaseInputDetails& inputBase, ISerializer& serializer) {
  serializer(inputBase.input, "input");
  serializer(inputBase.amount, "amount");
}

void serialize(KeyInputDetails& inputToKey, ISerializer& serializer) {
  serializer(inputToKey.input, "input");
  serializer(inputToKey.mixin, "mixin");
  serializer(inputToKey.outputs, "outputs");
}

void serialize(MultisignatureInputDetails& inputMultisig, ISerializer& serializer) {
  serializer(inputMultisig.input, "input");
  serializer(inputMultisig.output, "output");
}

void serialize(transactionInputDetails2& input, ISerializer& serializer) {
  if (serializer.type() == ISerializer::OUTPUT) {
    BinaryVariantTagGetter tagGetter;
    uint8_t tag = boost::apply_visitor(tagGetter, input);
    serializer.binary(&tag, sizeof(tag), "type");

    VariantSerializer visitor(serializer, "data");
    boost::apply_visitor(visitor, input);
  } else {
    uint8_t tag;
    serializer.binary(&tag, sizeof(tag), "type");

    getVariantValue(serializer, tag, input);
  }
}

void serialize(TransactionExtraDetails& extra, ISerializer& serializer) {
  serializePod(extra.publicKey, "publicKey", serializer);
  serializer(extra.nonce, "nonce");
  serializeAsBinary(extra.raw, "raw", serializer);
}

void serialize(TransactionExtraDetails2& extra, ISerializer& serializer) {
  serializePod(extra.publicKey, "publicKey", serializer);
  serializer(extra.nonce, "nonce");
  serializeAsBinary(extra.raw, "raw", serializer);
}

void serialize(TransactionDetails& transaction, ISerializer& serializer) {
  serializePod(transaction.hash, "hash", serializer);
  serializer(transaction.size, "size");
  serializer(transaction.fee, "fee");
  serializer(transaction.totalInputsAmount, "totalInputsAmount");
  serializer(transaction.totalOutputsAmount, "totalOutputsAmount");
  serializer(transaction.mixin, "mixin");
  serializer(transaction.unlockTime, "unlockTime");
  serializer(transaction.timestamp, "timestamp");
  serializePod(transaction.paymentId, "paymentId", serializer);
  serializer(transaction.inBlockchain, "inBlockchain");
  serializePod(transaction.blockHash, "blockHash", serializer);
  serializer(transaction.blockHeight, "blockIndex");
  serializer(transaction.extra, "extra");
  serializer(transaction.inputs, "inputs");
  serializer(transaction.outputs, "outputs");

  //serializer(transaction.signatures, "signatures");
  if (serializer.type() == ISerializer::OUTPUT) {
    std::vector<std::pair<size_t, Crypto::Signature>> signaturesForSerialization;
    signaturesForSerialization.reserve(transaction.signatures.size());
    size_t ctr = 0;
    for (const auto& signaturesV : transaction.signatures) {
      for (auto signature : signaturesV) {
        signaturesForSerialization.emplace_back(ctr, std::move(signature));
      }
      ++ctr;
    }
    size_t size = transaction.signatures.size();
    serializer(size, "signaturesSize");
    serializer(signaturesForSerialization, "signatures");
  } else {
    size_t size = 0;
    serializer(size, "signaturesSize");
    transaction.signatures.resize(size);

    std::vector<std::pair<size_t, Crypto::Signature>> signaturesForSerialization;
    serializer(signaturesForSerialization, "signatures");

    for (const auto& signatureWithIndex : signaturesForSerialization) {
      transaction.signatures[signatureWithIndex.first].push_back(signatureWithIndex.second);
    }
  }
}

void serialize(BlockDetails& block, ISerializer& serializer) {
  serializer(block.majorVersion, "majorVersion");
  serializer(block.minorVersion, "minorVersion");
  serializer(block.timestamp, "timestamp");
  serializePod(block.prevBlockHash, "prevBlockHash", serializer);
  serializer(block.nonce, "nonce");
  serializer(block.height, "index");
  serializePod(block.hash, "hash", serializer);
  serializer(block.difficulty, "difficulty");
  serializer(block.reward, "reward");
  serializer(block.baseReward, "baseReward");
  serializer(block.blockSize, "blockSize");
  serializer(block.transactionsCumulativeSize, "transactionsCumulativeSize");
  serializer(block.alreadyGeneratedCoins, "alreadyGeneratedCoins");
  serializer(block.alreadyGeneratedTransactions, "alreadyGeneratedTransactions");
  serializer(block.sizeMedian, "sizeMedian");
  serializer(block.penalty, "penalty");
  serializer(block.totalFeeAmount, "totalFeeAmount");
  serializer(block.transactions, "transactions");
}

} //namespace CryptoNote
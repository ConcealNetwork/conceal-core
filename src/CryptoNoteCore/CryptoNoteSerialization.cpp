// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "CryptoNoteSerialization.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>

#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>

#include "Serialization/ISerializer.h"
#include "Serialization/SerializationOverloads.h"
#include "Serialization/BinaryInputStreamSerializer.h"
#include "Serialization/BinaryOutputStreamSerializer.h"

#include "Common/StringOutputStream.h"
#include "crypto/crypto.h"

#include "Account.h"
#include "CryptoNoteConfig.h"
#include "CryptoNoteFormatUtils.h"
#include "CryptoNoteTools.h"
#include "TransactionExtra.h"

using namespace common;

namespace {

using namespace cn;
using namespace common;

class serialization_error : public std::runtime_error
{
public:
  using runtime_error::runtime_error;
};

size_t getSignaturesCount(const TransactionInput& input) {
  struct txin_signature_size_visitor : public boost::static_visitor < size_t > {
    size_t operator()(const BaseInput &) const { return 0; }
    size_t operator()(const KeyInput &txin) const { return txin.outputIndexes.size(); }
    size_t operator()(const MultisignatureInput& txin) const { return txin.signatureCount; }
  };

  return boost::apply_visitor(txin_signature_size_visitor(), input);
}

struct BinaryVariantTagGetter : boost::static_visitor<uint8_t>
{
  uint8_t operator()(const cn::BaseInput &) const { return 0xff; }
  uint8_t operator()(const cn::KeyInput &) const { return 0x2; }
  uint8_t operator()(const cn::MultisignatureInput &) const { return 0x3; }
  uint8_t operator()(const cn::KeyOutput &) const { return 0x2; }
  uint8_t operator()(const cn::MultisignatureOutput &) const { return 0x3; }
  uint8_t operator()(const cn::Transaction &) const { return 0xcc; }
  uint8_t operator()(const cn::Block &) const { return 0xbb; }
};

struct VariantSerializer : boost::static_visitor<> {
  VariantSerializer(cn::ISerializer& serializer, const std::string& name) : s(serializer), name(name) {}

  template <typename T>
  void operator() (T& param) { s(param, name); }

  cn::ISerializer& s;
  std::string name;
};

void getVariantValue(cn::ISerializer& serializer, uint8_t tag, cn::TransactionInput& in) {
  switch(tag) {
  case 0xff: {
    cn::BaseInput v;
    serializer(v, "value");
    in = v;
    break;
  }
  case 0x2: {
    cn::KeyInput v;
    serializer(v, "value");
    in = v;
    break;
  }
  case 0x3: {
    cn::MultisignatureInput v;
    serializer(v, "value");
    in = v;
    break;
  }
  default:
    throw serialization_error("Unknown variant tag");
  }
}

void getVariantValue(cn::ISerializer& serializer, uint8_t tag, cn::TransactionOutputTarget& out) {
  switch(tag) {
  case 0x2: {
    cn::KeyOutput v;
    serializer(v, "data");
    out = v;
    break;
  }
  case 0x3: {
    cn::MultisignatureOutput v;
    serializer(v, "data");
    out = v;
    break;
  }
  default:
    throw serialization_error("Unknown variant tag");
  }
}

template <typename T>
bool serializePod(T& v, common::StringView name, cn::ISerializer& serializer) {
  return serializer.binary(&v, sizeof(v), name);
}

bool serializeVarintVector(std::vector<uint32_t>& vector, cn::ISerializer& serializer, common::StringView name) {
  size_t size = vector.size();
  
  if (!serializer.beginArray(size, name)) {
    vector.clear();
    return false;
  }

  vector.resize(size);

  for (size_t i = 0; i < size; ++i) {
    serializer(vector[i], "");
  }

  serializer.endArray();
  return true;
}

}

namespace crypto {

bool serialize(PublicKey& pubKey, common::StringView name, cn::ISerializer& serializer) {
  return serializePod(pubKey, name, serializer);
}

bool serialize(SecretKey& secKey, common::StringView name, cn::ISerializer& serializer) {
  return serializePod(secKey, name, serializer);
}

bool serialize(Hash& h, common::StringView name, cn::ISerializer& serializer) {
  return serializePod(h, name, serializer);
}

bool serialize(KeyImage& keyImage, common::StringView name, cn::ISerializer& serializer) {
  return serializePod(keyImage, name, serializer);
}

bool serialize(chacha8_iv &chacha8, common::StringView name, cn::ISerializer &serializer)
{
  return serializePod(chacha8, name, serializer);
}

bool serialize(Signature& sig, common::StringView name, cn::ISerializer& serializer) {
  return serializePod(sig, name, serializer);
}

bool serialize(EllipticCurveScalar& ecScalar, common::StringView name, cn::ISerializer& serializer) {
  return serializePod(ecScalar, name, serializer);
}

bool serialize(EllipticCurvePoint& ecPoint, common::StringView name, cn::ISerializer& serializer) {
  return serializePod(ecPoint, name, serializer);
}

}

namespace cn {

void serialize(TransactionPrefix& txP, ISerializer& serializer) {
  serializer(txP.version, "version");

  if (TRANSACTION_VERSION_2 < txP.version) {
    throw serialization_error("Wrong transaction version");
  }

  serializer(txP.unlockTime, "unlock_time");
  serializer(txP.inputs, "vin");
  serializer(txP.outputs, "vout");
  serializeAsBinary(txP.extra, "extra", serializer);
}

void serialize(Transaction& tx, ISerializer& serializer) {
  serialize(static_cast<TransactionPrefix&>(tx), serializer);

  size_t sigSize = tx.inputs.size();
  
  if (serializer.type() == ISerializer::INPUT) {
    tx.signatures.resize(sigSize);
  }

  bool signaturesNotExpected = tx.signatures.empty();
  if (!signaturesNotExpected && tx.inputs.size() != tx.signatures.size()) {
    throw serialization_error("Unexpected signature size caused a serialization problem");
  }

  for (size_t i = 0; i < tx.inputs.size(); ++i) {
    size_t signatureSize = getSignaturesCount(tx.inputs[i]);
    if (signaturesNotExpected) {
      if (signatureSize == 0) {
        continue;
      } else {
        throw serialization_error("Unexpected signatures caused a serialization problem");
      }
    }

    if (serializer.type() == ISerializer::OUTPUT) {
      if (signatureSize != tx.signatures[i].size()) {
        throw serialization_error("Unexpected signature size caused a serialization problem");
      }

      for (crypto::Signature& sig : tx.signatures[i]) {
        serializePod(sig, "", serializer);
      }

    } else {
      std::vector<crypto::Signature> signatures(signatureSize);
      for (crypto::Signature& sig : signatures) {
        serializePod(sig, "", serializer);
      }

      tx.signatures[i] = std::move(signatures);
    }
  }
}

void serialize(TransactionInput& in, ISerializer& serializer) {
  if (serializer.type() == ISerializer::OUTPUT) {
    BinaryVariantTagGetter tagGetter;
    uint8_t tag = boost::apply_visitor(tagGetter, in);
    serializer.binary(&tag, sizeof(tag), "type");

    VariantSerializer visitor(serializer, "value");
    boost::apply_visitor(visitor, in);
  } else {
    uint8_t tag;
    serializer.binary(&tag, sizeof(tag), "type");

    getVariantValue(serializer, tag, in);
  }
}

void serialize(BaseInput& gen, ISerializer& serializer) {
  serializer(gen.blockIndex, "height");
}

void serialize(KeyInput& key, ISerializer& serializer) {
  serializer(key.amount, "amount");
  serializeVarintVector(key.outputIndexes, serializer, "key_offsets");
  serializer(key.keyImage, "k_image");
}

void serialize(MultisignatureInput& multisignature, ISerializer& serializer) {
  serializer(multisignature.amount, "amount");
  serializer(multisignature.signatureCount, "signatures");
  serializer(multisignature.outputIndex, "outputIndex");
  serializer(multisignature.term, "term");
}


void serialize(TransactionInputs & inputs, ISerializer & serializer) {
  serializer(inputs, "vin");
}


void serialize(TransactionOutput& output, ISerializer& serializer) {
  serializer(output.amount, "amount");
  serializer(output.target, "target");
}

void serialize(TransactionOutputTarget& output, ISerializer& serializer) {
  if (serializer.type() == ISerializer::OUTPUT) {
    BinaryVariantTagGetter tagGetter;
    uint8_t tag = boost::apply_visitor(tagGetter, output);
    serializer.binary(&tag, sizeof(tag), "type");

    VariantSerializer visitor(serializer, "data");
    boost::apply_visitor(visitor, output);
  } else {
    uint8_t tag;
    serializer.binary(&tag, sizeof(tag), "type");

    getVariantValue(serializer, tag, output);
  }
}

void serialize(KeyOutput& key, ISerializer& serializer) {
  serializer(key.key, "key");
}

void serialize(MultisignatureOutput& multisignature, ISerializer& serializer) {
  serializer(multisignature.keys, "keys");
  serializer(multisignature.requiredSignatureCount, "required_signatures");
  serializer(multisignature.term, "term");
}

void serializeBlockHeader(BlockHeader& header, ISerializer& serializer) {
  serializer(header.majorVersion, "major_version");
  if (header.majorVersion > BLOCK_MAJOR_VERSION_8) {
    throw serialization_error("Wrong major version");
  }

  serializer(header.minorVersion, "minor_version");
  serializer(header.timestamp, "timestamp");
  serializer(header.previousBlockHash, "prev_id");
  serializer.binary(&header.nonce, sizeof(header.nonce), "nonce");
}

void serialize(BlockHeader& header, ISerializer& serializer) {
  serializeBlockHeader(header, serializer);
}

void serialize(Block& block, ISerializer& serializer) {
  serializeBlockHeader(block, serializer);

  serializer(block.baseTransaction, "miner_tx");
  serializer(block.transactionHashes, "tx_hashes");
}

void serialize(AccountPublicAddress& address, ISerializer& serializer) {
  serializer(address.spendPublicKey, "m_spend_public_key");
  serializer(address.viewPublicKey, "m_view_public_key");
}

void serialize(AccountKeys& keys, ISerializer& s) {
  s(keys.address, "m_account_address");
  s(keys.spendSecretKey, "m_spend_secret_key");
  s(keys.viewSecretKey, "m_view_secret_key");
}

void doSerialize(TransactionExtraMergeMiningTag& tag, ISerializer& serializer) {
  serializer(tag.depth, "depth");
  serializer(tag.merkleRoot, "merkle_root");
}

void serialize(TransactionExtraMergeMiningTag& tag, ISerializer& serializer) {
  if (serializer.type() == ISerializer::OUTPUT) {
    std::string field;
    StringOutputStream os(field);
    BinaryOutputStreamSerializer output(os);
    doSerialize(tag, output);
    serializer(field, "");
  } else {
    std::string field;
    serializer(field, "");
    MemoryInputStream stream(field.data(), field.size());
    BinaryInputStreamSerializer input(stream);
    doSerialize(tag, input);
  }
}

void serialize(KeyPair& keyPair, ISerializer& serializer) {
  serializer(keyPair.secretKey, "secret_key");
  serializer(keyPair.publicKey, "public_key");
}


} //namespace cn

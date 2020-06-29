// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <array>
#include <string>
#include <vector>

#include "CryptoTypes.h"

#include "CryptoNote.h"
#include "BlockchainExplorerData.h"

#include <boost/variant.hpp>

namespace CryptoNote {

enum class TransactionRemoveReason : uint8_t 
{ 
  INCLUDED_IN_BLOCK = 0, 
  TIMEOUT = 1
};

struct TransactionOutputToKeyDetails {
  Crypto::PublicKey txOutKey;
};

struct TransactionOutputMultisignatureDetails {
  std::vector<Crypto::PublicKey> keys;
  uint32_t requiredSignatures;
};

struct TransactionOutputDetails {
  uint64_t amount;
  uint32_t globalIndex;

  boost::variant<
    TransactionOutputToKeyDetails,
    TransactionOutputMultisignatureDetails> output;
};

struct TransactionOutputReferenceDetails {
  Crypto::Hash transactionHash;
  size_t number;
};

struct TransactionInputGenerateDetails {
  uint32_t height;
};

struct TransactionInputToKeyDetails {
  std::vector<uint32_t> outputIndexes;
  Crypto::KeyImage keyImage;
  uint64_t mixin;
  TransactionOutputReferenceDetails output;
};

struct TransactionInputMultisignatureDetails {
  uint32_t signatures;
  TransactionOutputReferenceDetails output;
};

struct TransactionInputDetails {
  uint64_t amount;

  boost::variant<
    TransactionInputGenerateDetails,
    TransactionInputToKeyDetails,
    TransactionInputMultisignatureDetails> input;
};

struct TransactionExtraDetails {
  std::vector<size_t> padding;
  std::vector<Crypto::PublicKey> publicKey; 
  std::vector<std::string> nonce;
  std::vector<uint8_t> raw;
};

struct transactionOutputDetails2 {
	TransactionOutput output;
	uint64_t globalIndex;
};

struct BaseInputDetails {
	BaseInput input;
	uint64_t amount;
};

struct KeyInputDetails {
	KeyInput input;
	uint64_t mixin;
	std::vector<TransactionOutputReferenceDetails> outputs;
};

struct MultisignatureInputDetails {
	MultisignatureInput input;
	TransactionOutputReferenceDetails output;
};

typedef boost::variant<BaseInputDetails, KeyInputDetails, MultisignatureInputDetails> transactionInputDetails2;

struct TransactionExtraDetails2 {
	std::vector<size_t> padding;
	Crypto::PublicKey publicKey;
	BinaryArray nonce;
	BinaryArray raw;
};

struct TransactionDetails {
  Crypto::Hash hash;
  uint64_t size = 0;
  uint64_t fee = 0;
  uint64_t totalInputsAmount = 0;
  uint64_t totalOutputsAmount = 0;
  uint64_t mixin = 0;
  uint64_t unlockTime = 0;
  uint64_t timestamp = 0;
  Crypto::Hash paymentId;
  bool hasPaymentId = false;
  bool inBlockchain = false;
  Crypto::Hash blockHash;
  uint32_t blockHeight = 0;
  TransactionExtraDetails2 extra;
  std::vector<std::vector<Crypto::Signature>> signatures;
  std::vector<transactionInputDetails2> inputs;
  std::vector<transactionOutputDetails2> outputs;
};

struct BlockDetails {
  uint8_t majorVersion = 0;
  uint8_t minorVersion = 0;
  uint64_t timestamp = 0;
  Crypto::Hash prevBlockHash;
  uint32_t nonce = 0;
  bool isOrphaned = false;
  uint32_t height = 0;
  Crypto::Hash hash;
  uint64_t difficulty = 0;
  uint64_t reward = 0;
  uint64_t baseReward = 0;
  uint64_t blockSize = 0;
  uint64_t transactionsCumulativeSize = 0;
  uint64_t alreadyGeneratedCoins = 0;
  uint64_t alreadyGeneratedTransactions = 0;
  uint64_t sizeMedian = 0;
  double penalty = 0.0;
  uint64_t totalFeeAmount = 0;
  std::vector<TransactionDetails> transactions;
};

}

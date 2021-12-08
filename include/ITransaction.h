// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2021 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "CryptoNote.h"

namespace cn {

namespace TransactionTypes {
  
  enum class InputType : uint8_t { Invalid, Key, Multisignature, Generating };
  enum class OutputType : uint8_t { Invalid, Key, Multisignature };

  struct GlobalOutput {
    crypto::PublicKey targetKey;
    uint32_t outputIndex;
  };

  typedef std::vector<GlobalOutput> GlobalOutputsContainer;

  struct OutputKeyInfo {
    crypto::PublicKey transactionPublicKey;
    size_t transactionIndex;
    size_t outputInTransaction;
  };

  struct InputKeyInfo {
    uint64_t amount;
    GlobalOutputsContainer outputs;
    OutputKeyInfo realOutput;
  };
}

//
// ITransactionReader
// 
class ITransactionReader {
public:
  virtual ~ITransactionReader() { }

  virtual crypto::Hash getTransactionHash() const = 0;
  virtual crypto::Hash getTransactionPrefixHash() const = 0;
  virtual crypto::Hash getTransactionInputsHash() const = 0;
  virtual crypto::PublicKey getTransactionPublicKey() const = 0;
  virtual bool getTransactionSecretKey(crypto::SecretKey& key) const = 0;
  virtual uint64_t getUnlockTime() const = 0;

  // extra
  virtual bool getPaymentId(crypto::Hash& paymentId) const = 0;
  virtual bool getExtraNonce(BinaryArray& nonce) const = 0;
  virtual BinaryArray getExtra() const = 0;

  // inputs
  virtual size_t getInputCount() const = 0;
  virtual uint64_t getInputTotalAmount() const = 0;
  virtual TransactionTypes::InputType getInputType(size_t index) const = 0;
  virtual void getInput(size_t index, KeyInput& input) const = 0;
  virtual void getInput(size_t index, MultisignatureInput& input) const = 0;
  virtual std::vector<TransactionInput> getInputs() const = 0;
  // outputs
  virtual size_t getOutputCount() const = 0;
  virtual uint64_t getOutputTotalAmount() const = 0;
  virtual TransactionTypes::OutputType getOutputType(size_t index) const = 0;
  virtual void getOutput(size_t index, KeyOutput& output, uint64_t& amount) const = 0;
  virtual void getOutput(size_t index, MultisignatureOutput& output, uint64_t& amount) const = 0;

  // signatures
  virtual size_t getRequiredSignaturesCount(size_t inputIndex) const = 0;
  virtual bool findOutputsToAccount(const AccountPublicAddress& addr, const crypto::SecretKey& viewSecretKey, std::vector<uint32_t>& outs, uint64_t& outputAmount) const = 0;

  // various checks
  virtual bool validateInputs() const = 0;
  virtual bool validateOutputs() const = 0;
  virtual bool validateSignatures() const = 0;

  // serialized transaction
  virtual BinaryArray getTransactionData() const = 0;
  virtual TransactionPrefix getTransactionPrefix() const = 0;
};

//
// ITransactionWriter
// 
class ITransactionWriter {
public: 

  virtual ~ITransactionWriter() { }

  // transaction parameters
  virtual void setUnlockTime(uint64_t unlockTime) = 0;

  // extra
  virtual void setPaymentId(const crypto::Hash& paymentId) = 0;
  virtual void setExtraNonce(const BinaryArray& nonce) = 0;
  virtual void appendExtra(const BinaryArray& extraData) = 0;

  // Inputs/Outputs 
  virtual size_t addInput(const KeyInput& input) = 0;
  virtual size_t addInput(const MultisignatureInput& input) = 0;
  virtual size_t addInput(const AccountKeys& senderKeys, const TransactionTypes::InputKeyInfo& info, KeyPair& ephKeys) = 0;

  virtual size_t addOutput(uint64_t amount, const AccountPublicAddress& to) = 0;
  virtual size_t addOutput(uint64_t amount, const std::vector<AccountPublicAddress>& to, uint32_t requiredSignatures, uint32_t term = 0) = 0;
  virtual size_t addOutput(uint64_t amount, const KeyOutput& out) = 0;
  virtual size_t addOutput(uint64_t amount, const MultisignatureOutput& out) = 0;

  // transaction info
  virtual void setTransactionSecretKey(const crypto::SecretKey& key) = 0;

  // signing
  virtual void signInputKey(size_t input, const TransactionTypes::InputKeyInfo& info, const KeyPair& ephKeys) = 0;
  virtual void signInputMultisignature(size_t input, const crypto::PublicKey& sourceTransactionKey, size_t outputIndex, const AccountKeys& accountKeys) = 0;
  virtual void signInputMultisignature(size_t input, const KeyPair& ephemeralKeys) = 0;
};

class ITransaction : 
  public ITransactionReader, 
  public ITransactionWriter {
public:
  virtual ~ITransaction() { }

};

}

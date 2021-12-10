// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/Currency.h"

class TransactionBuilder {
public:

  typedef std::vector<cn::AccountKeys> KeysVector;
  typedef std::vector<crypto::Signature> SignatureVector;
  typedef std::vector<SignatureVector> SignatureMultivector;

  struct MultisignatureSource {
    cn::MultisignatureInput input;
    KeysVector keys;
    crypto::PublicKey srcTxPubKey;
    size_t srcOutputIndex;
  };

  TransactionBuilder(const cn::Currency& currency, uint64_t unlockTime = 0);

  // regenerate transaction keys
  cn::KeyPair getTxKeys() const;
  TransactionBuilder& newTxKeys();
  TransactionBuilder& setTxKeys(const cn::KeyPair& txKeys);

  void setVersion(std::size_t version);

  // inputs
  TransactionBuilder& setInput(const std::vector<cn::TransactionSourceEntry>& sources, const cn::AccountKeys& senderKeys);
  TransactionBuilder& addMultisignatureInput(const MultisignatureSource& source);

  // outputs
  TransactionBuilder& setOutput(const std::vector<cn::TransactionDestinationEntry>& destinations);
  TransactionBuilder& addOutput(const cn::TransactionDestinationEntry& dest);
  TransactionBuilder& addMultisignatureOut(uint64_t amount, const KeysVector& keys, uint32_t required, uint32_t term = 0);

  cn::Transaction build() const;

  std::vector<cn::TransactionSourceEntry> m_sources;
  std::vector<cn::TransactionDestinationEntry> m_destinations;

private:

  void fillInputs(cn::Transaction& tx, std::vector<cn::KeyPair>& contexts) const;
  void fillOutputs(cn::Transaction& tx) const;
  void signSources(const crypto::Hash& prefixHash, const std::vector<cn::KeyPair>& contexts, cn::Transaction& tx) const;

  struct MultisignatureDestination {
    uint64_t amount;
    uint32_t requiredSignatures;
    KeysVector keys;
    uint32_t term;
  };

  cn::AccountKeys m_senderKeys;

  std::vector<MultisignatureSource> m_msigSources;
  std::vector<MultisignatureDestination> m_msigDestinations;

  size_t m_version;
  uint64_t m_unlockTime;
  cn::KeyPair m_txKey;
  const cn::Currency& m_currency;
};

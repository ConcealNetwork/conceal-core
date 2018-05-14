// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2015-2016 The Bytecoin developers
// Copyright (c) 2016-2017 The TurtleCoin developers
// Copyright (c) 2017-2018 krypt0x aka krypt0chaos
// Copyright (c) 2018 The Circle Foundation
//
// This file is part of Conceal Sense Crypto Engine.
//
// Conceal is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Conceal is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Conceal.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/Currency.h"

class TransactionBuilder {
public:

  typedef std::vector<CryptoNote::AccountKeys> KeysVector;
  typedef std::vector<Crypto::Signature> SignatureVector;
  typedef std::vector<SignatureVector> SignatureMultivector;

  TransactionBuilder(const CryptoNote::Currency& currency, uint64_t unlockTime = 0);

  // regenerate transaction keys
  TransactionBuilder& newTxKeys();
  TransactionBuilder& setTxKeys(const CryptoNote::KeyPair& txKeys);

  // inputs
  TransactionBuilder& setInput(const std::vector<CryptoNote::TransactionSourceEntry>& sources, const CryptoNote::AccountKeys& senderKeys);

  // outputs
  TransactionBuilder& setOutput(const std::vector<CryptoNote::TransactionDestinationEntry>& destinations);
  TransactionBuilder& addOutput(const CryptoNote::TransactionDestinationEntry& dest);

  CryptoNote::Transaction build() const;

  std::vector<CryptoNote::TransactionSourceEntry> m_sources;
  std::vector<CryptoNote::TransactionDestinationEntry> m_destinations;

private:

  void fillInputs(CryptoNote::Transaction& tx, std::vector<CryptoNote::KeyPair>& contexts) const;
  void fillOutputs(CryptoNote::Transaction& tx) const;
  void signSources(const Crypto::Hash& prefixHash, const std::vector<CryptoNote::KeyPair>& contexts, CryptoNote::Transaction& tx) const;

  CryptoNote::AccountKeys m_senderKeys;

  size_t m_version;
  uint64_t m_unlockTime;
  CryptoNote::KeyPair m_txKey;
  const CryptoNote::Currency& m_currency;
};

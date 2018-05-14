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

#include <cstdint>

#include "CryptoNoteProtocol/ICryptoNoteProtocolObserver.h"
#include "CryptoNoteProtocol/ICryptoNoteProtocolQuery.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandlerCommon.h"

class ICryptoNoteProtocolQueryStub : public CryptoNote::ICryptoNoteProtocolHandler {
public:
  ICryptoNoteProtocolQueryStub() : peers(0), observedHeight(0), synchronized(false) {
  }

  virtual bool addObserver(CryptoNote::ICryptoNoteProtocolObserver* observer) override;
  virtual bool removeObserver(CryptoNote::ICryptoNoteProtocolObserver* observer) override;
  virtual uint32_t getObservedHeight() const override;
  virtual uint32_t getBlockchainHeight() const override;
  virtual size_t getPeerCount() const override;
  virtual bool isSynchronized() const override;

  void setPeerCount(uint32_t count);
  void setObservedHeight(uint32_t height);

  void setSynchronizedStatus(bool status);
  virtual void relayBlock(CryptoNote::NOTIFY_NEW_BLOCK_request& arg) override{};
  virtual void relayTransactions(const std::vector<CryptoNote::BinaryArray>& transactions) override{};


private:
  size_t peers;
  uint32_t observedHeight;
  uint32_t blockchainHeight;

  bool synchronized;
};

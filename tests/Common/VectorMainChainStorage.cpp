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

#include "VectorMainChainStorage.h"

#include <CryptoNoteCore/CryptoNoteTools.h>

namespace CryptoNote {

void VectorMainChainStorage::pushBlock(const RawBlock& rawBlock) {
  storage.push_back(rawBlock);
}

void VectorMainChainStorage::popBlock() {
  storage.pop_back();
}

RawBlock VectorMainChainStorage::getBlockByIndex(uint32_t index) const {
  return storage.at(index);
}

uint32_t VectorMainChainStorage::getBlockCount() const {
  return static_cast<uint32_t>(storage.size());
}

void VectorMainChainStorage::clear() {
  storage.clear();
}

std::unique_ptr<IMainChainStorage> createVectorMainChainStorage(const Currency& currency) {
  std::unique_ptr<IMainChainStorage> storage(new VectorMainChainStorage());

  RawBlock genesis;
  genesis.block = toBinaryArray(currency.genesisBlock());
  storage->pushBlock(genesis);

  return storage;
}

}

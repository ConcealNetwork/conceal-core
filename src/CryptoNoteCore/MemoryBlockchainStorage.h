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

#include <CryptoNoteCore/BlockchainStorage.h>

namespace CryptoNote {

class MemoryBlockchainStorage : public BlockchainStorage::IBlockchainStorageInternal {
public:
  explicit MemoryBlockchainStorage(uint32_t reserveSize);
  virtual ~MemoryBlockchainStorage() override;

  virtual void pushBlock(RawBlock&& rawBlock) override;

  //Returns MemoryBlockchainStorage with elements from [splitIndex, blocks.size() - 1].
  //Original MemoryBlockchainStorage will contain elements from [0, splitIndex - 1].
  virtual std::unique_ptr<BlockchainStorage::IBlockchainStorageInternal> splitStorage(uint32_t splitIndex) override;

  virtual RawBlock getBlockByIndex(uint32_t index) const override;
  virtual uint32_t getBlockCount() const override;

private:
  std::vector<RawBlock> blocks;
};

}

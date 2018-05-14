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

#include "MemoryBlockchainCacheFactory.h"

namespace CryptoNote {

MemoryBlockchainCacheFactory::MemoryBlockchainCacheFactory(const std::string& filename, Logging::ILogger& logger):
  filename(filename), logger(logger) {
}

MemoryBlockchainCacheFactory::~MemoryBlockchainCacheFactory() {
}

std::unique_ptr<IBlockchainCache> MemoryBlockchainCacheFactory::createRootBlockchainCache(const Currency& currency) {
  return createBlockchainCache(currency, nullptr, 0);
}

std::unique_ptr<IBlockchainCache> MemoryBlockchainCacheFactory::createBlockchainCache(
    const Currency& currency,
    IBlockchainCache* parent,
    uint32_t startIndex) {

  return std::unique_ptr<IBlockchainCache>(new BlockchainCache(filename, currency, logger, parent, startIndex));
}

} //namespace CryptoNote

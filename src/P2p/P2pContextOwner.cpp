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

#include "P2pContextOwner.h"
#include <cassert>
#include "P2pContext.h"

namespace CryptoNote {

P2pContextOwner::P2pContextOwner(P2pContext* ctx, ContextList& contextList) : contextList(contextList) {
  contextIterator = contextList.insert(contextList.end(), ContextList::value_type(ctx));
}

P2pContextOwner::P2pContextOwner(P2pContextOwner&& other) : contextList(other.contextList), contextIterator(other.contextIterator) {
  other.contextIterator = contextList.end();
}

P2pContextOwner::~P2pContextOwner() {
  if (contextIterator != contextList.end()) {
    contextList.erase(contextIterator);
  }
}

P2pContext& P2pContextOwner::get() {
  assert(contextIterator != contextList.end());
  return *contextIterator->get();
}

P2pContext* P2pContextOwner::operator -> () {
  return &get();
}

}

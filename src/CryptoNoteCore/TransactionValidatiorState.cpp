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

#include "TransactionValidatiorState.h"

namespace CryptoNote {

void mergeStates(TransactionValidatorState& destionation, const TransactionValidatorState& source) {
  destionation.spentKeyImages.insert(source.spentKeyImages.begin(), source.spentKeyImages.end());
}

bool hasIntersections(const TransactionValidatorState& destination, const TransactionValidatorState& source) {
  return std::any_of(source.spentKeyImages.begin(), source.spentKeyImages.end(),
                     [&](const Crypto::KeyImage& ki) { return destination.spentKeyImages.count(ki) != 0; });
}

void excludeFromState(TransactionValidatorState& state, const CachedTransaction& cachedTransaction) {
  const auto& transaction = cachedTransaction.getTransaction();
  for (auto& input : transaction.inputs) {
    if (input.type() == typeid(KeyInput)) {
      const auto& in = boost::get<KeyInput>(input);
      assert(state.spentKeyImages.count(in.keyImage) > 0);
      state.spentKeyImages.erase(in.keyImage);
    } else {
      assert(false);
    }
  }
}

}

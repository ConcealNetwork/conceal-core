// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "ITransaction.h"
#include <functional>
#include <cstring>

namespace cn {

  inline bool operator==(const AccountPublicAddress &_v1, const AccountPublicAddress &_v2)
  {
    return _v1.spendPublicKey == _v2.spendPublicKey && _v1.viewPublicKey == _v2.viewPublicKey;
  }
}

namespace std {

template<>
struct hash < cn::AccountPublicAddress > {
  size_t operator()(const cn::AccountPublicAddress& val) const {
    size_t spend = *(reinterpret_cast<const size_t*>(&val.spendPublicKey));
    size_t view = *(reinterpret_cast<const size_t*>(&val.viewPublicKey));
    return spend ^ view;
  }
};

}

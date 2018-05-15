// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2016-2018 krypt0x aka krypt0chaos
// Copyright (c) 2018 The Circle Foundation
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <future>

namespace System {

namespace Detail {

template<class T> using Future = std::future<T>;

template<class T> Future<T> async(std::function<T()>&& operation) {
  return std::async(std::launch::async, std::move(operation));
}

}

}


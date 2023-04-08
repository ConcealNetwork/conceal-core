// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <condition_variable>
#include <mutex>
#include <stdint.h>

namespace cn {

class WalletAsyncContextCounter
{
public:
  WalletAsyncContextCounter() : m_asyncContexts(0) {}

  void addAsyncContext();
  void delAsyncContext();

  //returns true if contexts are finished before timeout
  void waitAsyncContextsFinish();

private:
  uint32_t m_asyncContexts;
  std::condition_variable m_cv;
  std::mutex m_mutex;
};

} //namespace cn

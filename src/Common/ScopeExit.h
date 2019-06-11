// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once 

#include <functional>

namespace Tools {

class ScopeExit {
public:
  ScopeExit(std::function<void()>&& handler);
  ~ScopeExit();

  ScopeExit(const ScopeExit&) = delete;
  ScopeExit(ScopeExit&&) = delete;
  ScopeExit& operator=(const ScopeExit&) = delete;
  ScopeExit& operator=(ScopeExit&&) = delete;

  void cancel();

private:
  std::function<void()> m_handler;
  bool m_cancelled;
};

}

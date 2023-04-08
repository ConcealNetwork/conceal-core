// Copyright (c) 2012-2017 The Cryptonote developers
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <vector>

namespace Tests {

  class Process {
  public:

    void startChild(const std::string& executablePath, const std::vector<std::string>& args = {});
    void wait();

  private:

    size_t m_pid = 0;

  };
}

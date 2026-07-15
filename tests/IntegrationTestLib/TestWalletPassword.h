// Copyright (c) 2018-2026 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license.

#pragma once

#include <cstdlib>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

namespace Tests {

  // Test wallet password shared by all wallets of a test run (CWE-798:
  // no hardcoded credential). Taken from CCX_TEST_WALLET_PASSWORD when set
  // (needed when a wallet file must be reopened by a later run, e.g.
  // blockchain fixtures), otherwise generated randomly once per process.
  inline const std::string& testWalletPassword()
  {
    static const std::string password = [] {
      const char* fromEnv = std::getenv("CCX_TEST_WALLET_PASSWORD");
      if (fromEnv != nullptr && *fromEnv != '\0')
      {
        return std::string(fromEnv);
      }

      std::random_device device;
      std::mt19937 gen(device());
      std::uniform_int_distribution<int> dist(0, 255);
      std::ostringstream oss;
      oss << std::hex << std::setfill('0');
      for (int i = 0; i < 16; ++i)
      {
        oss << std::setw(2) << dist(gen);
      }
      return oss.str();
    }();
    return password;
  }
}

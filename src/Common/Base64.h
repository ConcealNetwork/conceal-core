// Copyright (c) 2016-2019, The Karbo developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs

// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <string>

namespace Tools
{
  namespace Base64
  {
    std::string encode(const std::string& data);
    std::string decode(std::string const& encoded_string);
  }
}

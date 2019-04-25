// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>

namespace Common { namespace Console {

enum class Color : uint8_t {
  Default,

  Blue,
  Green,
  Red,
  Yellow,
  White,
  Cyan,
  Magenta,

  BrightBlue,
  BrightGreen,
  BrightRed,
  BrightYellow,
  BrightWhite,
  BrightCyan,
  BrightMagenta
};

void setTextColor(Color color);
bool isConsoleTty();

}}

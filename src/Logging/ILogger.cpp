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

#include "ILogger.h"

namespace Logging {

const std::string BLUE = "\x1F""BLUE\x1F";
const std::string GREEN = "\x1F""GREEN\x1F";
const std::string RED = "\x1F""RED\x1F";
const std::string YELLOW = "\x1F""YELLOW\x1F";
const std::string WHITE = "\x1F""WHITE\x1F";
const std::string CYAN = "\x1F""CYAN\x1F";
const std::string MAGENTA = "\x1F""MAGENTA\x1F";
const std::string BRIGHT_BLUE = "\x1F""BRIGHT_BLUE\x1F";
const std::string BRIGHT_GREEN = "\x1F""BRIGHT_GREEN\x1F";
const std::string BRIGHT_RED = "\x1F""BRIGHT_RED\x1F";
const std::string BRIGHT_YELLOW = "\x1F""BRIGHT_YELLOW\x1F";
const std::string BRIGHT_WHITE = "\x1F""BRIGHT_WHITE\x1F";
const std::string BRIGHT_CYAN = "\x1F""BRIGHT_CYAN\x1F";
const std::string BRIGHT_MAGENTA = "\x1F""BRIGHT_MAGENTA\x1F";
const std::string DEFAULT = "\x1F""DEFAULT\x1F";

const char ILogger::COLOR_DELIMETER = '\x1F';

const std::array<std::string, 6> ILogger::LEVEL_NAMES = {
  {"FATAL",
  "ERROR",
  "WARNING",
  "INFO",
  "DEBUG",
  "TRACE"}
};

}

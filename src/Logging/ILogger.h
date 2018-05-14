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

#pragma once

#include <string>
#include <array>
#include <boost/date_time/posix_time/posix_time.hpp>

#undef ERROR

namespace Logging {

enum Level {
  FATAL = 0,
  ERROR = 1,
  WARNING = 2,
  INFO = 3,
  DEBUGGING = 4,
  TRACE = 5
};

extern const std::string BLUE;
extern const std::string GREEN;
extern const std::string RED;
extern const std::string YELLOW;
extern const std::string WHITE;
extern const std::string CYAN;
extern const std::string MAGENTA;
extern const std::string BRIGHT_BLUE;
extern const std::string BRIGHT_GREEN;
extern const std::string BRIGHT_RED;
extern const std::string BRIGHT_YELLOW;
extern const std::string BRIGHT_WHITE;
extern const std::string BRIGHT_CYAN;
extern const std::string BRIGHT_MAGENTA;
extern const std::string DEFAULT;

class ILogger {
public:
  const static char COLOR_DELIMETER;

  const static std::array<std::string, 6> LEVEL_NAMES;

  virtual void operator()(const std::string& category, Level level, boost::posix_time::ptime time, const std::string& body) = 0;
};

#ifndef ENDL
#define ENDL std::endl
#endif

}

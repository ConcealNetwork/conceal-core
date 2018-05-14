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

#include <set>
#include "ILogger.h"

namespace Logging {

class CommonLogger : public ILogger {
public:

  virtual void operator()(const std::string& category, Level level, boost::posix_time::ptime time, const std::string& body) override;
  virtual void enableCategory(const std::string& category);
  virtual void disableCategory(const std::string& category);
  virtual void setMaxLevel(Level level);

  void setPattern(const std::string& pattern);

protected:
  std::set<std::string> disabledCategories;
  Level logLevel;
  std::string pattern;

  CommonLogger(Level level);
  virtual void doLogString(const std::string& message);
};

}

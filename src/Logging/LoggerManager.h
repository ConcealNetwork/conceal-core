// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <list>
#include <memory>
#include <mutex>
#include "../Common/JsonValue.h"
#include "LoggerGroup.h"

namespace logging {

class LoggerManager : public LoggerGroup {
public:
  LoggerManager();
  void configure(const common::JsonValue& val);
  virtual void operator()(const std::string& category, Level level, boost::posix_time::ptime time, const std::string& body) override;

private:
  std::vector<std::unique_ptr<CommonLogger>> loggers;
  std::mutex reconfigureLock;
};

}

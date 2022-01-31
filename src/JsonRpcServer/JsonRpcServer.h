// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2022 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <system_error>

#include <System/Dispatcher.h>
#include <System/Event.h>
#include "Logging/ILogger.h"
#include "Logging/LoggerRef.h"
#include "Rpc/HttpServer.h"


namespace cn {
class HttpResponse;
class HttpRequest;
}

namespace common {
class JsonValue;
}

namespace platform_system {
class TcpConnection;
}

namespace cn {

class JsonRpcServer : HttpServer {
public:
  JsonRpcServer(platform_system::Dispatcher& sys, platform_system::Event& stopEvent, logging::ILogger& loggerGroup);
  JsonRpcServer(const JsonRpcServer&) = delete;

  void start(const std::string& bindAddress, uint16_t bindPort, const std::string& user = "", const std::string& password = "");

protected:
  static void makeErrorResponse(const std::error_code& ec, common::JsonValue& resp);
  static void makeMethodNotFoundResponse(common::JsonValue& resp);
  static void makeGenericErrorReponse(common::JsonValue& resp, const char* what, int errorCode = -32001);
  static void fillJsonResponse(const common::JsonValue& v, common::JsonValue& resp);
  static void prepareJsonResponse(const common::JsonValue& req, common::JsonValue& resp);
  static void makeJsonParsingErrorResponse(common::JsonValue& resp);

  virtual void processJsonRpcRequest(const common::JsonValue& req, common::JsonValue& resp) = 0;

private:
  // HttpServer
  virtual void processRequest(const cn::HttpRequest& request, cn::HttpResponse& response) override;

  platform_system::Dispatcher& system;
  platform_system::Event& stopEvent;
  logging::LoggerRef logger;
};

} //namespace cn

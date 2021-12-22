// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Karbo.
//
// Karbo is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Karbo is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// Copyright (c) 2014-2016 XDN developers
//
// You should have received a copy of the GNU Lesser General Public License
// along with Karbo.  If not, see <http://www.gnu.org/licenses/>.

#pragma once 

#include <unordered_set>

#include <HTTP/HttpRequest.h>
#include <HTTP/HttpResponse.h>

#include <System/ContextGroup.h>
#include <System/Dispatcher.h>
#include <System/TcpListener.h>
#include <System/TcpConnection.h>
#include <System/Event.h>

#include <Logging/LoggerRef.h>

namespace cn {

class HttpServer {

public:

  HttpServer(platform_system::Dispatcher& dispatcher, logging::ILogger& log);

  void start(const std::string& address, uint16_t port, const std::string& user = "", const std::string& password = "");
  void stop();

  virtual void processRequest(const HttpRequest& request, HttpResponse& response) = 0;
  virtual size_t get_connections_count() const;

protected:

  platform_system::Dispatcher& m_dispatcher;

private:

  void acceptLoop();
  void connectionHandler(platform_system::TcpConnection&& conn);
  bool authenticate(const HttpRequest& request) const;

  platform_system::ContextGroup workingContextGroup;
  logging::LoggerRef logger;
  platform_system::TcpListener m_listener;
  std::unordered_set<platform_system::TcpConnection*> m_connections;
  std::string m_credentials;
};

}

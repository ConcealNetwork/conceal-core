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
//
// You should have received a copy of the GNU Lesser General Public License
// along with Karbo.  If not, see <http://www.gnu.org/licenses/>.

#include "HttpClient.h"

#include <HTTP/HttpParser.h>
#include <System/Ipv4Resolver.h>
#include <System/Ipv4Address.h>
#include <System/TcpConnector.h>

namespace cn {

HttpClient::HttpClient(platform_system::Dispatcher& dispatcher, const std::string& address, uint16_t port) :
  m_dispatcher(dispatcher), m_address(address), m_port(port) {
}

HttpClient::~HttpClient() {
  if (m_connected) {
    disconnect();
  }
}

void HttpClient::request(const HttpRequest &req, HttpResponse &res) {
  if (!m_connected) {
    connect();
  }

  try {
    std::iostream stream(m_streamBuf.get());
    HttpParser parser;
    stream << req;
    stream.flush();
    parser.receiveResponse(stream, res);
  } catch (const std::exception &) {
    disconnect();
    throw;
  }
}

void HttpClient::connect() {
  try {
    auto ipAddr = platform_system::Ipv4Resolver(m_dispatcher).resolve(m_address);
    m_connection = platform_system::TcpConnector(m_dispatcher).connect(ipAddr, m_port);
    m_streamBuf.reset(new platform_system::TcpStreambuf(m_connection));
    m_connected = true;
  } catch (const std::exception& e) {
    throw ConnectException(e.what());
  }
}

bool HttpClient::isConnected() const {
  return m_connected;
}

void HttpClient::disconnect() {
  m_streamBuf.reset();
  try {
    m_connection.write(nullptr, 0); //Socket shutdown.
  } catch (std::exception&) {
    //Ignoring possible exception.
  }

  try {
    m_connection = platform_system::TcpConnection();
  } catch (std::exception&) {
    //Ignoring possible exception.
  }

  m_connected = false;
}

ConnectException::ConnectException(const std::string& whatArg) : std::runtime_error(whatArg.c_str()) {
}

}

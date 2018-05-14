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

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "BlockingQueue.h"
#include "ConsoleTools.h"

#ifndef _WIN32
#include <sys/select.h>
#endif

namespace Common {

  class AsyncConsoleReader {

  public:

    AsyncConsoleReader();
    ~AsyncConsoleReader();

    void start();
    bool getline(std::string& line);
    void stop();
    bool stopped() const;
    void pause();
    void unpause();

  private:

    void consoleThread();
    bool waitInput();

    std::atomic<bool> m_stop;
    std::thread m_thread;
    BlockingQueue<std::string> m_queue;
  };


  class ConsoleHandler {
    public:

      ~ConsoleHandler();

      typedef std::function<bool(const std::vector<std::string> &)> ConsoleCommandHandler;

      std::string getUsage() const;
      void setHandler(const std::string& command, const ConsoleCommandHandler& handler, const std::string& usage = "");
      void requestStop();
      bool runCommand(const std::vector<std::string>& cmdAndArgs);

      void start(bool startThread = true, const std::string& prompt = "", Console::Color promptColor = Console::Color::Default);
      void stop();
      void wait(); 
      void pause();
      void unpause();

    private:

      typedef std::map<std::string, std::pair<ConsoleCommandHandler, std::string>> CommandHandlersMap;

      virtual void handleCommand(const std::string& cmd);

      void handlerThread();

      std::thread m_thread;
      std::string m_prompt;
      Console::Color m_promptColor = Console::Color::Default;
      CommandHandlersMap m_handlers;
      AsyncConsoleReader m_consoleReader;
  };
}

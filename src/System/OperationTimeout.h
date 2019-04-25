// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <System/ContextGroup.h>
#include <System/Dispatcher.h>
#include <System/Timer.h>

namespace System {

template<typename T> class OperationTimeout {
public:
  OperationTimeout(Dispatcher& dispatcher, T& object, std::chrono::nanoseconds timeout) :
    object(object), timerContext(dispatcher), timeoutTimer(dispatcher) {
    timerContext.spawn([this, timeout]() {
      try {
        timeoutTimer.sleep(timeout);
        timerContext.interrupt();
      } catch (std::exception&) {
      }
    });
  }

  ~OperationTimeout() {
    timerContext.interrupt();
    timerContext.wait();
  }

private:
  T& object;
  ContextGroup timerContext;
  Timer timeoutTimer;
};

}

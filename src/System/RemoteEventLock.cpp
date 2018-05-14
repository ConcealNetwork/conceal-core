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

#include "RemoteEventLock.h"
#include <cassert>
#include <mutex>
#include <condition_variable>
#include <System/Dispatcher.h>
#include <System/Event.h>

namespace System {

RemoteEventLock::RemoteEventLock(Dispatcher& dispatcher, Event& event) : dispatcher(dispatcher), event(event) {
  std::mutex mutex;
  std::condition_variable condition;
  bool locked = false;

  dispatcher.remoteSpawn([&]() {
    while (!event.get()) {
      event.wait();
    }

    event.clear();
    mutex.lock();
    locked = true;
    condition.notify_one();
    mutex.unlock();
  });

  std::unique_lock<std::mutex> lock(mutex);
  while (!locked) {
    condition.wait(lock);
  }
}

RemoteEventLock::~RemoteEventLock() {
  std::mutex mutex;
  std::condition_variable condition;
  bool locked = true;

  Event* eventPointer = &event;
  dispatcher.remoteSpawn([&]() {
    assert(!eventPointer->get());
    eventPointer->set();

    mutex.lock();
    locked = false;
    condition.notify_one();
    mutex.unlock();
  });

  std::unique_lock<std::mutex> lock(mutex);
  while (locked) {
    condition.wait(lock);
  }
}

}

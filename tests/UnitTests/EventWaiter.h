// Copyright (c) 2012-2017 The Cryptonote developers
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <mutex>
#include <condition_variable>

#pragma once

class EventWaiter {
private:
  std::mutex mtx;
  std::condition_variable cv;
  bool available;

public:
  EventWaiter() : available(false) {}

  void notify();
  void wait();
  bool wait_for(const std::chrono::milliseconds& rel_time);
};

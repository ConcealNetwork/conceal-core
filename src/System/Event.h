// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

namespace System {

class Dispatcher;

class Event {
public:
  Event();
  explicit Event(Dispatcher& dispatcher);
  Event(const Event&) = delete;
  Event(Event&& other);
  ~Event();
  Event& operator=(const Event&) = delete;
  Event& operator=(Event&& other);
  bool get() const;
  void clear();
  void set();
  void wait();

private:
  Dispatcher* dispatcher;
  bool state;
  void* first;
  void* last;
};

}

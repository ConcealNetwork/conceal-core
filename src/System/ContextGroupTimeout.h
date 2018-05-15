// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2016-2018 krypt0x aka krypt0chaos
// Copyright (c) 2018 The Circle Foundation
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once 
#include <chrono>
#include <System/ContextGroup.h>
#include <System/Timer.h>

namespace System {

class ContextGroupTimeout {
public: 
  ContextGroupTimeout(Dispatcher&, ContextGroup&, std::chrono::nanoseconds);

private: 
  Timer timeoutTimer;
  ContextGroup workingContextGroup;
};

}

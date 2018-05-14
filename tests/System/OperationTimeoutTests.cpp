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

#include <gtest/gtest.h>
#include <System/OperationTimeout.h>
#include <System/InterruptedException.h>

using namespace System;

class OperationTimeoutTest : public testing::Test {
public:
  OperationTimeoutTest() : contextGroup(dispatcher), timer(dispatcher) {
  }

  Dispatcher dispatcher;
  ContextGroup contextGroup;
  Timer timer;
};

TEST_F(OperationTimeoutTest, DISABLED_timeoutHappens) {
  OperationTimeout<Timer> op(dispatcher, timer, std::chrono::milliseconds(100));
  contextGroup.spawn([&] { 
    EXPECT_THROW(timer.sleep(std::chrono::milliseconds(200)), InterruptedException); 
  });
  contextGroup.wait();
}

TEST_F(OperationTimeoutTest, DISABLED_timeoutSkipped) {
  {
    OperationTimeout<Timer> op(dispatcher, timer, std::chrono::milliseconds(200));
    contextGroup.spawn([&] { 
      EXPECT_NO_THROW(timer.sleep(std::chrono::milliseconds(100)));
    });
    contextGroup.wait();
  }
}

TEST_F(OperationTimeoutTest, DISABLED_noOperation) {
  {
    OperationTimeout<Timer> op(dispatcher, timer, std::chrono::milliseconds(100));
  }
}

// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <System/ErrorMessage.h>
#include <gtest/gtest.h>

using namespace platform_system;

TEST(ErrorMessageTests, testErrorMessage) {
  auto msg = errorMessage(100);
  ASSERT_EQ(msg.substr(0, 12), "result=100, ");
}

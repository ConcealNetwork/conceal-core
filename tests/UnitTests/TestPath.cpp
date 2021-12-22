// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest/gtest.h"
#include "Common/PathTools.h"

TEST(PathTools, NativePathToGeneric) {

#ifdef _WIN32
  const std::string input = "C:\\Windows\\System\\etc\\file.exe";
  const std::string output = "C:/Windows/System/etc/file.exe";
#else
  const std::string input = "/var/tmp/file.tmp";
  const std::string output = input;

#endif

  auto path = common::NativePathToGeneric(input);
  ASSERT_EQ(output, path);
}

TEST(PathTools, GetExtension) {
  ASSERT_EQ("", common::GetExtension(""));
  ASSERT_EQ(".ext", common::GetExtension(".ext"));

  ASSERT_EQ("", common::GetExtension("test"));
  ASSERT_EQ(".ext", common::GetExtension("test.ext"));
  ASSERT_EQ(".ext2", common::GetExtension("test.ext.ext2"));

  ASSERT_EQ(".ext", common::GetExtension("/path/file.ext"));
  ASSERT_EQ(".yyy", common::GetExtension("/path.xxx/file.yyy"));
  ASSERT_EQ("", common::GetExtension("/path.ext/file"));
}

TEST(PathTools, RemoveExtension) {

  ASSERT_EQ("", common::RemoveExtension(""));
  ASSERT_EQ("", common::RemoveExtension(".ext"));

  ASSERT_EQ("test", common::RemoveExtension("test"));
  ASSERT_EQ("test", common::RemoveExtension("test.ext"));
  ASSERT_EQ("test.ext", common::RemoveExtension("test.ext.ext2"));

  ASSERT_EQ("/path/file", common::RemoveExtension("/path/file.ext"));
  ASSERT_EQ("/path.ext/file", common::RemoveExtension("/path.ext/file.ext"));
  ASSERT_EQ("/path.ext/file", common::RemoveExtension("/path.ext/file"));
}

TEST(PathTools, SplitPath) {
  std::string dir;
  std::string file;

  common::SplitPath("/path/more/file", dir, file);

  ASSERT_EQ("/path/more", dir);
  ASSERT_EQ("file", file);

  common::SplitPath("file.ext", dir, file);

  ASSERT_EQ("", dir);
  ASSERT_EQ("file.ext", file);

  common::SplitPath("/path/more/", dir, file);

  ASSERT_EQ("/path/more", dir);
  ASSERT_EQ("", file);
}

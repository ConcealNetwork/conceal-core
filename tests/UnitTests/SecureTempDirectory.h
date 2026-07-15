// Copyright (c) 2018-2026 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license.

#pragma once

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>

#ifndef _WIN32
#include <stdlib.h>
#endif

namespace unit_test
{
  // Atomically create a private (0700) directory under the system temp path.
  // Prefer this over unique_path()+create_directories() (CWE-377 / TOCTOU).
  inline boost::filesystem::path createSecureTempDirectory(const std::string &prefix)
  {
#ifndef _WIN32
    const boost::filesystem::path templatePath =
        boost::filesystem::temp_directory_path() / (prefix + "XXXXXX");
    std::string templateStr = templatePath.string();
    std::vector<char> templateBuf(templateStr.begin(), templateStr.end());
    templateBuf.push_back('\0');

    char *created = ::mkdtemp(templateBuf.data());
    if (created == nullptr)
    {
      throw std::runtime_error(std::string("mkdtemp failed: ") +
                               std::strerror(errno));
    }
    return boost::filesystem::path(created);
#else
    // Windows temp directories are per-user; create_directories is adequate here.
    boost::filesystem::path dir =
        boost::filesystem::temp_directory_path() /
        boost::filesystem::unique_path(prefix + "%%%%%%%%");
    boost::system::error_code ec;
    boost::filesystem::create_directories(dir, ec);
    if (ec)
    {
      throw std::runtime_error(ec.message());
    }
    return dir;
#endif
  }
}

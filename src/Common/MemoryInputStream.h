// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "IInputStream.h"

namespace Common {

  class MemoryInputStream : public IInputStream {
  public:
    MemoryInputStream(const void* buffer, uint64_t bufferSize);
    uint64_t getPosition() const;
    bool endOfStream() const;
    
    // IInputStream
    virtual uint64_t readSome(void* data, uint64_t size) override;

  private:
    const char* buffer;
    uint64_t bufferSize;
    uint64_t position;
  };
}

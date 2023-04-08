// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <CryptoNote.h>
#include "BinaryInputStreamSerializer.h"
#include "BinaryOutputStreamSerializer.h"
#include "Common/MemoryInputStream.h"
#include "Common/StdInputStream.h"
#include "Common/StdOutputStream.h"
#include "Common/VectorOutputStream.h"

#include <fstream>

namespace cn {

template <typename T>
BinaryArray storeToBinary(const T& obj) {
  BinaryArray result;
  common::VectorOutputStream stream(result);
  BinaryOutputStreamSerializer ba(stream);
  serialize(const_cast<T&>(obj), ba);
  return result;
}

template <typename T>
void loadFromBinary(T& obj, const BinaryArray& blob) {
  common::MemoryInputStream stream(blob.data(), blob.size());
  BinaryInputStreamSerializer ba(stream);
  serialize(obj, ba);
}

template <typename T>
bool storeToBinaryFile(const T& obj, const std::string& filename) {
  try {
    std::ofstream dataFile;
    dataFile.open(filename, std::ios_base::binary | std::ios_base::out | std::ios::trunc);
    if (dataFile.fail()) {
      return false;
    }

    common::StdOutputStream stream(dataFile);
    BinaryOutputStreamSerializer out(stream);
    cn::serialize(const_cast<T&>(obj), out);
      
    if (dataFile.fail()) {
      return false;
    }

    dataFile.flush();
  } catch (std::exception&) {
    return false;
  }

  return true;
}

template<class T>
bool loadFromBinaryFile(T& obj, const std::string& filename) {
  try {
    std::ifstream dataFile;
    dataFile.open(filename, std::ios_base::binary | std::ios_base::in);
    if (dataFile.fail()) {
      return false;
    }

    common::StdInputStream stream(dataFile);
    BinaryInputStreamSerializer in(stream);
    serialize(obj, in);
    return !dataFile.fail();
  } catch (std::exception&) {
    return false;
  }
}

}

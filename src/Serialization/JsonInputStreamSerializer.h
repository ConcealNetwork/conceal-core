// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <iosfwd>
#include <string>
#include <vector>
#include "../Common/JsonValue.h"
#include "JsonInputValueSerializer.h"

namespace CryptoNote {

//deserialization
class JsonInputStreamSerializer : public JsonInputValueSerializer {
public:
  JsonInputStreamSerializer(std::istream& stream);
  virtual ~JsonInputStreamSerializer();
};

}

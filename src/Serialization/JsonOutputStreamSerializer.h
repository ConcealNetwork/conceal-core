// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2022 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <iostream>
#include "../Common/JsonValue.h"
#include "ISerializer.h"

namespace cn {

class JsonOutputStreamSerializer : public ISerializer {
public:
  JsonOutputStreamSerializer();
  virtual ~JsonOutputStreamSerializer();

  SerializerType type() const override;

  virtual bool beginObject(common::StringView name) override;
  virtual void endObject() override;

  virtual bool beginArray(size_t& size, common::StringView name) override;
  virtual void endArray() override;

  virtual bool operator()(uint8_t& value, common::StringView name) override;
  virtual bool operator()(int16_t& value, common::StringView name) override;
  virtual bool operator()(uint16_t& value, common::StringView name) override;
  virtual bool operator()(int32_t& value, common::StringView name) override;
  virtual bool operator()(uint32_t& value, common::StringView name) override;
  virtual bool operator()(int64_t& value, common::StringView name) override;
  virtual bool operator()(uint64_t& value, common::StringView name) override;
  virtual bool operator()(double& value, common::StringView name) override;
  virtual bool operator()(bool& value, common::StringView name) override;
  virtual bool operator()(std::string& value, common::StringView name) override;
  virtual bool binary(void* value, size_t size, common::StringView name) override;
  virtual bool binary(std::string& value, common::StringView name) override;

  template<typename T>
  bool operator()(T& value, common::StringView name) {
    return ISerializer::operator()(value, name);
  }

  const common::JsonValue& getValue() const {
    return root;
  }

  friend std::ostream& operator<<(std::ostream& out, const JsonOutputStreamSerializer& enumerator);

private:
  common::JsonValue root;
  std::vector<common::JsonValue*> chain;
};

}

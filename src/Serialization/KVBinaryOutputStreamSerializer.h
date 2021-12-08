// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2021 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <vector>
#include <Common/IOutputStream.h>
#include "ISerializer.h"
#include "MemoryStream.h"

namespace cn {

class KVBinaryOutputStreamSerializer : public ISerializer {
public:

  KVBinaryOutputStreamSerializer();
  virtual ~KVBinaryOutputStreamSerializer() {}

  void dump(common::IOutputStream& target);

  virtual ISerializer::SerializerType type() const override;

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

private:

  void writeElementPrefix(uint8_t type, common::StringView name);
  void checkArrayPreamble(uint8_t type);
  void updateState(uint8_t type);
  MemoryStream& stream();

  enum class State {
    Root,
    Object,
    ArrayPrefix,
    Array
  };

  struct Level {
    State state;
    std::string name;
    size_t count;

    Level(common::StringView nm) :
      name(nm), state(State::Object), count(0) {}

    Level(common::StringView nm, size_t arraySize) :
      name(nm), state(State::ArrayPrefix), count(arraySize) {}

    Level(Level&& rv) {
      state = rv.state;
      name = std::move(rv.name);
      count = rv.count;
    }

  };

  std::vector<MemoryStream> m_objectsStack;
  std::vector<Level> m_stack;
};

}

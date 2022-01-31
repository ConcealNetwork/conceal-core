// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2022 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <list>
#include <vector>
#include <Common/MemoryInputStream.h>
#include <Common/StringOutputStream.h>
#include "JsonInputStreamSerializer.h"
#include "JsonOutputStreamSerializer.h"
#include "KVBinaryInputStreamSerializer.h"
#include "KVBinaryOutputStreamSerializer.h"

namespace common {

template <typename T>
T getValueAs(const JsonValue& js) {
  return js;
  //cdstatic_assert(false, "undefined conversion");
}

template <>
inline std::string getValueAs<std::string>(const JsonValue& js) { return js.getString(); }

template <>
inline uint64_t getValueAs<uint64_t>(const JsonValue& js) { return static_cast<uint64_t>(js.getInteger()); }

}

namespace cn {

template <typename T>
common::JsonValue storeToJsonValue(const T& v) {
  JsonOutputStreamSerializer s;
  serialize(const_cast<T&>(v), s);
  return s.getValue();
}

template <typename T>
common::JsonValue storeContainerToJsonValue(const T& cont) {
  common::JsonValue js(common::JsonValue::ARRAY);
  for (const auto& item : cont) {
    js.pushBack(item);
  }
  return js;
}

template <typename T>
common::JsonValue storeToJsonValue(const std::vector<T>& v) { return storeContainerToJsonValue(v); }

template <typename T>
common::JsonValue storeToJsonValue(const std::list<T>& v) { return storeContainerToJsonValue(v); }

template <>
inline common::JsonValue storeToJsonValue(const std::string& v) { return common::JsonValue(v); }

template <typename T>
void loadFromJsonValue(T& v, const common::JsonValue& js) {
  JsonInputValueSerializer s(js);
  serialize(v, s);
}

template <typename T>
void loadFromJsonValue(std::vector<T>& v, const common::JsonValue& js) {
  for (size_t i = 0; i < js.size(); ++i) {
    v.push_back(common::getValueAs<T>(js[i]));
  }
}

template <typename T>
void loadFromJsonValue(std::list<T>& v, const common::JsonValue& js) {
  for (size_t i = 0; i < js.size(); ++i) {
    v.push_back(common::getValueAs<T>(js[i]));
  }
}

template <typename T>
std::string storeToJson(const T& v) {
  return storeToJsonValue(v).toString();
}

template <typename T>
bool loadFromJson(T& v, const std::string& buf) {
  try {
    if (buf.empty()) {
      return true;
    }
    auto js = common::JsonValue::fromString(buf);
    loadFromJsonValue(v, js);
  } catch (std::exception&) {
    return false;
  }
  return true;
}

template <typename T>
std::string storeToBinaryKeyValue(const T& v) {
  KVBinaryOutputStreamSerializer s;
  serialize(const_cast<T&>(v), s);
  
  std::string result;
  common::StringOutputStream stream(result);
  s.dump(stream);
  return result;
}

template <typename T>
bool loadFromBinaryKeyValue(T& v, const std::string& buf) {
  try {
    common::MemoryInputStream stream(buf.data(), buf.size());
    KVBinaryInputStreamSerializer s(stream);
    serialize(v, s);
    return true;
  } catch (std::exception&) {
    return false;
  }
}

}

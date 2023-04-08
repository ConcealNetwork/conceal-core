// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "BinaryOutputStreamSerializer.h"

#include <cassert>
#include <stdexcept>
#include "Common/StreamTools.h"

using namespace common;

namespace cn {

ISerializer::SerializerType BinaryOutputStreamSerializer::type() const {
  return ISerializer::OUTPUT;
}

bool BinaryOutputStreamSerializer::beginObject(common::StringView name) {
  return true;
}

void BinaryOutputStreamSerializer::endObject() {
}

bool BinaryOutputStreamSerializer::beginArray(size_t& size, common::StringView name) {
  writeVarint(stream, size);
  return true;
}

void BinaryOutputStreamSerializer::endArray() {
}

bool BinaryOutputStreamSerializer::operator()(uint8_t& value, common::StringView name) {
  writeVarint(stream, value);
  return true;
}

bool BinaryOutputStreamSerializer::operator()(uint16_t& value, common::StringView name) {
  writeVarint(stream, value);
  return true;
}

bool BinaryOutputStreamSerializer::operator()(int16_t& value, common::StringView name) {
  writeVarint(stream, static_cast<uint16_t>(value));
  return true;
}

bool BinaryOutputStreamSerializer::operator()(uint32_t& value, common::StringView name) {
  writeVarint(stream, value);
  return true;
}

bool BinaryOutputStreamSerializer::operator()(int32_t& value, common::StringView name) {
  writeVarint(stream, static_cast<uint32_t>(value));
  return true;
}

bool BinaryOutputStreamSerializer::operator()(int64_t& value, common::StringView name) {
  writeVarint(stream, static_cast<uint64_t>(value));
  return true;
}

bool BinaryOutputStreamSerializer::operator()(uint64_t& value, common::StringView name) {
  writeVarint(stream, value);
  return true;
}

bool BinaryOutputStreamSerializer::operator()(bool& value, common::StringView name) {
  char boolVal = value;
  checkedWrite(&boolVal, 1);
  return true;
}

bool BinaryOutputStreamSerializer::operator()(std::string& value, common::StringView name) {
  writeVarint(stream, value.size());
  checkedWrite(value.data(), value.size());
  return true;
}

bool BinaryOutputStreamSerializer::binary(void* value, size_t size, common::StringView name) {
  checkedWrite(static_cast<const char*>(value), size);
  return true;
}

bool BinaryOutputStreamSerializer::binary(std::string& value, common::StringView name) {
  // write as string (with size prefix)
  return (*this)(value, name);
}

bool BinaryOutputStreamSerializer::operator()(double& value, common::StringView name) {
  assert(false); //the method is not supported for this type of serialization
  throw std::runtime_error("BinaryOutputStreamSerializer does not support double serialization");
  return false;
}

void BinaryOutputStreamSerializer::checkedWrite(const char* buf, size_t size) {
  write(stream, buf, size);
}

}

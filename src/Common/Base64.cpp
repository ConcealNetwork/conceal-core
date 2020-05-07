// Copyright (c) 2016-2019, The Karbo developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2020 Conceal Network & Conceal Devs

// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "Base64.h"

namespace Tools {
namespace Base64 {

namespace {
  static const std::string base64chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

  bool is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
  }
}

std::string encode(const std::string& data) {
  const uint64_t resultSize = 4 * ((data.size() + 2) / 3);
  std::string result;
  result.reserve(resultSize);

  for (uint64_t i = 0; i < data.size(); i += 3) {
    uint64_t a = static_cast<uint64_t>(data[i]);
    uint64_t b = i + 1 < data.size() ? static_cast<uint64_t>(data[i + 1]) : 0;
    uint64_t c = i + 2 < data.size() ? static_cast<uint64_t>(data[i + 2]) : 0;

    result.push_back(base64chars[a >> 2]);
    result.push_back(base64chars[((a & 0x3) << 4) | (b >> 4)]);
    if (i + 1 < data.size()) {
      result.push_back(base64chars[((b & 0xF) << 2) | (c >> 6)]);
      if (i + 2 < data.size()) {
        result.push_back(base64chars[c & 0x3F]);
      }
    }
  }

  while (result.size() != resultSize) {
    result.push_back('=');
  }

  return result;
}
  
std::string decode(std::string const& encoded_string) {
  uint64_t in_len = encoded_string.size();
  uint64_t i = 0;
  uint64_t j = 0;
  uint64_t in_ = 0;
  unsigned char char_array_4[4], char_array_3[3];
  std::string ret;

  while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
    char_array_4[i++] = encoded_string[in_]; in_++;
    if (i == 4) {
      for (i = 0; i <4; i++)
        char_array_4[i] = (unsigned char)base64chars.find(char_array_4[i]);

      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

      for (i = 0; (i < 3); i++)
        ret += char_array_3[i];
      i = 0;
    }
  }

  if (i) {
    for (j = i; j <4; j++)
      char_array_4[j] = 0;

    for (j = 0; j <4; j++)
      char_array_4[j] = (unsigned char)base64chars.find(char_array_4[j]);

    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

    for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
  }

  return ret;
}

} // Base64
} // Tools

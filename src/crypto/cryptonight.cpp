// Copyright (c) 2019 The Circle Foundation
// Copyright (c) 2019 fireice-uk
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow_hash/cn_slow_hash.hpp"
#include "cryptonight.hpp"

namespace Crypto {


void cn_slow_hash_v0(cn_context &context, const void *data, size_t length, Hash &hash) {
	if(hw_check_aes())
		cryptonight_hash<true, CRYPTONIGHT>(data, length, reinterpret_cast<char *>(&hash), context);
	else
		cryptonight_hash<false, CRYPTONIGHT>(data, length, reinterpret_cast<char *>(&hash), context);
}

void cn_fast_slow_hash_v1(cn_context &context, const void *data, size_t length, Hash &hash) {
	if(hw_check_aes())
		cryptonight_hash<true, CRYPTONIGHT_FAST_V8>(data, length, reinterpret_cast<char *>(&hash), context);
	else
		cryptonight_hash<false, CRYPTONIGHT_FAST_V8>(data, length, reinterpret_cast<char *>(&hash), context);
}

void cn_conceal_slow_hash_v0(cn_context &context, const void *data, size_t length, Hash &hash) {
	if(hw_check_aes())
		cryptonight_hash<true, CRYPTONIGHT_CONCEAL>(data, length, reinterpret_cast<char *>(&hash), context);
	else
		cryptonight_hash<false, CRYPTONIGHT_CONCEAL>(data, length, reinterpret_cast<char *>(&hash), context);
}

void cn_gpu_hash_v0(cn_context &context, const void *data, size_t length, Hash &hash) {
    context.cn_gpu_state.hash(data, length, reinterpret_cast<char *>(&hash));
}
}

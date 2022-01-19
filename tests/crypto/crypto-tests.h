// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#if defined(__cplusplus)
#include "crypto/crypto.h"

extern "C" {
#endif

void setup_random(void);

#if defined(__cplusplus)
}

bool check_scalar(const crypto::EllipticCurveScalar &scalar);
void random_scalar(crypto::EllipticCurveScalar &res);
void hash_to_scalar(const void *data, size_t length, crypto::EllipticCurveScalar &res);
void hash_to_point(const crypto::Hash &h, crypto::EllipticCurvePoint &res);
void hash_to_ec(const crypto::PublicKey &key, crypto::EllipticCurvePoint &res);
#endif

// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypto/crypto.cpp"

#include "crypto-tests.h"

bool check_scalar(const crypto::EllipticCurveScalar &scalar) {
  return crypto::sc_check(reinterpret_cast<const unsigned char*>(&scalar)) == 0;
}

void random_scalar(crypto::EllipticCurveScalar &res) {
  crypto::random_scalar(res);
}

void hash_to_scalar(const void *data, size_t length, crypto::EllipticCurveScalar &res) {
  crypto::hash_to_scalar(data, length, res);
}

void hash_to_point(const crypto::Hash &h, crypto::EllipticCurvePoint &res) {
  crypto::ge_p2 point;
  crypto::ge_fromfe_frombytes_vartime(&point, reinterpret_cast<const unsigned char *>(&h));
  crypto::ge_tobytes(reinterpret_cast<unsigned char*>(&res), &point);
}

void hash_to_ec(const crypto::PublicKey &key, crypto::EllipticCurvePoint &res) {
  crypto::ge_p3 tmp;
  crypto::hash_to_ec(key, tmp);
  crypto::ge_p3_tobytes(reinterpret_cast<unsigned char*>(&res), &tmp);
}

// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "crypto/crypto.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"

#include "SingleTransactionTestBase.h"

class test_derive_secret_key : public single_tx_test_base
{
public:
  static const size_t loop_count = 1000000;

  bool init()
  {
    if (!single_tx_test_base::init())
      return false;

    crypto::generate_key_derivation(m_tx_pub_key, m_bob.getAccountKeys().viewSecretKey, m_key_derivation);
    m_spend_secret_key = m_bob.getAccountKeys().spendSecretKey;

    return true;
  }

  bool test()
  {
    cn::KeyPair in_ephemeral;
    crypto::derive_secret_key(m_key_derivation, 0, m_spend_secret_key, in_ephemeral.secretKey);
    return true;
  }

private:
  crypto::KeyDerivation m_key_derivation;
  crypto::SecretKey m_spend_secret_key;
};

// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "crypto/crypto.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"

#include "SingleTransactionTestBase.h"

class test_generate_key_image : public single_tx_test_base
{
public:
  static const size_t loop_count = 1000;

  bool init()
  {
    using namespace cn;

    if (!single_tx_test_base::init())
      return false;

    AccountKeys bob_keys = m_bob.getAccountKeys();

    crypto::KeyDerivation recv_derivation;
    crypto::generate_key_derivation(m_tx_pub_key, bob_keys.viewSecretKey, recv_derivation);

    crypto::derive_public_key(recv_derivation, 0, bob_keys.address.spendPublicKey, m_in_ephemeral.publicKey);
    crypto::derive_secret_key(recv_derivation, 0, bob_keys.spendSecretKey, m_in_ephemeral.secretKey);

    return true;
  }

  bool test()
  {
    crypto::KeyImage ki;
    crypto::generate_key_image(m_in_ephemeral.publicKey, m_in_ephemeral.secretKey, ki);
    return true;
  }

private:
  cn::KeyPair m_in_ephemeral;
};

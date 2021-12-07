// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2021 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "WalletUtils.h"

#include "CryptoNote.h"
#include "crypto/crypto.h"
#include "Wallet/WalletErrors.h"

namespace cn {

bool validateAddress(const std::string& address, const cn::Currency& currency) {
  cn::AccountPublicAddress ignore;
  return currency.parseAccountAddressString(address, ignore);
}

void throwIfKeysMissmatch(const Crypto::SecretKey& secretKey, const Crypto::PublicKey& expectedPublicKey, const std::string& message) {
  Crypto::PublicKey pub;
  bool r = Crypto::secret_key_to_public_key(secretKey, pub);
  if (!r || expectedPublicKey != pub) {
    throw std::system_error(make_error_code(cn::error::WRONG_PASSWORD), message);
  }
}

}

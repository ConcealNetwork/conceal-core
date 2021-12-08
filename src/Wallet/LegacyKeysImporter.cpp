// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2021 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "LegacyKeysImporter.h"

#include <vector>
#include <system_error>

#include "Common/StringTools.h"

#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/CryptoNoteTools.h"

#include "Serialization/SerializationTools.h"

#include "WalletLegacy/WalletLegacySerializer.h"
#include "WalletLegacy/WalletUserTransactionsCache.h"
#include "Wallet/WalletErrors.h"

using namespace crypto;

namespace {

struct keys_file_data {
  chacha8_iv iv;
  std::string account_data;

  void serialize(cn::ISerializer& s) {
    s(iv, "iv");
    s(account_data, "account_data");
  }
};

bool verify_keys(const SecretKey& sec, const PublicKey& expected_pub) {
  PublicKey pub;
  bool r = secret_key_to_public_key(sec, pub);
  return r && expected_pub == pub;
}

void loadKeysFromFile(const std::string& filename, const std::string& password, cn::AccountBase& account) {
  keys_file_data keys_file_data;
  std::string buf;

  if (!common::loadFileToString(filename, buf)) {
    throw std::system_error(make_error_code(cn::error::INTERNAL_WALLET_ERROR), "failed to load \"" + filename + '\"');
  }

  if (!cn::fromBinaryArray(keys_file_data, common::asBinaryArray(buf))) {
    throw std::system_error(make_error_code(cn::error::INTERNAL_WALLET_ERROR), "failed to deserialize \"" + filename + '\"');
  }

  chacha8_key key;
  cn_context cn_context;
  generate_chacha8_key(cn_context, password, key);
  std::string account_data;
  account_data.resize(keys_file_data.account_data.size());
  chacha8(keys_file_data.account_data.data(), keys_file_data.account_data.size(), key, keys_file_data.iv, &account_data[0]);

  const cn::AccountKeys& keys = account.getAccountKeys();

  if (cn::loadFromBinaryKeyValue(account, account_data) &&
      verify_keys(keys.viewSecretKey, keys.address.viewPublicKey) &&
      verify_keys(keys.spendSecretKey, keys.address.spendPublicKey)) {
    return;
  }

  throw std::system_error(make_error_code(cn::error::WRONG_PASSWORD));
}

}

namespace cn {

void importLegacyKeys(const std::string& legacyKeysFilename, const std::string& password, std::ostream& destination) {
  cn::AccountBase account;

  loadKeysFromFile(legacyKeysFilename, password, account);

  cn::WalletUserTransactionsCache transactionsCache;
  std::string cache;
  cn::WalletLegacySerializer importer(account, transactionsCache);
  importer.serialize(destination, password, false, cache);
}

} //namespace cn

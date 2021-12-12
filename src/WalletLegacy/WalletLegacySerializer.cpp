// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2021 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "WalletLegacySerializer.h"

#include <stdexcept>

#include "Common/MemoryInputStream.h"
#include "Common/StdInputStream.h"
#include "Common/StdOutputStream.h"
#include "Serialization/BinaryOutputStreamSerializer.h"
#include "Serialization/BinaryInputStreamSerializer.h"
#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/CryptoNoteSerialization.h"
#include "WalletLegacy/WalletUserTransactionsCache.h"
#include "Wallet/WalletErrors.h"
#include "WalletLegacy/KeysStorage.h"
#include "crypto/chacha8.h"

using namespace common;

namespace {

const uint32_t WALLET_SERIALIZATION_VERSION = 2;

bool verifyKeys(const crypto::SecretKey& sec, const crypto::PublicKey& expected_pub) {
  crypto::PublicKey pub;
  bool r = crypto::secret_key_to_public_key(sec, pub);
  return r && expected_pub == pub;
}

void throwIfKeysMissmatch(const crypto::SecretKey& sec, const crypto::PublicKey& expected_pub) {
  if (!verifyKeys(sec, expected_pub))
    throw std::system_error(make_error_code(cn::error::WRONG_PASSWORD));
}

}

namespace cn {

WalletLegacySerializer::WalletLegacySerializer(cn::AccountBase& account, WalletUserTransactionsCache& transactionsCache) :
  account(account),
  transactionsCache(transactionsCache),
  walletSerializationVersion(WALLET_SERIALIZATION_VERSION)
{
}

void WalletLegacySerializer::serialize(std::ostream& stream, const std::string& password, bool saveDetailed, const std::string& cache) {
  std::stringstream plainArchive;
  StdOutputStream plainStream(plainArchive);
  cn::BinaryOutputStreamSerializer serializer(plainStream);
  saveKeys(serializer);

  serializer(saveDetailed, "has_details");

  if (saveDetailed) {
    serializer(transactionsCache, "details");
  }

  serializer.binary(const_cast<std::string&>(cache), "cache");

  std::string plain = plainArchive.str();
  std::string cipher;

  crypto::chacha8_iv iv = encrypt(plain, password, cipher);

  uint32_t version = walletSerializationVersion;
  StdOutputStream output(stream);
  cn::BinaryOutputStreamSerializer s(output);
  s.beginObject("wallet");
  s(version, "version");
  s(iv, "iv");
  s(cipher, "data");
  s.endObject();

  stream.flush();
}

void WalletLegacySerializer::saveKeys(cn::ISerializer& serializer) {
  cn::KeysStorage keys;
  cn::AccountKeys acc = account.getAccountKeys();

  keys.creationTimestamp = account.get_createtime();
  keys.spendPublicKey = acc.address.spendPublicKey;
  keys.spendSecretKey = acc.spendSecretKey;
  keys.viewPublicKey = acc.address.viewPublicKey;
  keys.viewSecretKey = acc.viewSecretKey;

  keys.serialize(serializer, "keys");
}

crypto::chacha8_iv WalletLegacySerializer::encrypt(const std::string& plain, const std::string& password, std::string& cipher) {
  crypto::chacha8_key key;
  crypto::cn_context context;
  crypto::generate_chacha8_key(context, password, key);

  cipher.resize(plain.size());

  crypto::chacha8_iv iv = crypto::rand<crypto::chacha8_iv>();
  crypto::chacha8(plain.data(), plain.size(), key, iv, &cipher[0]);

  return iv;
}


void WalletLegacySerializer::deserialize(std::istream& stream, const std::string& password, std::string& cache) {
  StdInputStream stdStream(stream);
  cn::BinaryInputStreamSerializer serializerEncrypted(stdStream);

  serializerEncrypted.beginObject("wallet");

  uint32_t version;
  serializerEncrypted(version, "version");

  crypto::chacha8_iv iv;
  serializerEncrypted(iv, "iv");

  std::string cipher;
  serializerEncrypted(cipher, "data");

  serializerEncrypted.endObject();

  std::string plain;
  decrypt(cipher, plain, iv, password);

  MemoryInputStream decryptedStream(plain.data(), plain.size()); 
  cn::BinaryInputStreamSerializer serializer(decryptedStream);

  loadKeys(serializer);
  throwIfKeysMissmatch(account.getAccountKeys().viewSecretKey, account.getAccountKeys().address.viewPublicKey);

  if (account.getAccountKeys().spendSecretKey != NULL_SECRET_KEY) {
    throwIfKeysMissmatch(account.getAccountKeys().spendSecretKey, account.getAccountKeys().address.spendPublicKey);
  } else {
    if (!crypto::check_key(account.getAccountKeys().address.spendPublicKey)) {
      throw std::system_error(make_error_code(cn::error::WRONG_PASSWORD));
    }
  }

  bool detailsSaved;

  serializer(detailsSaved, "has_details");

  if (detailsSaved) {
    if (version == 1) {
      transactionsCache.deserializeLegacyV1(serializer);
    } else {
      serializer(transactionsCache, "details");
    }
  }

  serializer.binary(cache, "cache");
}

bool WalletLegacySerializer::deserialize(std::istream& stream, const std::string& password) {
  try {
    StdInputStream stdStream(stream);
    cn::BinaryInputStreamSerializer serializerEncrypted(stdStream);

    serializerEncrypted.beginObject("wallet");

    uint32_t version;
    serializerEncrypted(version, "version");

    crypto::chacha8_iv iv;
    serializerEncrypted(iv, "iv");

    std::string cipher;
    serializerEncrypted(cipher, "data");

    serializerEncrypted.endObject();

    std::string plain;
    decrypt(cipher, plain, iv, password);

    MemoryInputStream decryptedStream(plain.data(), plain.size());
    cn::BinaryInputStreamSerializer serializer(decryptedStream);

    cn::KeysStorage keys;
    try {
      keys.serialize(serializer, "keys");
    }
    catch (const std::runtime_error&) {
      return false;
    }
    cn::AccountKeys acc;
    acc.address.spendPublicKey = keys.spendPublicKey;
    acc.spendSecretKey = keys.spendSecretKey;
    acc.address.viewPublicKey = keys.viewPublicKey;
    acc.viewSecretKey = keys.viewSecretKey;

    crypto::PublicKey pub;
    bool r = crypto::secret_key_to_public_key(acc.viewSecretKey, pub);
    if (!r || acc.address.viewPublicKey != pub) {
      return false;
    }

    if (acc.spendSecretKey != NULL_SECRET_KEY) {
      crypto::PublicKey pub;
      bool r = crypto::secret_key_to_public_key(acc.spendSecretKey, pub);
      if (!r || acc.address.spendPublicKey != pub) {
        return false;
      }
    }
    else {
      if (!crypto::check_key(acc.address.spendPublicKey)) {
        return false;
      }
    }
  }
  catch (std::system_error&) {
    return false;
  }
  catch (std::exception&) {
    return false;
  }

  return true;
}




void WalletLegacySerializer::decrypt(const std::string& cipher, std::string& plain, crypto::chacha8_iv iv, const std::string& password) {
  crypto::chacha8_key key;
  crypto::cn_context context;
  crypto::generate_chacha8_key(context, password, key);

  plain.resize(cipher.size());

  crypto::chacha8(cipher.data(), cipher.size(), key, iv, &plain[0]);
}

void WalletLegacySerializer::loadKeys(cn::ISerializer& serializer) {
  cn::KeysStorage keys;

  try {
    keys.serialize(serializer, "keys");
  } catch (const std::runtime_error&) {
    throw std::system_error(make_error_code(cn::error::WRONG_PASSWORD));
  }

  cn::AccountKeys acc;
  acc.address.spendPublicKey = keys.spendPublicKey;
  acc.spendSecretKey = keys.spendSecretKey;
  acc.address.viewPublicKey = keys.viewPublicKey;
  acc.viewSecretKey = keys.viewSecretKey;

  account.setAccountKeys(acc);
  account.set_createtime(keys.creationTimestamp);
}

}

// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2021 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "KeysStorage.h"

#include "WalletLegacy/WalletLegacySerialization.h"
#include "Serialization/ISerializer.h"
#include "Serialization/SerializationOverloads.h"
#include "CryptoNoteCore/CryptoNoteSerialization.h"

namespace cn {

void KeysStorage::serialize(ISerializer& serializer, const std::string& name) {
  serializer.beginObject(name);

  serializer(creationTimestamp, "creation_timestamp");

  serializer(spendPublicKey, "spend_public_key");
  serializer(spendSecretKey, "spend_secret_key");

  serializer(viewPublicKey, "view_public_key");
  serializer(viewSecretKey, "view_secret_key");

  serializer.endObject();
}

}

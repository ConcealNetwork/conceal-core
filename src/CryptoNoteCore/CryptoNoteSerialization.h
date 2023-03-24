// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "CryptoNoteBasic.h"
#include "crypto/chacha8.h"
#include "Serialization/ISerializer.h"
#include "crypto/crypto.h"

namespace crypto {

bool serialize(PublicKey& pubKey, common::StringView name, cn::ISerializer& serializer);
bool serialize(SecretKey& secKey, common::StringView name, cn::ISerializer& serializer);
bool serialize(Hash& h, common::StringView name, cn::ISerializer& serializer);
bool serialize(chacha8_iv &chacha8, common::StringView name, cn::ISerializer &serializer);
bool serialize(KeyImage &keyImage, common::StringView name, cn::ISerializer &serializer);
bool serialize(Signature& sig, common::StringView name, cn::ISerializer& serializer);
bool serialize(EllipticCurveScalar& ecScalar, common::StringView name, cn::ISerializer& serializer);
bool serialize(EllipticCurvePoint& ecPoint, common::StringView name, cn::ISerializer& serializer);

}

namespace cn {

struct AccountKeys;
struct TransactionExtraMergeMiningTag;

void serialize(TransactionPrefix& txP, ISerializer& serializer);
void serialize(Transaction& tx, ISerializer& serializer);
void serialize(TransactionInput& in, ISerializer& serializer);
void serialize(TransactionOutput& in, ISerializer& serializer);

void serialize(BaseInput& gen, ISerializer& serializer);
void serialize(KeyInput& key, ISerializer& serializer);
void serialize(MultisignatureInput& multisignature, ISerializer& serializer);

void serialize(TransactionOutput& output, ISerializer& serializer);
void serialize(TransactionOutputTarget& output, ISerializer& serializer);
void serialize(KeyOutput& key, ISerializer& serializer);
void serialize(MultisignatureOutput& multisignature, ISerializer& serializer);

void serialize(BlockHeader& header, ISerializer& serializer);
void serialize(Block& block, ISerializer& serializer);
void serialize(TransactionExtraMergeMiningTag& tag, ISerializer& serializer);

void serialize(AccountPublicAddress& address, ISerializer& serializer);
void serialize(AccountKeys& keys, ISerializer& s);
void serialize(TransactionInputs &inputs, ISerializer &serializer);

void serialize(KeyPair& keyPair, ISerializer& serializer);

}

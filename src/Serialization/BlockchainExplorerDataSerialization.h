// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "BlockchainExplorerData.h"
#include "BlockchainExplorerData2.h"

#include "Serialization/ISerializer.h"

namespace CryptoNote {

void serialize(transaction_output_details& output, ISerializer& serializer);
void serialize(TransactionOutputReferenceDetails& outputReference, ISerializer& serializer);

void serialize(BaseInputDetails& inputBase, ISerializer& serializer);
void serialize(KeyInputDetails& inputToKey, ISerializer& serializer);
void serialize(MultisignatureInputDetails& inputMultisig, ISerializer& serializer);
void serialize(transaction_input_details& input, ISerializer& serializer);

void serialize(TransactionExtraDetails& extra, ISerializer& serializer);
//void serialize(TransactionDetails& transaction, ISerializer& serializer);
void serialize(TransactionDetails2& transaction, ISerializer& serializer);

//void serialize(BlockDetails& block, ISerializer& serializer);
void serialize(BlockDetails2& block, ISerializer& serializer);

} //namespace CryptoNote
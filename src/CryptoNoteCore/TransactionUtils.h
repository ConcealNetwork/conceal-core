// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2021 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "ITransaction.h"

namespace cn {

bool checkInputsKeyimagesDiff(const cn::TransactionPrefix& tx);

// TransactionInput helper functions
size_t getRequiredSignaturesCount(const TransactionInput& in);
uint64_t getTransactionInputAmount(const TransactionInput& in);
transaction_types::InputType getTransactionInputType(const TransactionInput& in);
const TransactionInput& getInputChecked(const cn::TransactionPrefix& transaction, size_t index);
const TransactionInput& getInputChecked(const cn::TransactionPrefix& transaction, size_t index, transaction_types::InputType type);

bool isOutToKey(const crypto::PublicKey& spendPublicKey, const crypto::PublicKey& outKey, const crypto::KeyDerivation& derivation, size_t keyIndex);

// TransactionOutput helper functions
transaction_types::OutputType getTransactionOutputType(const TransactionOutputTarget& out);
const TransactionOutput& getOutputChecked(const cn::TransactionPrefix& transaction, size_t index);
const TransactionOutput& getOutputChecked(const cn::TransactionPrefix& transaction, size_t index, transaction_types::OutputType type);

bool findOutputsToAccount(const cn::TransactionPrefix& transaction, const AccountPublicAddress& addr,
        const crypto::SecretKey& viewSecretKey, std::vector<uint32_t>& out, uint64_t& amount);

} //namespace cn

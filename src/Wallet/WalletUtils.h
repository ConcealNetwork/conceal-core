// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>

#include "CryptoNoteCore/Currency.h"

namespace cn {

bool validateAddress(const std::string& address, const cn::Currency& currency);
void throwIfKeysMissmatch(const crypto::SecretKey& secretKey, const crypto::PublicKey& expectedPublicKey, const std::string& message = "");
void importLegacyKeys(const std::string &legacyKeysFile, const std::string &filename, const std::string &password);
void createWalletFile(std::fstream &walletFile, const std::string &filename);
}

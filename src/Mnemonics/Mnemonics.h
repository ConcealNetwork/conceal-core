// Copyright 2014-2018 The Monero Developers
// Copyright (c) 2018-2022 Conceal Network & Conceal Devs
//
// Please see the included LICENSE file for more information.

#include "CryptoNote.h"

#include <vector>


namespace mnemonics
{
    crypto::SecretKey MnemonicToPrivateKey(const std::string words);
    crypto::SecretKey MnemonicToPrivateKey(const std::vector<std::string> words);

    std::string PrivateKeyToMnemonic(const crypto::SecretKey privateKey);

    bool HasValidChecksum(const std::vector<std::string> words);

    std::string GetChecksumWord(const std::vector<std::string> words);

    std::vector<int> GetWordIndexes(const std::vector<std::string> words);
}

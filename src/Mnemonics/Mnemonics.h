// Copyright 2014-2018 The Monero Developers
// Copyright (c) 2018-2022 Conceal Network & Conceal Devs
//
// Please see the included LICENSE file for more information.

#include "CryptoNote.h"

#include <vector>

namespace mnemonics
{
    crypto::SecretKey mnemonicToPrivateKey(const std::string &words);
    crypto::SecretKey mnemonicToPrivateKey(const std::vector<std::string> &words);

    std::string privateKeyToMnemonic(const crypto::SecretKey &privateKey);

    bool hasValidChecksum(const std::vector<std::string> &words);

    std::string getChecksumWord(const std::vector<std::string> &words);

    std::vector<int> getWordIndexes(const std::vector<std::string> &words);
}

// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "WalletUtils.h"

#include <fstream>

#include <boost/filesystem/operations.hpp>

#include "Common/Util.h"
#include "CryptoNote.h"
#include "crypto/crypto.h"
#include "Wallet/WalletErrors.h"
#include "Wallet/LegacyKeysImporter.h"


namespace cn {

bool validateAddress(const std::string& address, const cn::Currency& currency) {
  cn::AccountPublicAddress ignore;
  return currency.parseAccountAddressString(address, ignore);
}

void throwIfKeysMissmatch(const crypto::SecretKey& secretKey, const crypto::PublicKey& expectedPublicKey, const std::string& message) {
  crypto::PublicKey pub;
  bool r = crypto::secret_key_to_public_key(secretKey, pub);
  if (!r || expectedPublicKey != pub) {
    throw std::system_error(make_error_code(cn::error::WRONG_PASSWORD), message);
  }
}

  void importLegacyKeys(const std::string &legacyKeysFile, const std::string &filename, const std::string &password)
  {
    std::stringstream archive;

    cn::importLegacyKeys(legacyKeysFile, password, archive);

    std::fstream walletFile;
    createWalletFile(walletFile, filename);

    archive.flush();
    walletFile << archive.rdbuf();
    walletFile.flush();
  }

  void createWalletFile(std::fstream &walletFile, const std::string &filename)
  {
    boost::filesystem::path pathToWalletFile(filename);
    boost::filesystem::path directory = pathToWalletFile.parent_path();
    if (!directory.empty() && !tools::directoryExists(directory.string()))
    {
      throw std::runtime_error("Directory does not exist: " + directory.string());
    }

    walletFile.open(filename.c_str(), std::fstream::in | std::fstream::out | std::fstream::binary);
    if (walletFile)
    {
      walletFile.close();
      throw std::runtime_error("Wallet file already exists");
    }

    walletFile.open(filename.c_str(), std::fstream::out);
    walletFile.close();

    walletFile.open(filename.c_str(), std::fstream::in | std::fstream::out | std::fstream::binary);
  }
}

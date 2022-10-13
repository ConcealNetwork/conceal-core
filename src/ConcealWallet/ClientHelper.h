// Copyright (c) 2018-2022 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>

#include "IWalletLegacy.h"

#include "Common/JsonValue.h"
#include "CryptoNoteCore/Currency.h"

#include <Logging/LoggerRef.h>

using namespace common;
using common::JsonValue;

namespace cn
{
  struct ListedTxItem
  { // these are strings, made to be used for exporting txs to csv
    std::string timestamp;
    std::string tx_hash;
    std::string amount;
    std::string fee;
    std::string block_height;
    std::string unlock_time;
  };

  struct ListedDepositItem
  { // these are strings, made to be used for exporting deposits to csv
    std::string timestamp;
    std::string id;
    std::string amount;
    std::string interest;
    std::string block_height;
    std::string unlock_time;
    std::string status;
  };

  class client_helper
  {
  public:
    /** conceal_wallet **/
    JsonValue buildLoggerConfiguration(logging::Level level, const std::string& logfile);
    std::error_code initAndLoadWallet(cn::IWalletLegacy& wallet, std::istream& walletFile, const std::string& password);

    bool parseUrlAddress(const std::string& url, std::string& address, uint16_t& port);
    bool write_addr_file(const std::string& addr_filename, const std::string& address);
    bool existing_file(const std::string address_file, logging::LoggerRef logger);

    void save_wallet(cn::IWalletLegacy& wallet, const std::string& walletFilename, logging::LoggerRef& logger);

    std::string prep_wallet_filename(const std::string& wallet_basename);
    std::string tryToOpenWalletOrLoadKeysOrThrow(logging::LoggerRef& logger, std::unique_ptr<cn::IWalletLegacy>& wallet, const std::string& walletFile, const std::string& password);
    std::string get_commands_str(bool do_ext);
    std::string wallet_commands(bool is_extended);

    /**
     * @return - "Locked", "Unlocked" or "Spent" depending on state of deposit
    **/
    std::string deposit_status(const cn::Deposit &deposit) const;

    /**
     * @return - returns unlock height from txInfo.blockheight + deposit.term
    **/
    std::string deposit_unlock_height(const cn::Deposit &deposit, const cn::WalletLegacyTransaction &txInfo) const;

    /**
     * @return - returns deposit height from txInfo.blockHeight
    **/
    std::string deposit_height(const cn::WalletLegacyTransaction &txInfo) const;

    /**
     * @return - returns deposit info string as row for multiple deposit outputs
    **/
    std::string get_deposit_info(const cn::Deposit &deposit, cn::DepositId did, const Currency &currency, const cn::WalletLegacyTransaction &txInfo) const;

    /**
     * @return - returns single deposit info string for client output
    **/
    std::string get_single_deposit_info(const cn::Deposit &deposit, cn::DepositId did, const Currency &currency, const cn::WalletLegacyTransaction &txInfo) const;

    /**
     * @return - returns false if user rejects deposit
     */
    bool confirm_deposit(uint64_t term, uint64_t amount, bool is_testnet, const Currency& currency, logging::LoggerRef logger);

    /**
     * @return - Displays all balances (main + deposits)
     */
    std::string balances(std::unique_ptr<cn::IWalletLegacy>& wallet, const Currency& currency);

    /**
     * @return - structed information
     */
    ListedDepositItem list_deposit_item(const WalletLegacyTransaction& txInfo, Deposit deposit, DepositId id, const Currency &currency);
    ListedTxItem tx_item(const WalletLegacyTransaction& txInfo, const Currency &currency);
  };
}
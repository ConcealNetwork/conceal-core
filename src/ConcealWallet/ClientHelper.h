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
    /**
     * @return - returns deposit term, should be multiple of 21900
    **/
    uint32_t deposit_term(const cn::Deposit &deposit) const;

    /**
     * @return - returns deposit amount as string
    **/
    std::string deposit_amount(const cn::Deposit &deposit, const Currency &currency) const;

    /**
     * @return - returns deposit interest based of amount and term as string
    **/
    std::string deposit_interest(const cn::Deposit &deposit, const Currency &currency) const;

    /**
     * @return - "Locked", "Unlocked" or "Spent" depending on state of deposit
    **/
    std::string deposit_status(const cn::Deposit &deposit) const;

    /**
     * @return - returns deposit creatingTransactionId 
    **/
    size_t deposit_creating_tx_id(const cn::Deposit &deposit) const;

    /**
     * @return - returns deposit spendingTransactionId
    **/
    size_t deposit_spending_tx_id(const cn::Deposit &deposit) const;

    /**
     * @return - returns unlock height from transaction blockheight + deposit term
    **/
    std::string deposit_unlock_height(const cn::Deposit &deposit, const cn::WalletLegacyTransaction &txInfo) const;

    /**
     * @return - returns deposit height
    **/
    std::string deposit_height(const cn::WalletLegacyTransaction &txInfo) const;

    /**
     * @return - returns deposit info string for client output
    **/
    std::string get_deposit_info(const cn::Deposit &deposit, cn::DepositId did, const Currency &currency, const cn::WalletLegacyTransaction &txInfo) const;

    /**
     * @return - returns full deposit info string for client output
    **/
    std::string get_full_deposit_info(const cn::Deposit &deposit, cn::DepositId did, const Currency &currency, const cn::WalletLegacyTransaction &txInfo) const;

    /**
     * @return - returns deposit info string for file output
    **/
    std::string list_deposit_item(const WalletLegacyTransaction& txInfo, const Deposit deposit, std::string listed_deposit, DepositId id, const Currency &currency);

    /**
     * @return - returns transaction info string for file output
    **/
    std::string list_tx_item(const WalletLegacyTransaction& txInfo, std::string listed_tx, const Currency &currency);

    /**
     * @return - returns false if user rejects deposit
     */
    bool confirm_deposit(uint64_t term, uint64_t amount, bool is_testnet, const Currency& currency, logging::LoggerRef logger);

    /**
     * @return - returns logging config for file and client output
     */
    JsonValue buildLoggerConfiguration(logging::Level level, const std::string& logfile);

    /**
     * @return - returns a formatted url address
     */
    bool parseUrlAddress(const std::string& url, std::string& address, uint16_t& port);

    std::error_code initAndLoadWallet(cn::IWalletLegacy& wallet, std::istream& walletFile, const std::string& password);

    std::string tryToOpenWalletOrLoadKeysOrThrow(logging::LoggerRef& logger, std::unique_ptr<cn::IWalletLegacy>& wallet, const std::string& walletFile, const std::string& password);

    void save_wallet(cn::IWalletLegacy& wallet, const std::string& walletFilename, logging::LoggerRef& logger);

    /**
     * @return - Displays all balances (main + deposits)
     */
    std::stringstream balances(std::unique_ptr<cn::IWalletLegacy>& wallet, const Currency& currency);

    /**
     * @return - wallet commands as string
     */
    std::string wallet_commands(bool is_extended);

    /**
     * @return - creates wallet address file if possible
     */
    bool write_addr_file(const std::string& addr_filename, const std::string& address);

    /**
     * @return - string ending with .address for wallet file
     */
    std::string prep_wallet_filename(const std::string& wallet_basename);

    /**
     * @return - checks if file already exists in current location
     */
    bool existing_file(std::string address_file, logging::LoggerRef logger);

    std::string get_commands_str(bool do_ext);

    /**
     * @return - structed information
     */
    ListedDepositItem list_deposit_item(const WalletLegacyTransaction& txInfo, Deposit deposit, DepositId id, const Currency &currency);
    ListedTxItem tx_item(const WalletLegacyTransaction& txInfo, const Currency &currency);
  };
}
// Copyright (c) 2018-2022 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <ctime>
#include <future>
#include <fstream>
#include <sstream>
#include <string>
#include <boost/filesystem.hpp>

#include "ClientHelper.h"

#include "CryptoNoteConfig.h"

#include "Common/JsonValue.h"
#include "Common/StringTools.h"
#include "CryptoNoteCore/TransactionExtra.h"
#include "Wallet/LegacyKeysImporter.h"
#include "WalletLegacy/WalletHelper.h"

#include <Logging/LoggerRef.h>

using namespace common;

namespace cn
{
  uint32_t client_helper::deposit_term(const cn::Deposit &deposit) const
  {
    return deposit.term;
  }

  std::string client_helper::deposit_amount(const cn::Deposit &deposit, const Currency &currency) const
  {
    return currency.formatAmount(deposit.amount);
  }

  std::string client_helper::deposit_interest(const cn::Deposit &deposit, const Currency &currency) const
  {
    return currency.formatAmount(deposit.interest);
  }

  std::string client_helper::deposit_status(const cn::Deposit &deposit) const
  {
    std::string status_str = "";

    if (deposit.locked)
      status_str = "Locked";
    else if (deposit.spendingTransactionId == cn::WALLET_LEGACY_INVALID_TRANSACTION_ID)
      status_str = "Unlocked";
    else
      status_str = "Spent";

    return status_str;
  }

  size_t client_helper::deposit_creating_tx_id(const cn::Deposit &deposit) const
  {
    return deposit.creatingTransactionId;
  }

  size_t client_helper::deposit_spending_tx_id(const cn::Deposit &deposit) const
  {
    return deposit.spendingTransactionId;
  }

  std::string client_helper::deposit_unlock_height(const cn::Deposit &deposit, const cn::WalletLegacyTransaction &txInfo) const
  {
    std::string unlock_str = "";

    if (txInfo.blockHeight > cn::parameters::CRYPTONOTE_MAX_BLOCK_NUMBER)
    {
      unlock_str = "Please wait.";
    }
    else
    {
      unlock_str = std::to_string(txInfo.blockHeight + deposit_term(deposit));
    }

    bool bad_unlock2 = unlock_str == "0";
    if (bad_unlock2)
    {
      unlock_str = "ERROR";
    }

    return unlock_str;
  }

  std::string client_helper::deposit_height(const cn::WalletLegacyTransaction &txInfo) const
  {
    std::string height_str = "";
    uint64_t deposit_height = txInfo.blockHeight;

    bool bad_unlock = deposit_height > cn::parameters::CRYPTONOTE_MAX_BLOCK_NUMBER;
    if (bad_unlock)
    {
      height_str = "Please wait.";
    }
    else
    {
      height_str = std::to_string(deposit_height);
    }

    bool bad_unlock2 = height_str == "0";
    if (bad_unlock2)
    {
      height_str = "ERROR";
    }

    return height_str;
  }

  std::string client_helper::get_deposit_info(const cn::Deposit &deposit, cn::DepositId did, const Currency &currency, const cn::WalletLegacyTransaction &txInfo) const
  {
    std::stringstream full_info;

    full_info << std::left <<
      std::setw(8)  << makeCenteredString(8, std::to_string(did)) << " | " <<
      std::setw(20) << makeCenteredString(20, deposit_amount(deposit, currency)) << " | " <<
      std::setw(20) << makeCenteredString(20, deposit_interest(deposit, currency)) << " | " <<
      std::setw(16) << makeCenteredString(16, deposit_unlock_height(deposit, txInfo)) << " | " <<
      std::setw(10) << makeCenteredString(10, deposit_status(deposit));
    
    std::string as_str = full_info.str();

    return as_str;
  }

  std::string client_helper::get_full_deposit_info(const cn::Deposit &deposit, cn::DepositId did, const Currency &currency, const cn::WalletLegacyTransaction &txInfo) const
  {
    std::stringstream full_info;

    full_info << "ID:            " << std::to_string(did) << "\n"
              << "Amount:        " << deposit_amount(deposit, currency) << "\n"
              << "Interest:      " << deposit_interest(deposit, currency) << "\n"
              << "Height:        " << deposit_height(txInfo) << "\n"
              << "Unlock Height: " << deposit_unlock_height(deposit, txInfo) << "\n"
              << "Status:        " << deposit_status(deposit) << "\n";

    std::string as_str = full_info.str();

    return as_str;
  }

  std::string client_helper::list_deposit_item(const WalletLegacyTransaction& txInfo, Deposit deposit, std::string listed_deposit, DepositId id, const Currency &currency)
  {
    std::string format_amount = currency.formatAmount(deposit.amount);
    std::string format_interest = currency.formatAmount(deposit.interest);
    std::string format_total = currency.formatAmount(deposit.amount + deposit.interest);

    std::stringstream ss_id(makeCenteredString(8, std::to_string(id)));
    std::stringstream ss_amount(makeCenteredString(20, format_amount));
    std::stringstream ss_interest(makeCenteredString(20, format_interest));
    std::stringstream ss_height(makeCenteredString(16, deposit_height(txInfo)));
    std::stringstream ss_unlockheight(makeCenteredString(16, deposit_unlock_height(deposit, txInfo)));
    std::stringstream ss_status(makeCenteredString(10, deposit_status(deposit)));

    ss_id >> std::setw(8);
    ss_amount >> std::setw(20);
    ss_interest >> std::setw(20);
    ss_height >> std::setw(16);
    ss_unlockheight >> std::setw(16);
    ss_status >> std::setw(10);

    listed_deposit = ss_id.str() + " | " + ss_amount.str() + " | " + ss_interest.str() + " | " + ss_height.str() + " | "
      + ss_unlockheight.str() + " | " + ss_status.str() + "\n";

    return listed_deposit;
  }

  std::string client_helper::list_tx_item(const WalletLegacyTransaction& txInfo, std::string listed_tx, const Currency &currency)
  {
    std::vector<uint8_t> extraVec = asBinaryArray(txInfo.extra);

    crypto::Hash paymentId;
    std::string paymentIdStr = (cn::getPaymentIdFromTxExtra(extraVec, paymentId) && paymentId != NULL_HASH ? podToHex(paymentId) : "");

    char timeString[32 + 1];
    time_t timestamp = static_cast<time_t>(txInfo.timestamp);
    struct tm time;
#ifdef _WIN32
    gmtime_s(&time, &timestamp);
#else
    gmtime_r(&timestamp, &time);
#endif

    if (!std::strftime(timeString, sizeof(timeString), "%c", &time))
    {
      throw std::runtime_error("time buffer is too small");
    }

    std::string format_amount = currency.formatAmount(txInfo.totalAmount);

    std::stringstream ss_time(makeCenteredString(32, timeString));
    std::stringstream ss_hash(makeCenteredString(64, podToHex(txInfo.hash)));
    std::stringstream ss_amount(makeCenteredString(20, currency.formatAmount(txInfo.totalAmount)));
    std::stringstream ss_fee(makeCenteredString(14, currency.formatAmount(txInfo.fee)));
    std::stringstream ss_blockheight(makeCenteredString(8, std::to_string(txInfo.blockHeight)));
    std::stringstream ss_unlocktime(makeCenteredString(12, std::to_string(txInfo.unlockTime)));

    ss_time >> std::setw(32);
    ss_hash >> std::setw(64);
    ss_amount >> std::setw(20);
    ss_fee >> std::setw(14);
    ss_blockheight >> std::setw(8);
    ss_unlocktime >> std::setw(12);

    listed_tx = ss_time.str() + " | " + ss_hash.str() + " | " + ss_amount.str() + " | " + ss_fee.str() + " | "
      + ss_blockheight.str() + " | " + ss_unlocktime.str() + "\n";

    return listed_tx;
  }

  bool client_helper::confirm_deposit(uint64_t term, uint64_t amount, bool is_testnet, const Currency& currency, logging::LoggerRef logger)
  {
    uint64_t interest = currency.calculateInterestV3(amount, term);
    uint64_t min_term = is_testnet ? parameters::TESTNET_DEPOSIT_MIN_TERM_V3 : parameters::DEPOSIT_MIN_TERM_V3;

    logger(logging::INFO) << "Confirm deposit details:\n"
      << "\tAmount: " << currency.formatAmount(amount) << "\n"
      << "\tMonths: " << term / min_term << "\n"
      << "\tInterest: " << currency.formatAmount(interest) << "\n";

    logger(logging::INFO) << "Is this correct? (Y/N): \n";

    char c;
    std::cin >> c;
    c = std::tolower(c);

    if (c == 'y')
    {
      return true;
    }
    else if (c == 'n')
    {
      return false;
    }
    else
    {
      logger(logging::ERROR) << "Bad input, please enter either Y or N.";
    }

    return false;
  }

  JsonValue client_helper::buildLoggerConfiguration(logging::Level level, const std::string& logfile)
  {
    using common::JsonValue;

    JsonValue loggerConfiguration(JsonValue::OBJECT);
    loggerConfiguration.insert("globalLevel", static_cast<int64_t>(level));

    JsonValue& cfgLoggers = loggerConfiguration.insert("loggers", JsonValue::ARRAY);

    JsonValue& consoleLogger = cfgLoggers.pushBack(JsonValue::OBJECT);
    consoleLogger.insert("type", "console");
    consoleLogger.insert("level", static_cast<int64_t>(logging::TRACE));
    consoleLogger.insert("pattern", "");

    JsonValue& fileLogger = cfgLoggers.pushBack(JsonValue::OBJECT);
    fileLogger.insert("type", "file");
    fileLogger.insert("filename", logfile);
    fileLogger.insert("level", static_cast<int64_t>(logging::TRACE));

    return loggerConfiguration;
  }

  bool client_helper::parseUrlAddress(const std::string& url, std::string& address, uint16_t& port)
  {
    auto pos = url.find("://");
    size_t addrStart = 0;

    if (pos != std::string::npos)
      addrStart = pos + 3;

    auto addrEnd = url.find(':', addrStart);

    if (addrEnd != std::string::npos)
    {
      auto portEnd = url.find('/', addrEnd);
      port = common::fromString<uint16_t>(url.substr(
        addrEnd + 1, portEnd == std::string::npos ? std::string::npos : portEnd - addrEnd - 1));
    }
    else
    {
      addrEnd = url.find('/');
      port = 80;
    }

    address = url.substr(addrStart, addrEnd - addrStart);

    return true;
  }

  std::error_code client_helper::initAndLoadWallet(cn::IWalletLegacy& wallet, std::istream& walletFile, const std::string& password)
  {
    WalletHelper::InitWalletResultObserver initObserver;
    std::future<std::error_code> f_initError = initObserver.initResult.get_future();

    WalletHelper::IWalletRemoveObserverGuard removeGuard(wallet, initObserver);
    wallet.initAndLoad(walletFile, password);
    auto initError = f_initError.get();

    return initError;
  }

  std::string client_helper::tryToOpenWalletOrLoadKeysOrThrow(logging::LoggerRef& logger, std::unique_ptr<cn::IWalletLegacy>& wallet, const std::string& walletFile, const std::string& password)
  {
    std::string keys_file, walletFileName;
    WalletHelper::prepareFileNames(walletFile, keys_file, walletFileName);

    boost::system::error_code ignore;

    bool keysExists = boost::filesystem::exists(keys_file, ignore);
    bool walletExists = boost::filesystem::exists(walletFileName, ignore);

    if (!walletExists && !keysExists && boost::filesystem::exists(walletFile, ignore))
    {
      boost::system::error_code renameEc;
      boost::filesystem::rename(walletFile, walletFileName, renameEc);

      if (renameEc)
        throw std::runtime_error("failed to rename file '" + walletFile + "' to '" + walletFileName + "': " + renameEc.message());

      walletExists = true;
    }

    if (walletExists)
    {
      logger(logging::INFO) << "Loading wallet...";

      std::ifstream walletFile;
      walletFile.open(walletFileName, std::ios_base::binary | std::ios_base::in);

      if (walletFile.fail())
        throw std::runtime_error("error opening wallet file '" + walletFileName + "'");

      auto initError = initAndLoadWallet(*wallet, walletFile, password);

      walletFile.close();

      if (initError)
      { //bad password, or legacy format
        if (keysExists)
        {
          std::stringstream ss;
          cn::importLegacyKeys(keys_file, password, ss);

          boost::filesystem::rename(keys_file, keys_file + ".back");
          boost::filesystem::rename(walletFileName, walletFileName + ".back");

          initError = initAndLoadWallet(*wallet, ss, password);
          if (initError)
            throw std::runtime_error("failed to load wallet: " + initError.message());

          logger(logging::INFO) << "Saving wallet...";
          save_wallet(*wallet, walletFileName, logger);
          logger(logging::INFO, logging::BRIGHT_GREEN) << "Saving successful";

          return walletFileName;
        }
        else
        { // no keys, wallet error loading
          throw std::runtime_error("can't load wallet file '" + walletFileName + "', check password");
        }
      }
      else
      { //new wallet ok
        return walletFileName;
      }
    }
    else if (keysExists)
    { //wallet not exists but keys presented
      std::stringstream ss;
      cn::importLegacyKeys(keys_file, password, ss);
      boost::filesystem::rename(keys_file, keys_file + ".back");

      WalletHelper::InitWalletResultObserver initObserver;
      std::future<std::error_code> f_initError = initObserver.initResult.get_future();

      WalletHelper::IWalletRemoveObserverGuard removeGuard(*wallet, initObserver);
      wallet->initAndLoad(ss, password);
      auto initError = f_initError.get();

      removeGuard.removeObserver();

      if (initError) {
        throw std::runtime_error("failed to load wallet: " + initError.message());
      }

      logger(logging::INFO) << "Saving wallet...";
      save_wallet(*wallet, walletFileName, logger);
      logger(logging::INFO, logging::BRIGHT_GREEN) << "Saved successful";

      return walletFileName;
    }
    else
    { //no wallet no keys
      throw std::runtime_error("wallet file '" + walletFileName + "' is not found");
    }
  }

  void client_helper::save_wallet(cn::IWalletLegacy& wallet, const std::string& walletFilename, logging::LoggerRef& logger)
  {
    logger(logging::INFO) << "Saving wallet...";

    try
    {
      cn::WalletHelper::storeWallet(wallet, walletFilename);
      logger(logging::INFO, logging::BRIGHT_GREEN) << "Saved successful";
    }
    catch (std::exception& e)
    {
      logger(logging::ERROR, logging::BRIGHT_RED) << "Failed to store wallet: " << e.what();
      throw std::runtime_error("error saving wallet file '" + walletFilename + "'");
    }
  }
  
  std::stringstream client_helper::balances(std::unique_ptr<cn::IWalletLegacy>& wallet, const Currency& currency)
  {
    std::stringstream balances;
    
    uint64_t full_balance = wallet->actualBalance() + wallet->pendingBalance() + wallet->actualDepositBalance() + wallet->pendingDepositBalance();
    std::string full_balance_text = "Total Balance: " + currency.formatAmount(full_balance) + "\n";

    uint64_t non_deposit_unlocked_balance = wallet->actualBalance();
    std::string non_deposit_unlocked_balance_text = "Available: " + currency.formatAmount(non_deposit_unlocked_balance) + "\n";

    uint64_t non_deposit_locked_balance = wallet->pendingBalance();
    std::string non_deposit_locked_balance_text = "Locked: " + currency.formatAmount(non_deposit_locked_balance) + "\n";

    uint64_t deposit_unlocked_balance = wallet->actualDepositBalance();
    std::string deposit_locked_balance_text = "Unlocked Balance: " + currency.formatAmount(deposit_unlocked_balance) + "\n";

    uint64_t deposit_locked_balance = wallet->pendingDepositBalance();
    std::string deposit_unlocked_balance_text = "Locked Deposits: " + currency.formatAmount(deposit_locked_balance) + "\n";

    balances << full_balance_text << non_deposit_unlocked_balance_text << non_deposit_locked_balance_text
      << deposit_unlocked_balance_text << deposit_locked_balance_text; 

    return balances;
  }
}

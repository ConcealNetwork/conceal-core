// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2022 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <ctime>
#include <fstream>
#include <future>
#include <iomanip>
#include <thread>
#include <set>
#include <sstream>
#include <regex>

#include <boost/format.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/predef.h>

#include "ConcealWallet.h"
#include "TransferCmd.h"
#include "Const.h"

#include "version.h"

#include "Common/Base58.h"
#include "Common/CsvWriter.h"
#include "Common/CommandLine.h"
#include "Common/SignalHandler.h"
#include "Common/StringTools.h"
#include "Common/PathTools.h"
#include "Common/Util.h"
#include "Common/DnsTools.h"

#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "CryptoNoteCore/CryptoNoteTools.h"

#include "NodeRpcProxy/NodeRpcProxy.h"

#include "Rpc/CoreRpcServerCommandsDefinitions.h"
#include "Rpc/HttpClient.h"

#include "Wallet/LegacyKeysImporter.h"
#include "Wallet/WalletRpcServer.h"
#include "Wallet/WalletUtils.h"

#include "WalletLegacy/WalletLegacy.h"
#include "WalletLegacy/WalletHelper.h"

#include <Logging/LoggerManager.h>
#include <Mnemonics/Mnemonics.h>

#if defined(WIN32)
#include <crtdbg.h>
#endif

using namespace cn;
using namespace logging;
using common::JsonValue;

namespace
{
  inline std::string interpret_rpc_response(bool ok, const std::string& status)
  {
    std::string err;
    if (ok)
    {
      if (status == CORE_RPC_STATUS_BUSY)
      {
        err = "daemon is busy. Please try later";
      }
      else if (status != CORE_RPC_STATUS_OK)
      {
        err = status;
      }
    }
    else
    {
      err = "possible lost connection to daemon";
    }
    return err;
  }
  
  void printListTransfersHeader(LoggerRef& logger)
  {
    std::string header = common::makeCenteredString(TIMESTAMP_MAX_WIDTH, "timestamp (UTC)") + "  ";
    header += common::makeCenteredString(HASH_MAX_WIDTH, "hash") + "  ";
    header += common::makeCenteredString(TOTAL_AMOUNT_MAX_WIDTH, "total amount") + "  ";
    header += common::makeCenteredString(FEE_MAX_WIDTH, "fee") + "  ";
    header += common::makeCenteredString(BLOCK_MAX_WIDTH, "block") + "  ";
    header += common::makeCenteredString(UNLOCK_TIME_MAX_WIDTH, "unlock time");

    logger(INFO) << header;
    logger(INFO) << std::string(header.size(), '-');
  }
  
  void printListDepositsHeader(LoggerRef& logger)
  {
    std::string header = common::makeCenteredString(8, "ID") + " | ";
    header += common::makeCenteredString(20, "Amount") + " | ";
    header += common::makeCenteredString(20, "Interest") + " | ";
    header += common::makeCenteredString(16, "Unlock Height") + " | ";
    header += common::makeCenteredString(10, "State");

    logger(INFO) << "\n" << header;
    logger(INFO) << std::string(header.size(), '=');
  }
  
  void printListTransfersItem(LoggerRef& logger, const WalletLegacyTransaction& txInfo, IWalletLegacy& wallet, const Currency& currency) {
    std::vector<uint8_t> extraVec = common::asBinaryArray(txInfo.extra);
    crypto::Hash paymentId;
    std::string paymentIdStr = (getPaymentIdFromTxExtra(extraVec, paymentId) && paymentId != NULL_HASH ? common::podToHex(paymentId) : "");

    char timeString[TIMESTAMP_MAX_WIDTH + 1];
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

    std::string rowColor = txInfo.totalAmount < 0 ? MAGENTA : GREEN;
    logger(INFO, rowColor)
      << std::left << std::setw(TIMESTAMP_MAX_WIDTH) << timeString
      << "  " << std::setw(HASH_MAX_WIDTH) << common::podToHex(txInfo.hash)
      << "  " << std::setw(TOTAL_AMOUNT_MAX_WIDTH) << currency.formatAmount(txInfo.totalAmount)
      << "  " << std::setw(FEE_MAX_WIDTH) << currency.formatAmount(txInfo.fee)
      << "  " << std::setw(BLOCK_MAX_WIDTH) << txInfo.blockHeight
      << "  " << std::setw(UNLOCK_TIME_MAX_WIDTH) << txInfo.unlockTime;

    if (!paymentIdStr.empty())
    {
      logger(INFO, rowColor) << "payment ID: " << paymentIdStr;
    }

    if (txInfo.totalAmount < 0 && txInfo.transferCount > 0)
    {
      logger(INFO, rowColor) << "transfers:";
      for (TransferId id = txInfo.firstTransferId; id < txInfo.firstTransferId + txInfo.transferCount; ++id)
      {
        WalletLegacyTransfer tr;
        wallet.getTransfer(id, tr);
        logger(INFO, rowColor) << tr.address << "  " << std::setw(TOTAL_AMOUNT_MAX_WIDTH) << currency.formatAmount(tr.amount);
      }
    }

    logger(INFO, rowColor) << " "; //just to make logger print one endline
  }
  
  bool processServerAliasResponse(const std::string& s, std::string& address)
  {
    try
    {// Courtesy of Monero Project
      // make sure the txt record has "oa1:ccx" and find it
      auto pos = s.find("oa1:ccx");
      if (pos == std::string::npos)
      {
        return false;
      }

      // search from there to find "recipient_address="
      pos = s.find("recipient_address=", pos);
      if (pos == std::string::npos)
      {
        return false;
      }
      pos += 18; // move past "recipient_address="

      // find the next semicolon
      auto pos2 = s.find(";", pos);
      if (pos2 != std::string::npos)
      {
        // length of address == 98, we can at least validate that much here
        if (pos2 - pos == 98)
        {
          address = s.substr(pos, 98);
        }
        else
        {
          return false;
        }
      }
    }
    catch (std::exception&)
    {
      return false;
    }
      
    return true;
  }
  
  bool splitUrlToHostAndUri(const std::string& aliasUrl, std::string& host, std::string& uri)
  {
    size_t protoBegin = aliasUrl.find("http://");
    if (protoBegin != 0 && protoBegin != std::string::npos)
    {
      return false;
    }

    size_t hostBegin = protoBegin == std::string::npos ? 0 : 7; //strlen("http://")
    size_t hostEnd = aliasUrl.find('/', hostBegin);

    if (hostEnd == std::string::npos)
    {
      uri = "/";
      host = aliasUrl.substr(hostBegin);
    }
    else
    {
      uri = aliasUrl.substr(hostEnd);
      host = aliasUrl.substr(hostBegin, hostEnd - hostBegin);
    }

    return true;
  }
  
  bool askAliasesTransfersConfirmation(const std::map<std::string, std::vector<WalletLegacyTransfer>>& aliases,
      const Currency& currency)
  {
    std::cout << "Would you like to send money to the following addresses?" << std::endl;

    for (const auto& kv: aliases)
    {
      for (const auto& transfer: kv.second)
      {
        std::cout << transfer.address << " " << std::setw(21) << currency.formatAmount(transfer.amount) << "  " << kv.first << std::endl;
      }
    }

    std::string answer;
    do
    {
      std::cout << "y/n: ";
      std::getline(std::cin, answer);
    } while (answer != "y" && answer != "Y" && answer != "n" && answer != "N");

    return answer == "y" || answer == "Y";
  }
  
  bool processServerFeeAddressResponse(const std::string& response, std::string& fee_address)
  {
    try
    {
      std::stringstream stream(response);
      JsonValue json;
      stream >> json;

      auto rootIt = json.getObject().find("fee_address");
      if (rootIt == json.getObject().end())
      {
        return false;
      }

      fee_address = rootIt->second.getString();
    }
    catch (std::exception&)
    {
      return false;
    }

    return true;
  }
} // end namespace

conceal_wallet::conceal_wallet(platform_system::Dispatcher& dispatcher, const cn::Currency& currency, logging::LoggerManager& log) :
  m_dispatcher(dispatcher),
  m_daemon_port(0),
  m_currency(currency),
  logManager(log),
  logger(log, "concealwallet"),
  m_refresh_progress_reporter(*this),
  m_initResultPromise(nullptr),
  m_walletSynchronized(false),
  m_is_view_wallet(false) {
  /* help cmd prints out from m_chelper (ClientHelper), don't forget to update with new commands */
  m_consoleHandler.setHandler("help", boost::bind(&conceal_wallet::help, this, boost::arg<1>()), "Show this help");
  m_consoleHandler.setHandler("ext_help", boost::bind(&conceal_wallet::extended_help, this, boost::arg<1>()), "Show this help");
  m_consoleHandler.setHandler("create_integrated", boost::bind(&conceal_wallet::create_integrated, this, boost::arg<1>()), "create_integrated <payment_id> - Create an integrated address with a payment ID");
  m_consoleHandler.setHandler("export_keys", boost::bind(&conceal_wallet::export_keys, this, boost::arg<1>()), "Show the secret keys of the current wallet");
  m_consoleHandler.setHandler("balance", boost::bind(&conceal_wallet::show_balance, this, boost::arg<1>()), "Show current wallet balance");
  m_consoleHandler.setHandler("sign_message", boost::bind(&conceal_wallet::sign_message, this, boost::arg<1>()), "Sign a message with your wallet keys");
  m_consoleHandler.setHandler("verify_signature", boost::bind(&conceal_wallet::verify_signature, this, boost::arg<1>()), "Verify a signed message");
  m_consoleHandler.setHandler("incoming_transfers", boost::bind(&conceal_wallet::show_incoming_transfers, this, boost::arg<1>()), "Show incoming transfers");
  m_consoleHandler.setHandler("list_transfers", boost::bind(&conceal_wallet::list_transfers, this, boost::arg<1>()), "list_transfers <height> - Show all known transfers from a certain (optional) block height");
  m_consoleHandler.setHandler("payments", boost::bind(&conceal_wallet::show_payments, this, boost::arg<1>()), "payments <payment_id_1> [<payment_id_2> ... <payment_id_N>] - Show payments <payment_id_1>, ... <payment_id_N>");
  m_consoleHandler.setHandler("get_tx_proof", boost::bind(&conceal_wallet::get_tx_proof, this, boost::arg<1>()), "Generate a signature to prove payment: <txid> <address> [<txkey>]");
  m_consoleHandler.setHandler("bc_height", boost::bind(&conceal_wallet::show_blockchain_height, this, boost::arg<1>()), "Show blockchain height");
  m_consoleHandler.setHandler("show_dust", boost::bind(&conceal_wallet::show_dust, this, boost::arg<1>()), "Show the number of unmixable dust outputs");
  m_consoleHandler.setHandler("outputs", boost::bind(&conceal_wallet::show_num_unlocked_outputs, this, boost::arg<1>()), "Show the number of unlocked outputs available for a transaction");
  m_consoleHandler.setHandler("optimize", boost::bind(&conceal_wallet::optimize_outputs, this, boost::arg<1>()), "Combine many available outputs into a few by sending a transaction to self");
  m_consoleHandler.setHandler("optimize_all", boost::bind(&conceal_wallet::optimize_all_outputs, this, boost::arg<1>()), "Optimize your wallet several times so you can send large transactions");  
  m_consoleHandler.setHandler("transfer", boost::bind(&conceal_wallet::transfer, this, boost::arg<1>()),
    "transfer <addr_1> <amount_1> [<addr_2> <amount_2> ... <addr_N> <amount_N>] [-p payment_id]"
    " - Transfer <amount_1>,... <amount_N> to <address_1>,... <address_N>, respectively. ");
  m_consoleHandler.setHandler("set_log", boost::bind(&conceal_wallet::set_log, this, boost::arg<1>()), "set_log <level> - Change current log level, <level> is a number 0-4");
  m_consoleHandler.setHandler("address", boost::bind(&conceal_wallet::print_address, this, boost::arg<1>()), "Show current wallet public address");
  m_consoleHandler.setHandler("save", boost::bind(&conceal_wallet::save, this, boost::arg<1>()), "Save wallet synchronized data");
  m_consoleHandler.setHandler("reset", boost::bind(&conceal_wallet::reset, this, boost::arg<1>()), "Discard cache data and start synchronizing from the start");
  m_consoleHandler.setHandler("exit", boost::bind(&conceal_wallet::exit, this, boost::arg<1>()), "Close wallet");  
  m_consoleHandler.setHandler("balance_proof", boost::bind(&conceal_wallet::get_reserve_proof, this, boost::arg<1>()), "all|<amount> [<message>] - Generate a signature proving that you own at least <amount>, optionally with a challenge string <message>. ");
  m_consoleHandler.setHandler("save_keys", boost::bind(&conceal_wallet::save_keys_to_file, this, boost::arg<1>()), "Saves wallet private keys to \"<wallet_name>_conceal_backup.txt\"");
  m_consoleHandler.setHandler("list_deposits", boost::bind(&conceal_wallet::list_deposits, this, boost::arg<1>()), "Show all known deposits from this wallet");
  m_consoleHandler.setHandler("deposit", boost::bind(&conceal_wallet::deposit, this, boost::arg<1>()), "deposit <months> <amount> - Create a deposit");
  m_consoleHandler.setHandler("withdraw", boost::bind(&conceal_wallet::withdraw, this, boost::arg<1>()), "withdraw <id> - Withdraw a deposit");
  m_consoleHandler.setHandler("deposit_info", boost::bind(&conceal_wallet::deposit_info, this, boost::arg<1>()), "deposit_info <id> - Get infomation for deposit <id>");
  m_consoleHandler.setHandler("save_txs_to_file", boost::bind(&conceal_wallet::save_all_txs_to_file, this, boost::arg<1>()), "save_txs_to_file - Saves all known transactions to <wallet_name>_conceal_transactions.txt");
  m_consoleHandler.setHandler("check_address", boost::bind(&conceal_wallet::check_address, this, boost::arg<1>()), "check_address <address> - Checks to see if given wallet is valid.");
  m_consoleHandler.setHandler("show_view_tracking", boost::bind(&conceal_wallet::show_view_key, this, boost::arg<1>()), "Show view wallet tracking keys.");
}

//----------------------------------------------------------------------------------------------------

bool conceal_wallet::init(const boost::program_options::variables_map& vm)
{
  handle_command_line(vm);

  if (m_daemon_host.empty())
  {
    m_daemon_host = "localhost";
  }

  if (!m_daemon_address.empty()) 
  {
    if (!m_chelper.parseUrlAddress(m_daemon_address, m_daemon_host, m_daemon_port)) 
    {
      fail_msg_writer() << "failed to parse daemon address: " << m_daemon_address;
      return false;
    }
    m_remote_node_address = getFeeAddress();
    logger(INFO, BRIGHT_WHITE) << "Connected to remote node: " << m_daemon_host;
    if (!m_remote_node_address.empty()) 
    {
      logger(INFO, BRIGHT_WHITE) << "Fee address: " << m_remote_node_address;
    }    
  } 
  else 
  {
    if (!m_daemon_host.empty()) {
      m_remote_node_address = getFeeAddress();
    }
    m_daemon_address = std::string("http://") + m_daemon_host + ":" + std::to_string(m_daemon_port);
    logger(INFO, BRIGHT_WHITE) << "Connected to remote node: " << m_daemon_host;
    if (!m_remote_node_address.empty()) 
    {
      logger(INFO, BRIGHT_WHITE) << "Fee address: " << m_remote_node_address;
    }   
  }

  if (m_generate_new_arg.empty() && m_wallet_file_arg.empty()) {
    std::cout << "  " << ENDL
    << "  " << ENDL
    << "      @@@@@@   .@@@@@@&   .@@@   ,@@,   &@@@@@  @@@@@@@@    &@@@*    @@@        " << ENDL
    << "    &@@@@@@@  @@@@@@@@@@  .@@@@  ,@@,  @@@@@@@  @@@@@@@@    @@@@@    @@@        " << ENDL
    << "    @@@       @@@    @@@* .@@@@@ ,@@, &@@*      @@@        ,@@#@@.   @@@        " << ENDL
    << "    @@@       @@@    (@@& .@@@@@,,@@, @@@       @@@...     @@@ @@@   @@@        " << ENDL
    << "    @@@      .@@&    /@@& .@@*@@@.@@, @@@       @@@@@@     @@@ @@@   @@@        " << ENDL
    << "    @@@       @@@    #@@  .@@( @@@@@, @@@       @@@       @@@/ #@@&  @@@        " << ENDL
    << "    @@@       @@@    @@@, .@@( &@@@@, &@@*      @@@       @@@@@@@@@  @@@        " << ENDL
    << "    %@@@@@@@  @@@@@@@@@@  .@@(  @@@@,  @@@@@@@  @@@@@@@@ .@@@   @@@. @@@@@@@@#  " << ENDL
    << "      @@@@@@    @@@@@@(   .@@(   @@@,    @@@@@  @@@@@@@@ @@@    (@@@ @@@@@@@@#  " << ENDL
    << "  " << ENDL
    << "  " << ENDL;

    std::cout << "How you would like to proceed?\n\n\t[O]pen an existing wallet\n\t[G]enerate a new wallet file\n\t[I]mport wallet from keys/seed\n\t[E]xit.\n\n";
    std::string user_input_str;
    
    do
    {
      std::getline(std::cin, user_input_str);
      boost::algorithm::to_lower(user_input_str);
      bool good_input = user_input_str == "open" || user_input_str == "generate" ||
          user_input_str == "import" || user_input_str == "exit" ||
          user_input_str == "o" || user_input_str == "g" || user_input_str == "i" ||
          user_input_str == "e";

      if (!good_input)
      {
        std::cout << "Bad input: " << user_input_str << std::endl; 
      }
      else
      {
        break;
      }
    } while (true);

    if (user_input_str == "exit" || user_input_str == "e")
    {
      return false;
    }

    this->m_node.reset(new NodeRpcProxy(m_daemon_host, m_daemon_port));

    std::promise<std::error_code> errorPromise;
    std::future<std::error_code> f_error = errorPromise.get_future();
    auto callback = [&errorPromise](std::error_code e) {errorPromise.set_value(e); };

    m_node->addObserver(static_cast<INodeRpcProxyObserver*>(this));
    m_node->init(callback);
    auto error = f_error.get();
    if (error) {
      logger(ERROR) << "failed to init NodeRPCProxy: " << error.message();
      return false;
    }

    std::string homeEnvVar;
    if (BOOST_OS_WINDOWS)
      homeEnvVar = "USERPROFILE";
    else
      homeEnvVar = "HOME";

    std::string wallet_name;
    std::string password_str;
    tools::PasswordContainer pwd_container;
    //m_is_view_wallet = false;

    if (user_input_str == "import" || user_input_str == "i")
    {
      do {
        std::cout << "Wallet file name: ";
        std::getline(std::cin, wallet_name);
        wallet_name = std::regex_replace(wallet_name, std::regex("~"), getenv(homeEnvVar.c_str()));
        boost::algorithm::trim(wallet_name);
      } while (wallet_name.empty());

      if (m_chelper.existing_file(wallet_name, logger))
      {
        return false;
      }

      if (password_str.empty()) {
        if (pwd_container.read_password()) {
          password_str = pwd_container.password();
        }
      }

      std::cout << "What keys would you like to import?\n\t[P]rivate Keys\n\t[M]nemonic Seed\n\t[V]iew Tracking Key" << std::endl;
      std::cout << "\nWallets imported via [V]iew Tracking Keys have limited functionality" << std::endl;
      std::string user_import_str;

      do
      {
        std::getline(std::cin, user_import_str);
        boost::algorithm::to_lower(user_import_str);
        bool good_input = user_import_str == "private" || user_import_str == "private keys" ||
            user_import_str == "view" || user_import_str == "view keys" ||
            user_import_str == "mnemonic seed" || user_import_str == "mnemonic" || user_import_str == "exit" ||
            user_import_str == "p" || user_import_str == "m" || user_import_str == "v" || user_import_str == "e";

        if (!good_input)
        {
          std::cout << "Bad input: " << user_import_str << std::endl;
        }
        else
        {
          break;
        }
      } while (true);

      if (user_import_str == "exit" || user_import_str == "e")
      {
        return false;
      }

      if (user_import_str == "private" || user_import_str == "private keys" || user_import_str == "p")
      {
        std::string private_spend_key_string;
        std::string private_view_key_string;
        
        crypto::SecretKey private_spend_key;
        crypto::SecretKey private_view_key;
        
        do {
          std::cout << "Private Spend Key: ";
          std::getline(std::cin, private_spend_key_string);
          boost::algorithm::trim(private_spend_key_string);
        } while (private_spend_key_string.empty());

        do {
          std::cout << "Private View Key: ";
          std::getline(std::cin, private_view_key_string);
          boost::algorithm::trim(private_view_key_string);
        } while (private_view_key_string.empty());

        crypto::Hash private_spend_key_hash;
        crypto::Hash private_view_key_hash;
        size_t size;

        if (!common::fromHex(private_spend_key_string, &private_spend_key_hash, sizeof(private_spend_key_hash), size) || size != sizeof(private_spend_key_hash))
        {
          return false;
        }
        if (!common::fromHex(private_view_key_string, &private_view_key_hash, sizeof(private_view_key_hash), size) || size != sizeof(private_spend_key_hash))
        {
          return false;
        }

        private_spend_key = *(struct crypto::SecretKey *) &private_spend_key_hash;
        private_view_key = *(struct crypto::SecretKey *) &private_view_key_hash;

        if (!new_imported_wallet(private_spend_key, private_view_key, wallet_name, pwd_container.password())) {
          logger(ERROR, BRIGHT_RED) << "account creation failed";
          return false;
        }

        if (!m_chelper.write_addr_file(wallet_name, m_wallet->getAddress())) {
          logger(WARNING, BRIGHT_RED) << "Couldn't write wallet address file: " + wallet_name;
        }

        return true;
      }
      else if (user_import_str == "view" || user_import_str == "view keys" || user_import_str == "v")
      {
        std::string view_key_str;

        do
        {
          std::cout << "View Key: ";
          std::getline(std::cin, view_key_str);
          boost::algorithm::trim(view_key_str);
          boost::algorithm::to_lower(view_key_str);
        } while (view_key_str.empty());

        if (view_key_str.length() != 256)
        {
          logger(ERROR, BRIGHT_RED) << "Wrong view key.";
          return false;
        }
        
        AccountKeys keys;
        
        std::string public_spend_key_string = view_key_str.substr(0, 64);
        std::string public_view_key_string = view_key_str.substr(64, 64);
        std::string private_spend_key_string = view_key_str.substr(128, 64);
        std::string private_view_key_string = view_key_str.substr(192, 64);

        crypto::Hash public_spend_key_hash;
        crypto::Hash public_view_key_hash;
        crypto::Hash private_spend_key_hash;
        crypto::Hash private_view_key_hash;

        size_t size;
        if (!common::fromHex(public_spend_key_string, &public_spend_key_hash, sizeof(public_spend_key_hash), size)
            || size != sizeof(public_spend_key_hash))
        {
          return false;
        }
        if (!common::fromHex(public_view_key_string, &public_view_key_hash, sizeof(public_view_key_hash), size)
            || size != sizeof(public_view_key_hash))
        {
          return false;
        }
        if (!common::fromHex(private_spend_key_string, &private_spend_key_hash, sizeof(private_spend_key_hash), size)
            || size != sizeof(private_spend_key_hash))
        {
          return false;
        }
        if (!common::fromHex(private_view_key_string, &private_view_key_hash, sizeof(private_view_key_hash), size)
            || size != sizeof(private_view_key_hash))
        {
          return false;
        }

        crypto::PublicKey public_spend_key = *(struct crypto::PublicKey*)&public_spend_key_hash;
        crypto::PublicKey public_view_key = *(struct crypto::PublicKey*)&public_view_key_hash;
        crypto::SecretKey private_spend_key = *(struct crypto::SecretKey*)&private_spend_key_hash;
        crypto::SecretKey private_view_key = *(struct crypto::SecretKey*)&private_view_key_hash;

        keys.address.spendPublicKey = public_spend_key;
        keys.address.viewPublicKey = public_view_key;
        keys.spendSecretKey = private_spend_key;
        keys.viewSecretKey = private_view_key;

        if (!new_view_wallet(keys, wallet_name, pwd_container.password()))
        {
          logger(ERROR, BRIGHT_RED) << "account creation failed";
          return false;
        }

        if (!m_chelper.write_addr_file(wallet_name, m_wallet->getAddress())) {
          logger(WARNING, BRIGHT_RED) << "Couldn't write wallet address file: " + wallet_name;
        }
      }
      else // else we assume seed import
      {
        std::string mnemonic_seed;

        do
        {
          std::cout << "Mnemonics Phrase (25 words): ";
          std::getline(std::cin, mnemonic_seed);
          boost::algorithm::trim(mnemonic_seed);
          boost::algorithm::to_lower(mnemonic_seed);
        } while (mnemonic_seed.empty());
      
        crypto::SecretKey p_spend = mnemonics::mnemonicToPrivateKey(mnemonic_seed);
        crypto::SecretKey p_view;
        crypto::PublicKey dummy_var;

        AccountBase::generateViewFromSpend(p_spend, p_view, dummy_var);

        if (!new_imported_wallet(p_spend, p_view, wallet_name, pwd_container.password())) {
          logger(ERROR, BRIGHT_RED) << "account creation failed";
          return false;
        }

        if (!m_chelper.write_addr_file(wallet_name, m_wallet->getAddress())) {
          logger(WARNING, BRIGHT_RED) << "Couldn't write wallet address file: " + wallet_name;
        }

        return true;
      }
    }
    else if (user_input_str == "generate" || user_input_str == "g")
    {
      do {
        std::cout << "Wallet file name: ";
        std::getline(std::cin, wallet_name);
        wallet_name = std::regex_replace(wallet_name, std::regex("~"), getenv(homeEnvVar.c_str()));
        boost::algorithm::trim(wallet_name);
      } while (wallet_name.empty());

      if (m_chelper.existing_file(wallet_name, logger))
      {
        return false;
      }

      if (password_str.empty()) {
        if (pwd_container.read_password()) {
          password_str = pwd_container.password();
        }
      }

      if (!new_wallet(wallet_name, pwd_container.password())) {
        logger(ERROR, BRIGHT_RED) << "account creation failed";
        return false;
      }

      if (!m_chelper.write_addr_file(wallet_name, m_wallet->getAddress())) {
        logger(WARNING, BRIGHT_RED) << "Couldn't write wallet address file: " + wallet_name;
      }

      return true;
    }
    else // else we assume we're opening a wallet
    {
      do {
        std::cout << "Wallet file name: ";
        std::getline(std::cin, wallet_name);
        wallet_name = std::regex_replace(wallet_name, std::regex("~"), getenv(homeEnvVar.c_str()));
        boost::algorithm::trim(wallet_name);
      } while (wallet_name.empty());

      if (password_str.empty()) {
        if (pwd_container.read_password()) {
          password_str = pwd_container.password();
        }
      }

      m_wallet_file_arg = wallet_name;

      m_wallet.reset(new WalletLegacy(m_currency, *m_node, logManager, m_testnet));

      try {
        m_wallet_file = m_chelper.tryToOpenWalletOrLoadKeysOrThrow(logger, m_wallet, m_wallet_file_arg, pwd_container.password());
      } catch (const std::exception& e) {
        fail_msg_writer() << "failed to load wallet: " << e.what();
        return false;
      }

      m_wallet->addObserver(this);
      m_node->addObserver(static_cast<INodeObserver*>(this));

      std::string tmp_str = m_wallet_file;
      m_frmt_wallet_file = tmp_str.erase(tmp_str.size() - 7);

      AccountKeys keys;
      m_wallet->getAccountKeys(keys);
      
      if (keys.spendSecretKey == NULL_SECRET_KEY)
      {
        //m_is_view_wallet = true;
        logger(INFO) << "Loading view wallet.";
      }

      logger(INFO, BRIGHT_WHITE) << "Opened wallet: " << m_wallet->getAddress();

      success_msg_writer() <<
        "**********************************************************************\n" <<
        "Use \"help\" command to see the list of available commands.\n" <<
        "**********************************************************************";

      return true;
    }
  }

  return true;
}

//----------------------------------------------------------------------------------------------------

bool conceal_wallet::new_wallet(const std::string &wallet_file, const std::string& password)
{
  m_wallet_file = wallet_file;

  m_wallet.reset(new WalletLegacy(m_currency, *m_node, logManager, m_testnet));
  m_node->addObserver(static_cast<INodeObserver*>(this));
  m_wallet->addObserver(this);

  try
  {
    m_initResultPromise.reset(new std::promise<std::error_code>());
    std::future<std::error_code> f_initError = m_initResultPromise->get_future();
    m_wallet->initAndGenerate(password);
    auto initError = f_initError.get();
    m_initResultPromise.reset(nullptr);
    if (initError)
    {
      fail_msg_writer() << "failed to generate new wallet: " << initError.message();
      return false;
    }

    m_chelper.save_wallet(*m_wallet, m_wallet_file, logger);

    AccountKeys keys;
    m_wallet->getAccountKeys(keys);

    std::string secretKeysData = std::string(reinterpret_cast<char*>(&keys.spendSecretKey), sizeof(keys.spendSecretKey)) + std::string(reinterpret_cast<char*>(&keys.viewSecretKey), sizeof(keys.viewSecretKey));
    std::string guiKeys = tools::base_58::encode_addr(cn::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX, secretKeysData);

    logger(INFO, BRIGHT_GREEN) << "ConcealWallet is an open-source, client-side, free wallet which allow you to send and receive CCX instantly on the blockchain. You are  in control of your funds & your keys. When you generate a new wallet, login, send, receive or deposit $CCX everything happens locally. Your seed is never transmitted, received or stored. That's why its imperative to write, print or save your seed somewhere safe. The backup of keys is your responsibility. If you lose your seed, your account can not be recovered. The Conceal Team doesn't take any responsibility for lost funds due to nonexistent/missing/lost private keys." << std::endl << std::endl;

    std::cout << "Wallet Address: " << m_wallet->getAddress() << std::endl;
    std::cout << "Private spend key: " << common::podToHex(keys.spendSecretKey) << std::endl;
    std::cout << "Private view key: " <<  common::podToHex(keys.viewSecretKey) << std::endl;
    std::cout << "Mnemonic Seed: " << mnemonics::privateKeyToMnemonic(keys.spendSecretKey) << std::endl;
  }
  catch (const std::exception& e)
  {
    fail_msg_writer() << "failed to generate new wallet: " << e.what();
    return false;
  }

  success_msg_writer() <<
    "**********************************************************************\n" <<
    "Your wallet has been generated.\n" <<
    "Use \"help\" command to see the list of available commands.\n" <<
    "Always use \"exit\" command when closing Conceal Wallet to save\n" <<
    "current session's state. Otherwise, you will possibly need to synchronize \n" <<
    "your wallet again. Your wallet key is NOT under risk anyway.\n" <<
    "**********************************************************************";
  return true;
}

//----------------------------------------------------------------------------------------------------

bool conceal_wallet::new_imported_wallet(crypto::SecretKey &secret_key, crypto::SecretKey &view_key, const std::string &wallet_file, const std::string& password)
{
  m_wallet_file = wallet_file;

  m_wallet.reset(new WalletLegacy(m_currency, *m_node.get(), logManager, m_testnet));
  m_node->addObserver(static_cast<INodeObserver*>(this));
  m_wallet->addObserver(this);
  
  try
  {
    m_initResultPromise.reset(new std::promise<std::error_code>());
    std::future<std::error_code> f_initError = m_initResultPromise->get_future();

    AccountKeys wallet_keys;
    wallet_keys.spendSecretKey = secret_key;
    wallet_keys.viewSecretKey = view_key;
    crypto::secret_key_to_public_key(wallet_keys.spendSecretKey, wallet_keys.address.spendPublicKey);
    crypto::secret_key_to_public_key(wallet_keys.viewSecretKey, wallet_keys.address.viewPublicKey);

    m_wallet->initWithKeys(wallet_keys, password);
    auto initError = f_initError.get();
    m_initResultPromise.reset(nullptr);
    if (initError)
    {
      fail_msg_writer() << "failed to generate new wallet: " << initError.message();
      return false;
    }

    m_chelper.save_wallet(*m_wallet, m_wallet_file, logger);

    AccountKeys keys;
    m_wallet->getAccountKeys(keys);

    logger(INFO, BRIGHT_WHITE) <<
      "Imported wallet: " << m_wallet->getAddress() << std::endl;

    if (keys.spendSecretKey == boost::value_initialized<crypto::SecretKey>())
    {
      m_is_view_wallet = true;
    }
  }
  catch (const std::exception& e)
  {
    fail_msg_writer() << "failed to import wallet: " << e.what();
    return false;
  }
  
  success_msg_writer() <<
    "**********************************************************************\n" <<
    "Your wallet has been imported.\n" <<
    "Use \"help\" command to see the list of available commands.\n" <<
    "Always use \"exit\" command when closing Conceal Wallet to save\n" <<
    "current session's state. Otherwise, you will possibly need to synchronize \n" <<
    "your wallet again. Your wallet key is NOT under risk anyway.\n" <<
    "**********************************************************************";
  return true;
}

//----------------------------------------------------------------------------------------------------
bool conceal_wallet::new_view_wallet(AccountKeys &view_key, const std::string &wallet_file, const std::string& password)
{
  m_wallet_file = wallet_file;

  m_wallet.reset(new WalletLegacy(m_currency, *m_node.get(), logManager, m_testnet));
  m_node->addObserver(static_cast<INodeObserver*>(this));
  m_wallet->addObserver(this);

  try
  {
    m_initResultPromise.reset(new std::promise<std::error_code>());
    std::future<std::error_code> f_initError = m_initResultPromise->get_future();

    m_wallet->initWithKeys(view_key, password);

    auto initError = f_initError.get();
    m_initResultPromise.reset(nullptr);
    if (initError)
    {
      fail_msg_writer() << "failed to generate new wallet: " << initError.message();
      return false;
    }

    m_chelper.save_wallet(*m_wallet, m_wallet_file, logger);

    AccountKeys keys;
    m_wallet->getAccountKeys(keys);

    logger(INFO, BRIGHT_WHITE) << "Imported wallet: " << m_wallet->getAddress() << std::endl;

    m_is_view_wallet = true;
  }
  catch (const std::exception& e)
  {
    fail_msg_writer() << "failed to import wallet: " << e.what();
    return false;
  }

  success_msg_writer() <<
      "**********************************************************************\n" <<
      "Your tracking wallet has been imported. It doesn't allow spending funds.\n" <<
      "It allows to view incoming transactions but not outgoing ones. \n" <<
      "If there were spendings total balance will be inaccurate. \n" <<
      "Use \"help\" command to see the list of available commands.\n" <<
      "Always use \"exit\" command when closing concealwallet to save\n" <<
      "current session's state. Otherwise, you will possibly need to synchronize \n" <<
      "your wallet again. Your wallet key is NOT under risk anyway.\n" <<
      "**********************************************************************";
  return true;
}
//----------------------------------------------------------------------------------------------------

bool conceal_wallet::deinit()
{
  m_wallet->removeObserver(this);
  m_node->removeObserver(static_cast<INodeObserver*>(this));
  m_node->removeObserver(static_cast<INodeRpcProxyObserver*>(this));
  if (!m_wallet.get()) { return true; }
  return close_wallet();
}

//----------------------------------------------------------------------------------------------------

void conceal_wallet::handle_command_line(const boost::program_options::variables_map& vm)
{
  m_testnet = vm[arg_testnet.name].as<bool>();
  m_wallet_file_arg = command_line::get_arg(vm, arg_wallet_file);
  m_generate_new_arg = command_line::get_arg(vm, arg_generate_new_wallet);
  m_daemon_address = command_line::get_arg(vm, arg_daemon_address);
  m_daemon_host = command_line::get_arg(vm, arg_daemon_host);
  m_daemon_port = command_line::get_arg(vm, arg_daemon_port);
  if (m_daemon_port == 0) { m_daemon_port = RPC_DEFAULT_PORT; }
  if (m_testnet && vm[arg_daemon_port.name].defaulted()) { m_daemon_port = TESTNET_RPC_DEFAULT_PORT; }
}

//----------------------------------------------------------------------------------------------------

bool conceal_wallet::process_command(const std::vector<std::string> &args)
{
  return m_consoleHandler.runCommand(args);
}

//----------------------------------------------------------------------------------------------------

bool conceal_wallet::run()
{
  std::unique_lock<std::mutex> lock(m_walletSynchronizedMutex);
  while (!m_walletSynchronized) { m_walletSynchronizedCV.wait(lock); }

  std::cout << std::endl;
  // print first 10 chars of address
  std::string addr_start = m_wallet->getAddress().substr(0, 10);
  m_consoleHandler.start(false, "[" + addr_start + "]: ", common::console::Color::BrightYellow);
  return true;
}

//----------------------------------------------------------------------------------------------------

void conceal_wallet::stop()
{
  m_consoleHandler.requestStop();
}

//----------------------------------------------------------------------------------------------------

void conceal_wallet::printConnectionError() const
{
  logger(ERROR, BRIGHT_RED) << "wallet failed to connect to daemon (" << m_daemon_address << ").";
}

//----------------------------------------------------------------------------------------------------

std::string conceal_wallet::getFeeAddress()
{
  /* This extracts the fee address from the remote node */
  HttpClient httpClient(m_dispatcher, m_daemon_host, m_daemon_port);

  HttpRequest req;
  HttpResponse res;

  req.setUrl("/feeaddress");
  try
  {
	  httpClient.request(req, res);
  }
  catch (const std::exception& e) {
	  fail_msg_writer() << "Error connecting to the remote node: " << e.what();
  }

  if (res.getStatus() != HttpResponse::STATUS_200)
  {
	  fail_msg_writer() << "Remote node returned code " + std::to_string(res.getStatus());
  }

  std::string address;
  if (!processServerFeeAddressResponse(res.getBody(), address))
  {
	  fail_msg_writer() << "Failed to parse remote node response";
  }

  return address;
}

//----------------------------------------------------------------------------------------------------

std::string conceal_wallet::resolveAlias(const std::string& aliasUrl)
{
  std::string host;
  std::string uri;
  std::vector<std::string>records;
  std::string address;

  if (!splitUrlToHostAndUri(aliasUrl, host, uri))
  {
    throw std::runtime_error("Failed to split URL to Host and URI");
  }

  if (!common::fetch_dns_txt(aliasUrl, records))
  {
    throw std::runtime_error("Failed to lookup DNS record");
  }

  for (const auto& record : records)
  {
    if (processServerAliasResponse(record, address))
    {
      return address;
    }
  }

  throw std::runtime_error("Failed to parse server response");
}

//----------------------------------------------------------------------------------------------------

void conceal_wallet::synchronizationCompleted(std::error_code result)
{
  std::unique_lock<std::mutex> lock(m_walletSynchronizedMutex);
  m_walletSynchronized = true;
  m_walletSynchronizedCV.notify_one();
}

//----------------------------------------------------------------------------------------------------

void conceal_wallet::synchronizationProgressUpdated(uint32_t current, uint32_t total)
{
  std::unique_lock<std::mutex> lock(m_walletSynchronizedMutex);
  if (!m_walletSynchronized) { m_refresh_progress_reporter.update(current, false); }
}

//----------------------------------------------------------------------------------------------------

void conceal_wallet::initCompleted(std::error_code result)
{
  if (m_initResultPromise.get() != nullptr)
  {
    m_initResultPromise->set_value(result);
  }
}

//----------------------------------------------------------------------------------------------------

void conceal_wallet::connectionStatusUpdated(bool connected)
{
  if (connected)
  {
    logger(INFO, GREEN) << "Wallet connected to daemon.";
  }
  else
  {
    printConnectionError();
  }
}

//----------------------------------------------------------------------------------------------------

void conceal_wallet::externalTransactionCreated(cn::TransactionId transactionId)
{
  WalletLegacyTransaction txInfo;
  m_wallet->getTransaction(transactionId, txInfo);

  std::stringstream logPrefix;
  if (txInfo.blockHeight == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT)
  {
    logPrefix << "Unconfirmed";
  }
  else
  {
    logPrefix << "Height " << txInfo.blockHeight << ',';
  }

  if (txInfo.totalAmount >= 0)
  {
    logger(INFO, GREEN) <<
      logPrefix.str() << " transaction " << common::podToHex(txInfo.hash) <<
      ", received " << m_currency.formatAmount(txInfo.totalAmount);
  }
  else
  {
    logger(INFO, MAGENTA) <<
      logPrefix.str() << " transaction " << common::podToHex(txInfo.hash) <<
      ", spent " << m_currency.formatAmount(static_cast<uint64_t>(-txInfo.totalAmount));
  }

  if (txInfo.blockHeight == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT)
  {
    m_refresh_progress_reporter.update(m_node->getLastLocalBlockHeight(), true);
  }
  else
  {
    m_refresh_progress_reporter.update(txInfo.blockHeight, true);
  }
}

//----------------------------------------------------------------------------------------------------

bool conceal_wallet::close_wallet()
{
  m_chelper.save_wallet(*m_wallet, m_wallet_file, logger);
  logger(logging::INFO, logging::BRIGHT_GREEN) << "Closing wallet...";
  m_wallet->removeObserver(this);
  m_wallet->shutdown();
  return true;
}

//----------------------------------------------------------------------------------------------------

bool conceal_wallet::help(const std::vector<std::string> &args)
{
  if (!args.empty())
  {
    logger(ERROR) << "Usage: \"help\"";
    return true;
  }

  try
  {
    logger(INFO) << m_chelper.get_commands_str(false);
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"help\" command: " << e.what();
  }
  
  return true;
}

bool conceal_wallet::extended_help(const std::vector<std::string> &args)
{
  if (!args.empty())
  {
    logger(ERROR) << "Usage: \"ext_help\"";
    return true;
  }

  try
  {
    logger(INFO) << m_chelper.get_commands_str(true);
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"ext_help\" command: " << e.what();
  }
  
  return true;
}

bool conceal_wallet::exit(const std::vector<std::string> &args)
{
  if (!args.empty())
  {
    logger(ERROR) << "Usage: \"exit\"";
    return true;
  }

  try
  {
    m_consoleHandler.requestStop();
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"exit\" command: " << e.what();
  }

  return true;
}

bool conceal_wallet::show_dust(const std::vector<std::string>& args)
{
  if (!args.empty())
  {
    logger(ERROR) << "Usage: \"show_dust\"";
    return true;
  }

  /* This function shows the number of outputs in the wallet
  that are below the dust threshold */
  try
  {
    logger(INFO, BRIGHT_WHITE) << "Dust outputs: " << m_wallet->dustBalance();
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"show_dust\" command: " << e.what();
  }

	return true;
}

bool conceal_wallet::set_log(const std::vector<std::string> &args)
{
  if (args.size() != 1) {
    logger(ERROR) << "use: set_log <log_level_number_0-4>";
    return true;
  }

  try
  {
    uint16_t l = 0;
    if (!common::fromString(args[0], l))
    {
      logger(ERROR) << "wrong number format, use: set_log <log_level_number_0-4>";
      return true;
     }

    if (l > logging::TRACE)
    {
      logger(ERROR) << "wrong number range, use: set_log <log_level_number_0-4>";
      return true;
    }

    logManager.setMaxLevel(static_cast<logging::Level>(l));
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"set_log\" command: " << e.what();
  }

  return true;
}

bool conceal_wallet::save(const std::vector<std::string> &args)
{
  if (!args.empty())
  {
    logger(ERROR) << "Usage: \"save\"";
    return true;
  }

  try
  {
    m_chelper.save_wallet(*m_wallet, m_wallet_file, logger);
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"save\" command: " << e.what();
  }

  return true;
}

bool conceal_wallet::reset(const std::vector<std::string> &args)
{
  if (!args.empty())
  {
    logger(ERROR) << "Usage: \"reset\"";
    return true;
  }

  try
  {
    { // nesting avoids redec on mutex lock
      std::unique_lock<std::mutex> lock(m_walletSynchronizedMutex);
      m_walletSynchronized = false;
    }

    m_wallet->reset();
    success_msg_writer(true) << "Reset completed successfully.";

    std::unique_lock<std::mutex> lock(m_walletSynchronizedMutex);
    while (!m_walletSynchronized) { m_walletSynchronizedCV.wait(lock); }

    std::cout << std::endl; 
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"reset\" command: " << e.what();
  }

  return true;
}

bool conceal_wallet::get_reserve_proof(const std::vector<std::string> &args)
{
  if (m_is_view_wallet)
  {
    logger(ERROR) << "This is view wallet. Spending is impossible.";
    return true;
  }

  if (args.size() > 2 || args.empty())
  {
    fail_msg_writer() << "Usage: balance_proof (all|<amount>) [<message>]";
    return true;
  }

  uint64_t reserve = 0;
  if (args[0] == "all")
  {
    reserve = m_wallet->actualBalance();
  }
  else
  {
    if (!m_currency.parseAmount(args[0], reserve))
    {
      fail_msg_writer() << "amount is wrong: " << args[0];
      return true;
    }
  }

  try
  {
    const std::string sig_str = m_wallet->getReserveProof(reserve, args.size() == 2 ? args[1] : "");

    std::cout << "\nThe following is sensitive information and will not appear in log files:\n" << sig_str << "\n\n" << std::endl;

    const std::string filename = "balance_proof_" + args[0] + "_CCX.txt";
    boost::system::error_code ec;
    if (boost::filesystem::exists(filename, ec))
    {
      boost::filesystem::remove(filename, ec);
    }
    
    std::ofstream proofFile(filename, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!proofFile.good())
    {
      return false;
    }

    proofFile << sig_str;

    success_msg_writer() << "signature file saved to: " << filename;
  }
  catch (const std::exception &e)
  {
    fail_msg_writer() << e.what();
  }

  return true;
}

bool conceal_wallet::get_tx_proof(const std::vector<std::string> &args)
{
  if(args.size() < 2 && args.size() > 3)
  {
    fail_msg_writer() << "Usage: get_tx_proof <txid> <dest_address> [<txkey>]";
    return true;
  }

  const std::string &str_hash = args[0];
  crypto::Hash txid;
  if (!parse_hash256(str_hash, txid))
  {
    fail_msg_writer() << "Failed to parse txid";
    return true;
  }

  const std::string address_string = args[1];
  cn::AccountPublicAddress address;
  if (!m_currency.parseAccountAddressString(address_string, address))
  {
     fail_msg_writer() << "Failed to parse address " << address_string;
     return true;
  }

  try
  {
    std::string sig_str;
    crypto::SecretKey tx_key;
    crypto::SecretKey tx_key2;
    bool r = m_wallet->get_tx_key(txid, tx_key);
    
    if (args.size() == 3)
    {
      crypto::Hash tx_key_hash;
      size_t size;
      if (!common::fromHex(args[2], &tx_key_hash, sizeof(tx_key_hash), size) || size != sizeof(tx_key_hash))
      {
        fail_msg_writer() << "failed to parse tx_key";
        return true;
      }

      tx_key2 = *(struct crypto::SecretKey *) &tx_key_hash;

      if (r && (args.size() == 3 && tx_key != tx_key2))
      {
        fail_msg_writer() << "Tx secret key was found for the given txid, but you've also provided another tx secret key which doesn't match the found one.";
        return true;
      }

      tx_key = tx_key2;
    }
    else
    {
      if (!r)
      {
        fail_msg_writer() << "Tx secret key wasn't found in the wallet file. Provide it as the optional third parameter if you have it elsewhere.";
        return true;
      }
    }

    if (m_wallet->getTxProof(txid, address, tx_key, sig_str))
    {
      success_msg_writer() << "Signature: " << sig_str << std::endl;
    }
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"get_tx_proof\" command: " << e.what();
  }

  return true;
}

bool conceal_wallet::show_balance(const std::vector<std::string>& args)
{
  if (!args.empty())
  {
    logger(ERROR) << "Usage: \"balance\"";
    return true;
  }

  try
  {
    std::stringstream balances = m_chelper.balances(m_wallet, m_currency);
    logger(INFO) << balances.str();
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"balance\" command: " << e.what();
  }

  return true;
}

bool conceal_wallet::sign_message(const std::vector<std::string>& args)
{
  if (m_is_view_wallet)
  {
    logger(ERROR) << "This is view wallet. Spending is impossible.";
    return true;
  }

  if(args.size() != 1)
  {
    fail_msg_writer() << "Use: sign_message <message>";
    return true;
  }

  try
  {
    AccountKeys keys;
    m_wallet->getAccountKeys(keys);

    crypto::Hash message_hash;
    crypto::Signature sig;
    crypto::cn_fast_hash(args[0].data(), args[0].size(), message_hash);
    crypto::generate_signature(message_hash, keys.address.spendPublicKey, keys.spendSecretKey, sig);
  
    success_msg_writer() << "Sig" << tools::base_58::encode(std::string(reinterpret_cast<char*>(&sig)));
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"sign_message\" command: " << e.what();
  }

  return true;	
}

bool conceal_wallet::verify_signature(const std::vector<std::string>& args)
{
  if (args.size() != 3)
  {
    fail_msg_writer() << "Use: verify_signature <message> <address> <signature>";
    return true;
  }

  std::string encodedSig = args[2];
  const size_t prefix_size = strlen("Sig");

  if(encodedSig.substr(0, prefix_size) != "Sig")
  {
    fail_msg_writer() << "Invalid signature prefix";
    return true;
  } 

  try
  {
    crypto::Hash message_hash;
    crypto::cn_fast_hash(args[0].data(), args[0].size(), message_hash);
  
    std::string decodedSig;
    crypto::Signature sig;
    tools::base_58::decode(encodedSig.substr(prefix_size), decodedSig);
    memcpy(&sig, decodedSig.data(), sizeof(sig));
  
    uint64_t prefix;
    cn::AccountPublicAddress addr;
    cn::parseAccountAddressString(prefix, addr, args[1]);
  
    if(crypto::check_signature(message_hash, addr.spendPublicKey, sig))
    {
      success_msg_writer() << "Valid";
    }
    else
    {
      success_msg_writer() << "Invalid";
    }
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"verify_signature\" command: " << e.what();
  }

  return true;
}

bool conceal_wallet::create_integrated(const std::vector<std::string>& args) 
{
  /* take a payment Id as an argument and generate an integrated wallet address */
  /* check if there is a payment id */
  if (args.empty()) 
  {
    fail_msg_writer() << "Usage: \"create_integrated <payment_id>\"";
    return true;
  }

  std::string paymentID = args[0];
  std::regex hexChars("^[0-9a-f]+$");
  if(paymentID.size() != 64 || !regex_match(paymentID, hexChars))
  {
    fail_msg_writer() << "Invalid payment ID";
    return true;
  }

  try
  {
    std::string address = m_wallet->getAddress();
    uint64_t prefix;
    cn::AccountPublicAddress addr;

    /* get the spend and view public keys from the address */
    if(!cn::parseAccountAddressString(prefix, addr, address))
    {
      logger(ERROR, BRIGHT_RED) << "Failed to parse account address from string";
      return true;
    }

    cn::BinaryArray ba;
    cn::toBinaryArray(addr, ba);
    std::string keys = common::asString(ba);

    /* create the integrated address the same way you make a public address */
    std::string integratedAddress = tools::base_58::encode_addr (cn::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
                                                                paymentID + keys
    );

    std::cout << std::endl << "Integrated address: " << integratedAddress << std::endl << std::endl;
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"create_integrated\" command: " << e.what();
  }

  return true;
}

bool conceal_wallet::export_keys(const std::vector<std::string>& args)
{
  if (!args.empty())
  {
    logger(ERROR) << "Usage: \"export_keys\"";
    return true;
  }

  try
  {
    AccountKeys keys;
    m_wallet->getAccountKeys(keys);

    std::string secretKeysData = std::string(reinterpret_cast<char*>(&keys.spendSecretKey), sizeof(keys.spendSecretKey)) + std::string(reinterpret_cast<char*>(&keys.viewSecretKey), sizeof(keys.viewSecretKey));
    std::string guiKeys = tools::base_58::encode_addr(cn::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX, secretKeysData);

    logger(INFO, BRIGHT_GREEN) << std::endl << "ConcealWallet is an open-source, client-side, free wallet which allow you to send and receive CCX instantly on the blockchain. You are  in control of your funds & your keys. When you generate a new wallet, login, send, receive or deposit $CCX everything happens locally. Your seed is never transmitted, received or stored. That's why its imperative to write, print or save your seed somewhere safe. The backup of keys is your responsibility. If you lose your seed, your account can not be recovered. The Conceal Team doesn't take any responsibility for lost funds due to nonexistent/missing/lost private keys." << std::endl << std::endl;

    std::cout << "Private spend key: " << common::podToHex(keys.spendSecretKey) << std::endl;
    std::cout << "Private view key: " <<  common::podToHex(keys.viewSecretKey) << std::endl;
    //std::cout << "GUI key: " << guiKeys << std::endl;

    crypto::PublicKey unused_dummy_variable;
    crypto::SecretKey deterministic_private_view_key;

    AccountBase::generateViewFromSpend(keys.spendSecretKey, deterministic_private_view_key, unused_dummy_variable);

    bool deterministic_private_keys = deterministic_private_view_key == keys.viewSecretKey;

    /* dont show a mnemonic seed if it is an old non-deterministic wallet */
    if (deterministic_private_keys)
    {
      std::cout << "Mnemonic seed: " << mnemonics::privateKeyToMnemonic(keys.spendSecretKey) << std::endl << std::endl;
    }
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"export_keys\" command: " << e.what();
  }

  return true;
}

bool conceal_wallet::show_incoming_transfers(const std::vector<std::string>& args)
{
  if(!args.empty())
  {
    fail_msg_writer() << "Use: \"incoming_transfers\"";
    return true;
  }

  try
  {
    bool hasTransfers = false;
    size_t transactionsCount = m_wallet->getTransactionCount();

    for (size_t trantransactionNumber = 0; trantransactionNumber < transactionsCount; ++trantransactionNumber)
    {
      WalletLegacyTransaction txInfo;
      m_wallet->getTransaction(trantransactionNumber, txInfo);
      if (txInfo.totalAmount < 0) continue;
      hasTransfers = true;
      logger(INFO) << "        amount       \t                              tx id";
      logger(INFO, GREEN) <<
        std::setw(21) << m_currency.formatAmount(txInfo.totalAmount) << '\t' << common::podToHex(txInfo.hash);
    }
    if (!hasTransfers) { success_msg_writer() << "No incoming transfers"; }
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"incoming_transfers\" command: " << e.what();
  }

  return true;
}

bool conceal_wallet::list_transfers(const std::vector<std::string>& args)
{
  bool haveTransfers = false;
  bool haveBlockHeight = false;
  std::string blockHeightString = ""; 
  uint32_t blockHeight = 0;
  WalletLegacyTransaction txInfo;

  /* get block height from arguments */
  if (args.empty()) 
  {
    haveBlockHeight = false;
  }
  else
  {
    blockHeightString = args[0];
    haveBlockHeight = true;
    blockHeight = atoi(blockHeightString.c_str());
  }

  try
  {
    size_t transactionsCount = m_wallet->getTransactionCount();
    for (size_t trantransactionNumber = 0; trantransactionNumber < transactionsCount; ++trantransactionNumber) 
    {
      m_wallet->getTransaction(trantransactionNumber, txInfo);
      if (txInfo.state != WalletLegacyTransactionState::Active || txInfo.blockHeight == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT)
      {
        continue;
      }

      if (!haveTransfers)
      {
        printListTransfersHeader(logger);
        haveTransfers = true;
      }

      if (haveBlockHeight == false)
      {
        printListTransfersItem(logger, txInfo, *m_wallet, m_currency);
      }
      else
      {
        if (txInfo.blockHeight >= blockHeight)
        {
          printListTransfersItem(logger, txInfo, *m_wallet, m_currency);
        }
      }
    }
    if (!haveTransfers) { success_msg_writer() << "No transfers"; }
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"list_transfers\" command: " << e.what();
  }

  return true;
}

bool conceal_wallet::show_payments(const std::vector<std::string> &args)
{
  if (args.empty())
  {
    fail_msg_writer() << "expected at least one payment ID";
    return true;
  }

  try
  {
    auto hashes = args;
    std::sort(std::begin(hashes), std::end(hashes));
    hashes.erase(std::unique(std::begin(hashes), std::end(hashes)), std::end(hashes));
    std::vector<PaymentId> paymentIds;
    paymentIds.reserve(hashes.size());
    std::transform(std::begin(hashes), std::end(hashes), std::back_inserter(paymentIds), [](const std::string& arg)
    {
      PaymentId paymentId;
      if (!cn::parsePaymentId(arg, paymentId))
      {
        throw std::runtime_error("payment ID has invalid format: \"" + arg + "\", expected 64-character string");
      }
      return paymentId;
    });

    logger(INFO) << "                            payment                             \t" <<
      "                          transaction                           \t" <<
      "  height\t       amount        ";

    auto payments = m_wallet->getTransactionsByPaymentIds(paymentIds);

    for (auto& payment : payments)
    {
      for (auto& transaction : payment.transactions)
      {
        success_msg_writer(true) <<
          common::podToHex(payment.paymentId) << '\t' <<
          common::podToHex(transaction.hash) << '\t' <<
          std::setw(8) << transaction.blockHeight << '\t' <<
          std::setw(21) << m_currency.formatAmount(transaction.totalAmount);
      }

      if (payment.transactions.empty())
      {
        success_msg_writer() << "No payments with id " << common::podToHex(payment.paymentId);
      }
    }
  }
  catch (std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"payments\" command: " << e.what();
  }

  return true;
}

bool conceal_wallet::show_blockchain_height(const std::vector<std::string>& args)
{
  if(!args.empty())
  {
    logger(ERROR) << "Use: \"bc_height\"";
    return true;
  }

  try
  {
    uint64_t bc_height = m_node->getLastLocalBlockHeight();
    success_msg_writer() << bc_height;
  }
  catch (std::exception &e)
  {
    logger(ERROR) << "Failed to execute \"bc_height\" command: " << e.what();
  }

  return true;
}

bool conceal_wallet::show_num_unlocked_outputs(const std::vector<std::string>& args)
{
  if(!args.empty())
  {
    logger(ERROR) << "Usage: \"outputs\"";
    return true;
  }

  try
  {
    std::vector<TransactionOutputInformation> unlocked_outputs = m_wallet->getUnspentOutputs();
    success_msg_writer() << "Count: " << unlocked_outputs.size();
    for (const auto& out : unlocked_outputs)
    {
      success_msg_writer() << "Key: " << out.transactionPublicKey << " amount: " << m_currency.formatAmount(out.amount);
    }
  }
  catch (std::exception &e)
  {
    logger(ERROR) << "Failed to execute \"outputs\" command: " << e.what();
  }

  return true;
}

bool conceal_wallet::optimize_outputs(const std::vector<std::string>& args)
{
  if (m_is_view_wallet)
  {
    logger(ERROR) << "This is view wallet. Spending is impossible.";
    return true;
  }

  if(!args.empty())
  {
    logger(ERROR) << "Usage: \"optimize\"";
    return true;
  }

  try
  {
    cn::WalletHelper::SendCompleteResultObserver sent;
    WalletHelper::IWalletRemoveObserverGuard removeGuard(*m_wallet, sent);

    std::vector<cn::WalletLegacyTransfer> transfers;
    std::vector<cn::TransactionMessage> messages;
    std::string extraString;
    uint64_t fee = cn::parameters::MINIMUM_FEE_V2;
    uint64_t mixIn = 0;
    uint64_t unlockTimestamp = 0;
    uint64_t ttl = 0;
    crypto::SecretKey transactionSK;
    cn::TransactionId tx = m_wallet->sendTransaction(transactionSK, transfers, fee, extraString, mixIn, unlockTimestamp, messages, ttl);
    if (tx == WALLET_LEGACY_INVALID_TRANSACTION_ID)
    {
      fail_msg_writer() << "Can't send money";
      return true;
    }

    std::error_code sendError = sent.wait(tx);
    removeGuard.removeObserver();
    if (sendError)
    {
      fail_msg_writer() << sendError.message();
      return true;
    }

    cn::WalletLegacyTransaction txInfo;
    m_wallet->getTransaction(tx, txInfo);
    success_msg_writer(true) << "Money successfully sent, transaction " << common::podToHex(txInfo.hash);
    success_msg_writer(true) << "Transaction secret key " << common::podToHex(transactionSK);

    m_chelper.save_wallet(*m_wallet, m_wallet_file, logger);
  }
  catch (const std::system_error& e)
  {
    fail_msg_writer() << e.what();
  }
  catch (const std::exception& e)
  {
    fail_msg_writer() << e.what();
  }
  catch (...)
  {
    fail_msg_writer() << "unknown error";
  }

  return true;
}

bool conceal_wallet::optimize_all_outputs(const std::vector<std::string>& args)
{
  if (m_is_view_wallet)
  {
    logger(ERROR) << "This is view wallet. Spending is impossible.";
    return true;
  }

  uint64_t num_unlocked_outputs = 0;

  try
  {
    num_unlocked_outputs = m_wallet->getNumUnlockedOutputs();
    success_msg_writer() << "Total outputs: " << num_unlocked_outputs;
  }
  catch (std::exception &e)
  {
    fail_msg_writer() << "failed to get outputs: " << e.what();
  }

  uint64_t remainder = num_unlocked_outputs % 100;
  uint64_t rounds = (num_unlocked_outputs - remainder) / 100;
  success_msg_writer() << "Total optimization rounds: " << rounds;
  for(uint64_t a = 1; a < rounds; a = a + 1 )
  {
    try {
      cn::WalletHelper::SendCompleteResultObserver sent;
      WalletHelper::IWalletRemoveObserverGuard removeGuard(*m_wallet, sent);

      std::vector<cn::WalletLegacyTransfer> transfers;
      std::vector<cn::TransactionMessage> messages;
      std::string extraString;
      uint64_t fee = cn::parameters::MINIMUM_FEE_V2;
      uint64_t mixIn = 0;
      uint64_t unlockTimestamp = 0;
      uint64_t ttl = 0;
      crypto::SecretKey transactionSK;
      cn::TransactionId tx = m_wallet->sendTransaction(transactionSK, transfers, fee, extraString, mixIn, unlockTimestamp, messages, ttl);
      if (tx == WALLET_LEGACY_INVALID_TRANSACTION_ID)
      {
        fail_msg_writer() << "Can't send money";
        return true;
      }

      std::error_code sendError = sent.wait(tx);
      removeGuard.removeObserver();
      if (sendError)
      {
        fail_msg_writer() << sendError.message();
        return true;
      }

      cn::WalletLegacyTransaction txInfo;
      m_wallet->getTransaction(tx, txInfo);
      success_msg_writer(true) << a << ". Optimization transaction successfully sent, transaction " << common::podToHex(txInfo.hash);

      m_chelper.save_wallet(*m_wallet, m_wallet_file, logger);
    }
    catch (const std::system_error& e)
    {
      fail_msg_writer() << e.what();
    }
    catch (const std::exception& e)
    {
      fail_msg_writer() << e.what();
    }
    catch (...)
    {
      fail_msg_writer() << "unknown error";
    }
  }

  return true;
}

bool conceal_wallet::transfer(const std::vector<std::string> &args)
{
  if (m_is_view_wallet)
  {
    logger(ERROR) << "This is view wallet. Spending is impossible.";
    return true;
  }

  try {
    transfer_cmd cmd(m_currency, m_remote_node_address);

    if (!cmd.parseTx(logger, args))
      return true;

    for (auto& kv: cmd.aliases) {
      std::string address;

      try {
        address = resolveAlias(kv.first);

        AccountPublicAddress ignore;
        if (!m_currency.parseAccountAddressString(address, ignore)) {
          throw std::runtime_error("Address \"" + address + "\" is invalid");
        }
      } catch (std::exception& e) {
        fail_msg_writer() << "Couldn't resolve alias: " << e.what() << ", alias: " << kv.first;
        return true;
      }

      for (auto& transfer: kv.second) {
        transfer.address = address;
      }
    }

    if (!cmd.aliases.empty()) {
      if (!askAliasesTransfersConfirmation(cmd.aliases, m_currency)) {
        return true;
      }

      for (auto& kv: cmd.aliases) {
        std::copy(std::move_iterator<std::vector<WalletLegacyTransfer>::iterator>(kv.second.begin()),
                  std::move_iterator<std::vector<WalletLegacyTransfer>::iterator>(kv.second.end()),
                  std::back_inserter(cmd.dsts));
      }
    }

    std::vector<TransactionMessage> messages;
    for (auto dst : cmd.dsts) {
      for (auto msg : cmd.messages) {
        messages.emplace_back(TransactionMessage{ msg, dst.address });
      }
    }

    uint64_t ttl = 0;
    if (cmd.ttl != 0) {
      ttl = static_cast<uint64_t>(time(nullptr)) + cmd.ttl;
    }

    cn::WalletHelper::SendCompleteResultObserver sent;

    std::string extraString;
    std::copy(cmd.extra.begin(), cmd.extra.end(), std::back_inserter(extraString));

    WalletHelper::IWalletRemoveObserverGuard removeGuard(*m_wallet, sent);

    /* set static mixin of 4*/
    cmd.fake_outs_count = cn::parameters::MINIMUM_MIXIN;

    /* force minimum fee */
    if (cmd.fee < cn::parameters::MINIMUM_FEE_V2) {
      cmd.fee = cn::parameters::MINIMUM_FEE_V2;
    }

    crypto::SecretKey transactionSK;
    cn::TransactionId tx = m_wallet->sendTransaction(transactionSK, cmd.dsts, cmd.fee, extraString, cmd.fake_outs_count, 0, messages, ttl);
    if (tx == WALLET_LEGACY_INVALID_TRANSACTION_ID) {
      fail_msg_writer() << "Can't send money";
      return true;
    }

    std::error_code sendError = sent.wait(tx);
    removeGuard.removeObserver();

    if (sendError) {
      fail_msg_writer() << sendError.message();
      return true;
    }

    cn::WalletLegacyTransaction txInfo;
    m_wallet->getTransaction(tx, txInfo);
    success_msg_writer(true) << "Money successfully sent, transaction hash: " << common::podToHex(txInfo.hash);
    success_msg_writer(true) << "Transaction secret key " << common::podToHex(transactionSK); 

    m_chelper.save_wallet(*m_wallet, m_wallet_file, logger);
  } catch (const std::system_error& e) {
    fail_msg_writer() << e.what();
  } catch (const std::exception& e) {
    fail_msg_writer() << e.what();
  } catch (...) {
    fail_msg_writer() << "unknown error";
  }

  return true;
}

bool conceal_wallet::print_address(const std::vector<std::string> &args)
{
  if (!args.empty())
  {
    logger(ERROR) << "Usage: \"address\"";
    return true;
  }

  try
  {
    logger(INFO) << m_wallet->getAddress();
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"address\" command: " << e.what();
  }

  return true;
}

bool conceal_wallet::save_keys_to_file(const std::vector<std::string>& args)
{
  if (!args.empty())
  {
    logger(ERROR) << "Usage: \"save_keys\"";
    return true;
  }

  try
  {
    std::string formatted_wal_str = m_frmt_wallet_file + "_conceal_backup.txt";
    std::ofstream backup_file(formatted_wal_str);

    AccountKeys keys;
    m_wallet->getAccountKeys(keys);

    std::string priv_key = "\t\tConceal Keys Backup\n\n";
    priv_key += "Wallet file name: " + m_wallet_file + "\n";
    priv_key += "Private spend key: " + common::podToHex(keys.spendSecretKey) + "\n";
    priv_key += "Private view key: " +  common::podToHex(keys.viewSecretKey) + "\n";

    crypto::PublicKey unused_dummy_variable;
    crypto::SecretKey deterministic_private_view_key;

    AccountBase::generateViewFromSpend(keys.spendSecretKey, deterministic_private_view_key, unused_dummy_variable);
    bool deterministic_private_keys = deterministic_private_view_key == keys.viewSecretKey;

    /* dont show a mnemonic seed if it is an old non-deterministic wallet */
    if (deterministic_private_keys)
    {
      std::cout << "Mnemonic seed: " << mnemonics::privateKeyToMnemonic(keys.spendSecretKey) << std::endl << std::endl;
    }

    backup_file << priv_key;

    logger(INFO, BRIGHT_GREEN) << "Wallet keys have been saved to the current folder where \"concealwallet\" is located as \""
      << formatted_wal_str << ".";
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"save_keys\" command: " << e.what();
  }

  return true;
}

bool conceal_wallet::save_all_txs_to_file(const std::vector<std::string> &args)
{
  /* check args, default: include_deposits = false */
  bool include_deposits;

  if (args.empty() || args[0] == "false")
  {
    include_deposits = false;
  }
  else if (args[0] == "true")
  {
    include_deposits = true;
  }
  else
  {
    logger(ERROR) << "Usage: \"save_txs_to_file\" - Saves only transactions to file.\n"
      << "        \"save_txs_to_file false\" - Saves only transactions to file.\n"
      << "        \"save_txs_to_file true\" - Saves transactions and deposits to file.";
    return true;
  }

  try
  {
    /* get total txs in wallet */
    size_t tx_count = m_wallet->getTransactionCount();

    /* check wallet txs before doing work */
    if (tx_count == 0) {
      logger(ERROR, BRIGHT_RED) << "No transfers";
      return true;
    }

    /* tell user about prepped job */
    logger(INFO) << "Preparing file and " << std::to_string(tx_count) << " transactions...";

    /* create filename for later use */
    std::string formatted_wal_str = m_frmt_wallet_file + "_conceal_transactions.csv";

    /* get tx struct */
    WalletLegacyTransaction txInfo;

    /* declare csv_writer */
    csv_writer csv;

    /* go through tx ids for the amount of transactions in wallet */
    for (TransactionId i = 0; i < tx_count; ++i) 
    {
      /* get tx to list from i */
      m_wallet->getTransaction(i, txInfo);

      /* check tx state */
      if (txInfo.state != WalletLegacyTransactionState::Active || txInfo.blockHeight == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT)
      {
        continue;
      }

      /* get the tx from this struct */
      ListedTxItem tx_item = m_chelper.tx_item(txInfo, m_currency);

      /* make sure we dont exceed 6 columns */
      csv.enableAutoNewRow(6);

      /* create a single line with this information */
      csv.newRow() << tx_item.timestamp <<
          tx_item.tx_hash <<
          tx_item.amount <<
          tx_item.fee <<
          tx_item.block_height <<
          tx_item.unlock_time;

      /* write line to file */
      csv.writeToFile(formatted_wal_str, true);

      /* tell user about progress */
      logger(INFO) << "Transaction: " << i << " was pushed to " << formatted_wal_str;
    }

    /* tell user job complete */
    logger(INFO, BRIGHT_GREEN) << "All transactions have been saved to the current folder where the wallet file is located as \""
      << formatted_wal_str << "\".";
  
    /* if user uses "save_txs_to_file true" then we go through the deposits */
    if (include_deposits == true)
    {
      /* get total deposits in wallet */
      size_t deposit_count = m_wallet->getDepositCount();

      /* check wallet txs before doing work */
      if (deposit_count == 0)
      {
        logger(ERROR, BRIGHT_RED) << "No deposits";
        return true;
      }

      /* tell user about prepped job */
      logger(INFO) << "Preparing " << std::to_string(deposit_count) << " deposits...";

      /* go through deposits ids for the amount of deposits in wallet */
      for (DepositId id = 0; id < deposit_count; ++id)
      {
        /* get deposit info from id and store it to deposit */
        Deposit deposit = m_wallet->get_deposit(id);
        cn::WalletLegacyTransaction txInfo;

        /* get deposit info and use its transaction in the chain */
        m_wallet->getTransaction(deposit.creatingTransactionId, txInfo);

        /* get the tx from this struct */
        ListedDepositItem deposit_item = m_chelper.list_deposit_item(txInfo, deposit, id, m_currency);

        /* make sure we dont exceed 7 columns */
        csv.enableAutoNewRow(7);

        /* create a single line with this information */
        csv.newRow() << deposit_item.timestamp <<
            deposit_item.id <<
            deposit_item.amount <<
            deposit_item.interest <<
            deposit_item.block_height <<
            deposit_item.unlock_time <<
            deposit_item.status;

        /* write line to file */
        csv.writeToFile(formatted_wal_str, true);

        /* tell user about progress */
        logger(INFO) << "Deposit: " << id << " was pushed to " << formatted_wal_str;
      }

      /* tell user job complete */
      logger(INFO, BRIGHT_GREEN) << "All deposits have been saved to the end of the file current folder where the wallet file is located as \""
        << formatted_wal_str << "\".";
    }
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"save_txs_to_file\" command: " << e.what();
  }

  return true;
}

bool conceal_wallet::list_deposits(const std::vector<std::string> &args)
{
  if (!args.empty())
  {
    logger(ERROR) << "Usage: \"list_deposits\"";
    return true;
  }

  try
  {
    bool haveDeposits = m_wallet->getDepositCount() > 0;

    if (!haveDeposits)
    {
      success_msg_writer() << "No deposits";
      return true;
    }

    printListDepositsHeader(logger);

    /* go through deposits ids for the amount of deposits in wallet */
    for (DepositId id = 0; id < m_wallet->getDepositCount(); ++id)
    {
      /* get deposit info from id and store it to deposit */
      Deposit deposit = m_wallet->get_deposit(id);
      cn::WalletLegacyTransaction txInfo;
      m_wallet->getTransaction(deposit.creatingTransactionId, txInfo);
      logger(INFO) << m_chelper.get_deposit_info(deposit, id, m_currency, txInfo);
    }
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"list_deposits\" command: " << e.what();
  }

  return true;
}

bool conceal_wallet::deposit(const std::vector<std::string> &args)
{
  if (m_is_view_wallet)
  {
    logger(ERROR) << "This is view wallet. Spending is impossible.";
    return true;
  }

  if (args.size() != 2)
  {
    logger(ERROR) << "Usage: deposit <months> <amount>";
    return true;
  }

  try
  {
    /**
     * Change arg to uint64_t using boost then multiply by min_term so user can type in months
    **/
    uint64_t min_term = m_testnet ? parameters::TESTNET_DEPOSIT_MIN_TERM_V3 : parameters::DEPOSIT_MIN_TERM_V3;
    uint64_t max_term = m_testnet ? parameters::TESTNET_DEPOSIT_MAX_TERM_V3 : parameters::DEPOSIT_MAX_TERM_V3;
    uint64_t deposit_term = boost::lexical_cast<uint64_t>(args[0]) * min_term;

    /* Now validate the deposit term and the amount */
    if (deposit_term < min_term)
    {
      logger(ERROR, BRIGHT_RED) << "Deposit term is too small, min=" << min_term << ", given=" << deposit_term;
      return true;
    }

    if (deposit_term > max_term)
    {
      logger(ERROR, BRIGHT_RED) << "Deposit term is too big, max=" << max_term << ", given=" << deposit_term;
      return true;
    }

    uint64_t deposit_amount = boost::lexical_cast<uint64_t>(args[1]);
    bool ok = m_currency.parseAmount(args[1], deposit_amount);

    if (!ok || 0 == deposit_amount)
    {
      logger(ERROR, BRIGHT_RED) << "amount is wrong: " << deposit_amount <<
        ", expected number from 1 to " << m_currency.formatAmount(cn::parameters::MONEY_SUPPLY);
      return true;
    }

    if (deposit_amount < cn::parameters::DEPOSIT_MIN_AMOUNT)
    {
      logger(ERROR, BRIGHT_RED) << "Deposit amount is too small, min=" << cn::parameters::DEPOSIT_MIN_AMOUNT
        << ", given=" << m_currency.formatAmount(deposit_amount);
      return true;
    }

    if (!m_chelper.confirm_deposit(deposit_term, deposit_amount, m_testnet, m_currency, logger))
    {
      logger(ERROR) << "Deposit is not being created.";
      return true;
    }

    logger(INFO) << "Creating deposit...";

    /* Use defaults for fee + mix in */
    uint64_t deposit_fee = cn::parameters::MINIMUM_FEE_V2;
    uint64_t deposit_mix_in = cn::parameters::MINIMUM_MIXIN;

    cn::WalletHelper::SendCompleteResultObserver sent;
    WalletHelper::IWalletRemoveObserverGuard removeGuard(*m_wallet, sent);

    cn::TransactionId tx = m_wallet->deposit(deposit_term, deposit_amount, deposit_fee, deposit_mix_in);

    if (tx == WALLET_LEGACY_INVALID_DEPOSIT_ID)
    {
      fail_msg_writer() << "Can't deposit money";
      return true;
    }

    std::error_code sendError = sent.wait(tx);
    removeGuard.removeObserver();

    if (sendError)
    {
      fail_msg_writer() << sendError.message();
      return true;
    }

    cn::WalletLegacyTransaction d_info;
    m_wallet->getTransaction(tx, d_info);
    success_msg_writer(true) << "Money successfully sent, transaction hash: " << common::podToHex(d_info.hash)
      << "\n\tID: " << d_info.firstDepositId;

    m_chelper.save_wallet(*m_wallet, m_wallet_file, logger);
  }
  catch (const std::system_error& e)
  {
    fail_msg_writer() << e.what();
  }
  catch (const std::exception& e)
  {
    fail_msg_writer() << e.what();
  }
  catch (...)
  {
    fail_msg_writer() << "unknown error";
  }

  return true;
}

bool conceal_wallet::withdraw(const std::vector<std::string> &args)
{
  if (m_is_view_wallet)
  {
    logger(ERROR) << "This is view wallet. Spending is impossible.";
    return true;
  }

  if (args.size() != 1)
  {
    logger(ERROR) << "Usage: withdraw <id>";
    return true;
  }

  try
  {
    if (m_wallet->getDepositCount() == 0)
    {
      logger(ERROR) << "No deposits have been made in this wallet.";
      return true;
    }

    uint64_t deposit_id = boost::lexical_cast<uint64_t>(args[0]);
    uint64_t deposit_fee = cn::parameters::MINIMUM_FEE_V2;
    
    cn::WalletHelper::SendCompleteResultObserver sent;
    WalletHelper::IWalletRemoveObserverGuard removeGuard(*m_wallet, sent);

    cn::TransactionId tx = m_wallet->withdrawDeposit(deposit_id, deposit_fee);
  
    if (tx == WALLET_LEGACY_INVALID_DEPOSIT_ID)
    {
      fail_msg_writer() << "Can't withdraw money";
      return true;
    }

    std::error_code sendError = sent.wait(tx);
    removeGuard.removeObserver();

    if (sendError)
    {
      fail_msg_writer() << sendError.message();
      return true;
    }

    cn::WalletLegacyTransaction d_info;
    m_wallet->getTransaction(tx, d_info);
    success_msg_writer(true) << "Money successfully sent, transaction hash: " << common::podToHex(d_info.hash);

    m_chelper.save_wallet(*m_wallet, m_wallet_file, logger);
  }
  catch (std::exception &e)
  {
    fail_msg_writer() << "failed to withdraw deposit: " << e.what();
  }

  return true;
}

bool conceal_wallet::deposit_info(const std::vector<std::string> &args)
{
  if (args.size() != 1)
  {
    logger(ERROR) << "Usage: deposit_info <id>";
    return true;
  }

  try
  {
    uint64_t deposit_id = boost::lexical_cast<uint64_t>(args[0]);
    cn::Deposit deposit;
    if (!m_wallet->getDeposit(deposit_id, deposit))
    {
      logger(ERROR, BRIGHT_RED) << "Error: Invalid deposit id: " << deposit_id;
      return false;
    }

    cn::WalletLegacyTransaction txInfo;
    m_wallet->getTransaction(deposit.creatingTransactionId, txInfo);

    logger(INFO) << m_chelper.get_full_deposit_info(deposit, deposit_id, m_currency, txInfo);
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"deposit_info\" command: " << e.what();
  }

  return true;
}

bool conceal_wallet::check_address(const std::vector<std::string> &args)
{
  if (args.size() != 1) 
  {
    logger(ERROR) << "Usage: check_address <address>";
    return true;
  }

  try
  {
    const std::string addr = args[0];

    if (!cn::validateAddress(addr, m_currency))
    {
      logger(ERROR) << "Invalid wallet address: " << addr;
      return true;
    }

    logger(INFO) << "The wallet " << addr << " seems to be valid, please still be cautious still.";
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"check_address\" command: " << e.what();
  }

  return true;
}

bool conceal_wallet::read_file_csv(const std::vector<std::string> &args)
{
  // designed to read and print csv file, could be used for other things in future
  std::string filename = m_frmt_wallet_file + "_conceal_transactions.csv";

  std::ifstream file(filename);
  std::vector<std::vector<std::string>> matrix;
  std::vector<std::string> row;
  std::string line;
  std::string cell;

  while(file)
  {
    std::getline(file, line);
    std::stringstream lineStream(line);
    row.clear();

    while(std::getline(lineStream, cell, ',' ))
    {
      row.push_back(cell);
    }
    if(!row.empty())
    {
      matrix.push_back( row );
    }
  }

  for(int i = 0; i < int(matrix.size()); i++) // row
  {
    for(int j = 0; j < int(matrix[i].size()); j++) // column
    {
      // dont clog the logger, use a cout
      std::cout << matrix[i][j] << " ";
    }

    std::cout << std::endl;
  }

  return true;
}

bool conceal_wallet::show_view_key(const std::vector<std::string> &args)
{
  if (!args.empty())
  {
    logger(ERROR) << "Usage: \"show_view_tracking\"";
    return true;
  }

  try
  {
    AccountKeys keys;
    m_wallet->getAccountKeys(keys);
    std::string spend_public_key = common::podToHex(keys.address.spendPublicKey);
    keys.spendSecretKey = boost::value_initialized<crypto::SecretKey>();

    std::cout << "View Tracking Key: " << spend_public_key << common::podToHex(keys.address.viewPublicKey)
      << common::podToHex(keys.spendSecretKey) << common::podToHex(keys.viewSecretKey) << std::endl;
  }
  catch(const std::exception& e)
  {
    logger(ERROR) << "Failed to execute \"show_view_tracking\" command: " << e.what();
  }
  return true;
}
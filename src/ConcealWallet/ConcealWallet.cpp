// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ConcealWallet.h"
#include "TransferCmd.h"
#include "Const.h"

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

#include "Common/Base58.h"
#include "Common/CommandLine.h"
#include "Common/SignalHandler.h"
#include "Common/StringTools.h"
#include "Common/PathTools.h"
#include "Common/Util.h"
#include "Common/DnsTools.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "NodeRpcProxy/NodeRpcProxy.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"
#include "Rpc/HttpClient.h"
#include "CryptoNoteCore/CryptoNoteTools.h"

#include "Wallet/WalletGreen.h"
#include "Wallet/WalletRpcServer.h"
#include "Wallet/WalletUtils.h"
#include "Wallet/LegacyKeysImporter.h"

#include "version.h"

#include <Logging/LoggerManager.h>
#include <Mnemonics/Mnemonics.h>

#if defined(WIN32)
#include <crtdbg.h>
#endif

using namespace cn;
using namespace logging;
using common::JsonValue;

#define EXTENDED_LOGS_FILE "wallet_details.log"
#undef ERROR

namespace {

inline std::string interpret_rpc_response(bool ok, const std::string& status) {
  std::string err;
  if (ok) {
    if (status == CORE_RPC_STATUS_BUSY) {
      err = "daemon is busy. Please try later";
    } else if (status != CORE_RPC_STATUS_OK) {
      err = status;
    }
  } else {
    err = "possible lost connection to daemon";
  }
  return err;
}

void printListTransfersHeader(LoggerRef& logger) {
  std::string header = common::makeCenteredString(TIMESTAMP_MAX_WIDTH, "timestamp (UTC)") + "  ";
  header += common::makeCenteredString(HASH_MAX_WIDTH, "hash") + "  ";
  header += common::makeCenteredString(TOTAL_AMOUNT_MAX_WIDTH, "total amount") + "  ";
  header += common::makeCenteredString(FEE_MAX_WIDTH, "fee") + "  ";
  header += common::makeCenteredString(BLOCK_MAX_WIDTH, "block") + "  ";
  header += common::makeCenteredString(UNLOCK_TIME_MAX_WIDTH, "unlock time");

  logger(INFO) << header;
  logger(INFO) << std::string(header.size(), '-');
}

void printListDepositsHeader(LoggerRef& logger) {
  std::string header = common::makeCenteredString(8, "ID") + " | ";
  header += common::makeCenteredString(20, "Amount") + " | ";
  header += common::makeCenteredString(20, "Interest") + " | ";
  header += common::makeCenteredString(16, "Unlock Height") + " | ";
  header += common::makeCenteredString(10, "State");

  logger(INFO) << "\n" << header;
  logger(INFO) << std::string(header.size(), '=');
}

void printListTransfersItem(LoggerRef& logger, const WalletTransaction& txInfo, IWallet& wallet, const Currency& currency, size_t transactionIndex) {
  std::vector<uint8_t> extraVec = common::asBinaryArray(txInfo.extra);

  crypto::Hash paymentId;
  std::string paymentIdStr = (getPaymentIdFromTxExtra(extraVec, paymentId) && paymentId != NULL_HASH ? common::podToHex(paymentId) : "");

  std::string timeString = formatTimestamp(static_cast<time_t>(txInfo.timestamp));

  std::string rowColor = txInfo.totalAmount < 0 ? MAGENTA : GREEN;
  logger(INFO, rowColor)
    << std::left << std::setw(TIMESTAMP_MAX_WIDTH) << timeString
    << "  " << std::setw(HASH_MAX_WIDTH) << common::podToHex(txInfo.hash)
    << "  " << std::setw(TOTAL_AMOUNT_MAX_WIDTH) << currency.formatAmount(txInfo.totalAmount)
    << "  " << std::setw(FEE_MAX_WIDTH) << currency.formatAmount(txInfo.fee)
    << "  " << std::setw(BLOCK_MAX_WIDTH) << txInfo.blockHeight
    << "  " << std::setw(UNLOCK_TIME_MAX_WIDTH) << txInfo.unlockTime;

  if (!paymentIdStr.empty()) {
    logger(INFO, rowColor) << "payment ID: " << paymentIdStr;
  }

  if (txInfo.totalAmount < 0)
  {
    if (wallet.getTransactionTransferCount(transactionIndex) > 0)
    {
      logger(INFO, rowColor) << "transfers:";
      for (size_t id = 0; id < wallet.getTransactionTransferCount(transactionIndex); ++id)
      {
        WalletTransfer tr = wallet.getTransactionTransfer(transactionIndex, id);
        logger(INFO, rowColor) << tr.address << "  " << std::setw(TOTAL_AMOUNT_MAX_WIDTH) << currency.formatAmount(tr.amount);
      }
    }
  }

  logger(INFO, rowColor) << " "; //just to make logger print one endline
}

std::string prepareWalletAddressFilename(const std::string& walletBaseName) {
  return walletBaseName + ".address";
}

bool writeAddressFile(const std::string& addressFilename, const std::string& address) {
  std::ofstream addressFile(addressFilename, std::ios::out | std::ios::trunc | std::ios::binary);
  if (!addressFile.good()) {
    return false;
  }

  addressFile << address;

  return true;
}

bool processServerAliasResponse(const std::string& s, std::string& address) {
  try {
  //   
  // Courtesy of Monero Project
		// make sure the txt record has "oa1:ccx" and find it
		auto pos = s.find("oa1:ccx");
		if (pos == std::string::npos)
			return false;
		// search from there to find "recipient_address="
		pos = s.find("recipient_address=", pos);
		if (pos == std::string::npos)
			return false;
		pos += 18; // move past "recipient_address="
		// find the next semicolon
		auto pos2 = s.find(";", pos);
		if (pos2 != std::string::npos)
		{
			// length of address == 95, we can at least validate that much here
			if (pos2 - pos == 98)
			{
				address = s.substr(pos, 98);
			} else {
				return false;
			}
		}
    }
	catch (std::exception&) {
		return false;
	}

	return true;
}

bool splitUrlToHostAndUri(const std::string& aliasUrl, std::string& host, std::string& uri) {
  size_t protoBegin = aliasUrl.find("http://");
  if (protoBegin != 0 && protoBegin != std::string::npos) {
    return false;
  }

  size_t hostBegin = protoBegin == std::string::npos ? 0 : 7; //strlen("http://")
  size_t hostEnd = aliasUrl.find('/', hostBegin);

  if (hostEnd == std::string::npos) {
    uri = "/";
    host = aliasUrl.substr(hostBegin);
  } else {
    uri = aliasUrl.substr(hostEnd);
    host = aliasUrl.substr(hostBegin, hostEnd - hostBegin);
  }

  return true;
}

bool askAliasesTransfersConfirmation(const std::map<std::string, std::vector<WalletOrder>>& aliases, const Currency& currency) {
  std::cout << "Would you like to send money to the following addresses?" << std::endl;

  for (const auto& kv: aliases) {
    for (const auto& transfer: kv.second) {
      std::cout << transfer.address << " " << std::setw(21) << currency.formatAmount(transfer.amount) << "  " << kv.first << std::endl;
    }
  }

  std::string answer;
  do {
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
    if (rootIt == json.getObject().end()) {
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

}

std::string conceal_wallet::get_commands_str(bool do_ext) {
  std::stringstream ss;
  ss << "";

  std::string usage;
  if (do_ext)
    usage = wallet_menu(true);
  else
    usage = wallet_menu(false);

  boost::replace_all(usage, "\n", "\n  ");
  usage.insert(0, "  ");

  ss << usage << std::endl;

  return ss.str();
}

bool conceal_wallet::help(const std::vector<std::string> &args/* = std::vector<std::string>()*/) {
  success_msg_writer() << get_commands_str(false);
  return true;
}

bool conceal_wallet::extended_help(const std::vector<std::string> &args/* = std::vector<std::string>()*/) {
  success_msg_writer() << get_commands_str(true);
  return true;
}

bool conceal_wallet::exit(const std::vector<std::string> &args)
{
  stop();
  return true;
}

conceal_wallet::conceal_wallet(platform_system::Dispatcher& dispatcher, const cn::Currency& currency, logging::LoggerManager& log) :
  m_dispatcher(dispatcher),
  m_stopEvent(m_dispatcher),
  m_daemon_port(0),
  m_currency(currency),
  logManager(log),
  logger(log, "concealwallet"),
  m_refresh_progress_reporter(*this),
  m_initResultPromise(nullptr),
  m_walletSynchronized(false) {
  m_consoleHandler.setHandler("create_integrated", boost::bind(&conceal_wallet::create_integrated, this, boost::arg<1>()), "create_integrated <payment_id> - Create an integrated address with a payment ID");
  m_consoleHandler.setHandler("export_keys", boost::bind(&conceal_wallet::export_keys, this, boost::arg<1>()), "Show the secret keys of the current wallet");
  m_consoleHandler.setHandler("balance", boost::bind(&conceal_wallet::show_balance, this, boost::arg<1>()), "Show current wallet balance");
  m_consoleHandler.setHandler("sign_message", boost::bind(&conceal_wallet::sign_message, this, boost::arg<1>()), "Sign a message with your wallet keys");
  m_consoleHandler.setHandler("verify_signature", boost::bind(&conceal_wallet::verify_signature, this, boost::arg<1>()), "Verify a signed message");
  m_consoleHandler.setHandler("incoming_transfers", boost::bind(&conceal_wallet::show_incoming_transfers, this, boost::arg<1>()), "Show incoming transfers");
  m_consoleHandler.setHandler("list_transfers", boost::bind(&conceal_wallet::listTransfers, this, boost::arg<1>()), "list_transfers <height> - Show all known transfers from a certain (optional) block height");
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
  m_consoleHandler.setHandler("help", boost::bind(&conceal_wallet::help, this, boost::arg<1>()), "Show this help");
  m_consoleHandler.setHandler("ext_help", boost::bind(&conceal_wallet::extended_help, this, boost::arg<1>()), "Show this help");
  m_consoleHandler.setHandler("exit", boost::bind(&conceal_wallet::exit, this, boost::arg<1>()), "Close wallet");  
  m_consoleHandler.setHandler("balance_proof", boost::bind(&conceal_wallet::get_reserve_proof, this, boost::arg<1>()), "all|<amount> [<message>] - Generate a signature proving that you own at least <amount>, optionally with a challenge string <message>. ");
  m_consoleHandler.setHandler("save_keys", boost::bind(&conceal_wallet::save_keys_to_file, this, boost::arg<1>()), "Saves wallet private keys to \"<wallet_name>_conceal_backup.txt\"");
  m_consoleHandler.setHandler("list_deposits", boost::bind(&conceal_wallet::list_deposits, this, boost::arg<1>()), "Show all known deposits from this wallet");
  m_consoleHandler.setHandler("deposit", boost::bind(&conceal_wallet::deposit, this, boost::arg<1>()), "deposit <months> <amount> - Create a deposit");
  m_consoleHandler.setHandler("withdraw", boost::bind(&conceal_wallet::withdraw, this, boost::arg<1>()), "withdraw <id> - Withdraw a deposit");
  m_consoleHandler.setHandler("deposit_info", boost::bind(&conceal_wallet::deposit_info, this, boost::arg<1>()), "deposit_info <id> - Get infomation for deposit <id>");
  m_consoleHandler.setHandler("save_txs_to_file", boost::bind(&conceal_wallet::save_all_txs_to_file, this, boost::arg<1>()), "save_txs_to_file - Saves all known transactions to <wallet_name>_conceal_transactions.txt");
  m_consoleHandler.setHandler("check_address", boost::bind(&conceal_wallet::check_address, this, boost::arg<1>()), "check_address <address> - Checks to see if given wallet is valid.");
}

std::string conceal_wallet::wallet_menu(bool do_ext)
{
  std::string menu_item;

  if (do_ext)
  {
    menu_item += "\t\tConceal Wallet Extended Menu\n\n";
    menu_item += "[ ] = Optional arg\n";
    menu_item += "\"balance_proof <amount>\"                           - Generate a signature proving that you own at least <amount> | [<message>]\n";
    menu_item += "\"create_integrated <payment_id>\"                   - Create an integrated address with a payment ID.\n";
    menu_item += "\"get_tx_proof <txid> <address>\"                    - Generate a signature to prove payment | [<txkey>]\n";
    menu_item += "\"incoming_transfers\"                               - Show incoming transfers.\n";
    menu_item += "\"optimize\"                                         - Combine many available outputs into a few by sending a transaction to self.\n";
    menu_item += "\"optimize_all\"                                     - Optimize your wallet several times so you can send large transactions.\n";
    menu_item += "\"outputs\"                                          - Show the number of unlocked outputs available for a transaction.\n";
    menu_item += "\"payments <payment_id>\"                            - Show payments from payment ID. | [<payment_id_2> ... <payment_id_N>]\n";
    menu_item += "\"save_txs_to_file\"                                 - Saves all known transactions to <wallet_name>_conceal_transactions.txt | [false] or [true] to include deposits (default: false)\n";
    menu_item += "\"set_log <level>\"                                  - Change current log level, default = 3, <level> is a number 0-4.\n";
    menu_item += "\"sign_message <message>\"                           - Sign a message with your wallet keys.\n";
    menu_item += "\"show_dust\"                                        - Show the number of unmixable dust outputs.\n";
    menu_item += "\"verify_signature <message> <address> <signature>\" - Verify a signed message.\n";
  }
  else
  {
    menu_item += "\t\tConceal Wallet Menu\n\n";
    menu_item += "[ ] = Optional arg\n\n";
    menu_item += "\"help\" | \"ext_help\"           - Shows this help dialog or extended help dialog.\n\n";
    menu_item += "\"address\"                     - Shows wallet address.\n";
    menu_item += "\"balance\"                     - Shows wallet main and deposit balance.\n";
    menu_item += "\"bc_height\"                   - Shows current blockchain height.\n";
    menu_item += "\"check_address <address>\"     - Checks to see if given wallet is valid.\n";
    menu_item += "\"deposit <months> <amount>\"   - Create a deposit to the blockchain.\n";
    menu_item += "\"deposit_info <id>\"           - Display full information for deposit <id>.\n";
    menu_item += "\"exit\"                        - Safely exits the wallet application.\n";
    menu_item += "\"export_keys\"                 - Displays backup keys.\n";
    menu_item += "\"list_deposits\"               - Show all known deposits.\n";
    menu_item += "\"list_transfers\"              - Show all known transfers, optionally from a certain height. | <block_height>\n";
    menu_item += "\"reset\"                       - Reset cached blockchain data and starts synchronizing from block 0.\n";
    menu_item += "\"transfer <address> <amount>\" - Transfers <amount> to <address>. | [-p<payment_id>] [<amount_2>]...[<amount_N>] [<address_2>]...[<address_n>]\n";
    menu_item += "\"save\"                        - Save wallet synchronized blockchain data.\n";
    menu_item += "\"save_keys\"                   - Saves wallet private keys to \"<wallet_name>_conceal_backup.txt\".\n";
    menu_item += "\"withdraw <id>\"               - Withdraw a deposit from the blockchain.\n";
  }

  return menu_item;
}

/* This function shows the number of outputs in the wallet
  that are below the dust threshold */
bool conceal_wallet::show_dust(const std::vector<std::string> &)
{
  logger(INFO, BRIGHT_WHITE) << "Dust outputs: " << m_wallet->getDustBalance();
  return true;
}

//----------------------------------------------------------------------------------------------------
bool conceal_wallet::set_log(const std::vector<std::string> &args) {
  if (args.size() != 1) {
    fail_msg_writer() << "use: set_log <log_level_number_0-4>";
    return true;
  }

  uint16_t l = 0;
  if (!common::fromString(args[0], l)) {
    fail_msg_writer() << "wrong number format, use: set_log <log_level_number_0-4>";
    return true;
  }

  if (l > logging::TRACE) {
    fail_msg_writer() << "wrong number range, use: set_log <log_level_number_0-4>";
    return true;
  }

  logManager.setMaxLevel(static_cast<logging::Level>(l));
  return true;
}

bool key_import = true;

//----------------------------------------------------------------------------------------------------
bool conceal_wallet::init(const boost::program_options::variables_map& vm) {
  if (command_line::has_arg_2(vm, arg_daemon_address) && (command_line::has_arg_2(vm, arg_daemon_host) || command_line::has_arg_2(vm, arg_daemon_port)))
  {
    fail_msg_writer() << "you can't specify daemon host or port several times";
    return false;
  }
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

  if (m_generate_new.empty() && m_wallet_file_arg.empty()) {
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
    std::cout << "How you would like to proceed?\n\n\t[O]pen an existing wallet\n\t[G]enerate a new wallet file\n\t[I]mport wallet from keys\n\t[M]nemonic seed import\n\t[E]xit.\n\n";
    char c;
    do {
      std::string answer;
      std::getline(std::cin, answer);
      c = answer[0];
      if (!(c == 'O' || c == 'G' || c == 'E' || c == 'I' || c == 'o' || c == 'g' || c == 'e' || c == 'i' || c == 'm' || c == 'M')) {
        std::cout << "Unknown command: " << c <<std::endl;
      } else {
        break;
      }
    } while (true);

    if (c == 'E' || c == 'e') {
      return false;
    }

    std::string homeEnvVar;

    if (BOOST_OS_WINDOWS)
      homeEnvVar = "USERPROFILE";
    else
      homeEnvVar = "HOME";

    std::cout << "Specify wallet file name (e.g., name.wallet).\n";
    std::string userInput;
    do {
      std::cout << "Wallet file name: ";
      std::getline(std::cin, userInput);
      userInput = std::regex_replace(userInput, std::regex("~"), getenv(homeEnvVar.c_str()));
      boost::algorithm::trim(userInput);
    } while (userInput.empty());
    if (c == 'i' || c == 'I'){
      key_import = true;
      m_import_new = userInput;
    } else if (c == 'm' || c == 'M') {
      key_import = false;
      m_import_new = userInput;
    } else if (c == 'g' || c == 'G') {
      m_generate_new = userInput;
    } else {
      m_wallet_file_arg = userInput;
    }
  }



  if (!m_generate_new.empty() && !m_wallet_file_arg.empty() && !m_import_new.empty()) {
    fail_msg_writer() << "you can't specify 'generate-new-wallet' and 'wallet-file' arguments simultaneously";
    return false;
  }

  std::string walletFileName;
  if (!m_generate_new.empty() || !m_import_new.empty()) {
    std::string ignoredString;
    if (!m_generate_new.empty()) {
      WalletHelper::prepareFileNames(m_generate_new, ignoredString, walletFileName);
    } else if (!m_import_new.empty()) {
      WalletHelper::prepareFileNames(m_import_new, ignoredString, walletFileName);
    }
    boost::system::error_code ignore;
    if (boost::filesystem::exists(walletFileName, ignore)) {
      fail_msg_writer() << walletFileName << " already exists";
      return false;
    }
  }



  tools::PasswordContainer pwd_container;
  if (command_line::has_arg(vm, arg_password)) {
    pwd_container.password(command_line::get_arg(vm, arg_password));
  } else if (!pwd_container.read_password()) {
    fail_msg_writer() << "failed to read wallet password";
    return false;
  }

  this->m_node.reset(new NodeRpcProxy(m_daemon_host, m_daemon_port));
  NodeInitObserver initObserver;
  m_node->init(std::bind(&NodeInitObserver::initCompleted, &initObserver, std::placeholders::_1));
  initObserver.waitForInitEnd();

  if (!m_generate_new.empty()) {
    std::string walletAddressFile = prepareWalletAddressFilename(m_generate_new);
    boost::system::error_code ignore;
    if (boost::filesystem::exists(walletAddressFile, ignore)) {
      logger(ERROR, BRIGHT_RED) << "Address file already exists: " + walletAddressFile;
      return false;
    }

    if (!new_wallet(walletFileName, pwd_container.password())) {
      logger(ERROR, BRIGHT_RED) << "account creation failed";
      return false;
    }

    if (!writeAddressFile(walletAddressFile, m_wallet->getAddress(0)))
    {
      logger(WARNING, BRIGHT_RED) << "Couldn't write wallet address file: " + walletAddressFile;
    }
  } else if (!m_import_new.empty()) {
    std::string walletAddressFile = prepareWalletAddressFilename(m_import_new);
    boost::system::error_code ignore;
    if (boost::filesystem::exists(walletAddressFile, ignore)) {
      logger(ERROR, BRIGHT_RED) << "Address file already exists: " + walletAddressFile;
      return false;
    }

    std::string private_spend_key_string;
    std::string private_view_key_string;

    crypto::SecretKey private_spend_key;
    crypto::SecretKey private_view_key;

    if (key_import) {
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
    } else {
      std::string mnemonic_phrase;
  
      do {
        std::cout << "Mnemonics Phrase (25 words): ";
        std::getline(std::cin, mnemonic_phrase);
        boost::algorithm::trim(mnemonic_phrase);
        boost::algorithm::to_lower(mnemonic_phrase);
      } while (mnemonic_phrase.empty());

      private_spend_key = mnemonics::mnemonicToPrivateKey(mnemonic_phrase);

      /* This is not used, but is needed to be passed to the function, not sure how we can avoid this */
      crypto::PublicKey unused_dummy_variable;

      AccountBase::generateViewFromSpend(private_spend_key, private_view_key, unused_dummy_variable);
    }

    /* We already have our keys if we import via mnemonic seed */
    if (key_import) {
      crypto::Hash private_spend_key_hash;
      crypto::Hash private_view_key_hash;
      size_t size;

      if (!common::fromHex(private_spend_key_string, &private_spend_key_hash, sizeof(private_spend_key_hash), size) || size != sizeof(private_spend_key_hash)) {
        return false;
      }
      if (!common::fromHex(private_view_key_string, &private_view_key_hash, sizeof(private_view_key_hash), size) || size != sizeof(private_spend_key_hash)) {
        return false;
      }

      private_spend_key = *(struct crypto::SecretKey *) &private_spend_key_hash;
      private_view_key = *(struct crypto::SecretKey *) &private_view_key_hash;
    }

    if (!new_wallet(private_spend_key, private_view_key, walletFileName, pwd_container.password())) {
      logger(ERROR, BRIGHT_RED) << "account creation failed";
      return false;
    }

    if (!writeAddressFile(walletAddressFile, m_wallet->getAddress(0)))
    {
      logger(WARNING, BRIGHT_RED) << "Couldn't write wallet address file: " + walletAddressFile;
    }
  } else {    
    m_wallet.reset(new WalletGreen(m_dispatcher, m_currency, *m_node, logManager));

    try
    {
      m_wallet->load(m_wallet_file_arg, pwd_container.password());
      m_wallet_file = m_wallet_file_arg;
      success_msg_writer(true) << "Wallet loaded";
    }
    catch (const std::exception &e)
    {
      fail_msg_writer() << "failed to load wallet: " << e.what();
      return false;
    }

    success_msg_writer() <<
      "**********************************************************************\n" <<
      "Use \"help\" command to see the list of available commands.\n" <<
      "**********************************************************************";
  }
  m_wallet->addObserver(this);
  return true;
}

//----------------------------------------------------------------------------------------------------
bool conceal_wallet::deinit()
{
  m_wallet->removeObserver(this);
  if (!m_wallet.get())
  {
    return true;
  }
  return close_wallet();
}
//----------------------------------------------------------------------------------------------------
void conceal_wallet::handle_command_line(const boost::program_options::variables_map& vm) {
  m_testnet = vm[arg_testnet.name].as<bool>();
  m_wallet_file_arg = command_line::get_arg(vm, arg_wallet_file);
  m_generate_new = command_line::get_arg(vm, arg_generate_new_wallet);
  m_daemon_address = command_line::get_arg(vm, arg_daemon_address);
  m_daemon_host = command_line::get_arg(vm, arg_daemon_host);
  m_daemon_port = command_line::get_arg(vm, arg_daemon_port);
  if (m_daemon_port == 0)
  {
    m_daemon_port = RPC_DEFAULT_PORT;
  }
  if (m_testnet && vm[arg_daemon_port.name].defaulted())
  {
    m_daemon_port = TESTNET_RPC_DEFAULT_PORT;
  }
}
//----------------------------------------------------------------------------------------------------
bool conceal_wallet::new_wallet(const std::string &wallet_file, const std::string& password) {
  m_wallet_file = wallet_file;

  m_wallet.reset(new WalletGreen(m_dispatcher, m_currency, *m_node, logManager));
  try {
    m_wallet->generateNewWallet(wallet_file, password);

    KeyPair viewKey = m_wallet->getViewKey();
    KeyPair spendKey = m_wallet->getAddressSpendKey(0);
    
    logger(INFO, BRIGHT_GREEN) << "ConcealWallet is an open-source, client-side, free wallet which allow you to send and receive CCX instantly on the blockchain. You are  in control of your funds & your keys. When you generate a new wallet, login, send, receive or deposit $CCX everything happens locally. Your seed is never transmitted, received or stored. That's why its imperative to write, print or save your seed somewhere safe. The backup of keys is your responsibility. If you lose your seed, your account can not be recovered. The Conceal Team doesn't take any responsibility for lost funds due to nonexistent/missing/lost private keys." << std::endl << std::endl;

    std::cout << "Wallet Address: " << m_wallet->getAddress(0) << std::endl;
    std::cout << "Private spend key: " << common::podToHex(spendKey.secretKey) << std::endl;
    std::cout << "Private view key: " <<  common::podToHex(viewKey.secretKey) << std::endl;
    std::cout << "Mnemonic Seed: " << mnemonics::privateKeyToMnemonic(spendKey.secretKey) << std::endl;
  }
  catch (const std::exception& e) {
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
bool conceal_wallet::new_wallet(const crypto::SecretKey &secret_key, const crypto::SecretKey &view_key, const std::string &wallet_file, const std::string& password) {
  m_wallet_file = wallet_file;

  m_wallet.reset(new WalletGreen(m_dispatcher, m_currency, *m_node, logManager));
  
  try
  {
    m_wallet->initializeWithViewKey(wallet_file, password, view_key);
    std::string address = m_wallet->createAddress(secret_key);
    logger(INFO, BRIGHT_WHITE) << "Imported wallet: " << address << std::endl;
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
bool conceal_wallet::close_wallet()
{
  m_wallet->save();
  logger(logging::INFO, logging::BRIGHT_GREEN) << "Closing wallet...";

  m_wallet->removeObserver(this);
  m_wallet->shutdown();

  return true;
}

//----------------------------------------------------------------------------------------------------
bool conceal_wallet::save(const std::vector<std::string> &)
{
  m_wallet->save();
  return true;
}

bool conceal_wallet::reset(const std::vector<std::string> &)
{
  m_wallet->removeObserver(this);
  {
    std::unique_lock<std::mutex> lock(m_walletSynchronizedMutex);
    m_walletSynchronized = false;
  }

  m_wallet->reset(0);
  m_wallet->addObserver(this);
  success_msg_writer(true) << "Reset completed successfully.";

  std::unique_lock<std::mutex> lock(m_walletSynchronizedMutex);
  m_walletSynchronizedCV.wait(lock, [this]
                              { return m_walletSynchronized; });

  std::cout << std::endl;

  return true;
}

bool conceal_wallet::get_reserve_proof(const std::vector<std::string> &args)
{
	if (args.size() != 1 && args.size() != 2)
  {
		fail_msg_writer() << "Usage: balance_proof (all|<amount>) [<message>]";
		return true;
	}

	uint64_t reserve = 0;
	if (args[0] != "all") {
		if (!m_currency.parseAmount(args[0], reserve)) {
			fail_msg_writer() << "amount is wrong: " << args[0];
			return true;
		}
	} else {
		reserve = m_wallet->getActualBalance();
	}

	try {
		const std::string sig_str = m_wallet->getReserveProof(m_wallet->getAddress(0), reserve, args.size() == 2 ? args[1] : "");
		
		logger(INFO, BRIGHT_WHITE) << "\n\n" << sig_str << "\n\n" << std::endl;

		const std::string filename = "balance_proof_" + args[0] + "_CCX.txt";
		boost::system::error_code ec;
		if (boost::filesystem::exists(filename, ec)) {
			boost::filesystem::remove(filename, ec);
		}

		std::ofstream proofFile(filename, std::ios::out | std::ios::trunc | std::ios::binary);
		if (!proofFile.good()) {
			return false;
		}
		proofFile << sig_str;

		success_msg_writer() << "signature file saved to: " << filename;

	} catch (const std::exception &e) {
		fail_msg_writer() << e.what();
	}

	return true;
}


bool conceal_wallet::get_tx_proof(const std::vector<std::string> &args)
{
  if(args.size() != 2 && args.size() != 3) {
    fail_msg_writer() << "Usage: get_tx_proof <txid> <dest_address> [<txkey>]";
    return true;
  }

  const std::string &str_hash = args[0];
  crypto::Hash txid;
  if (!parse_hash256(str_hash, txid)) {
    fail_msg_writer() << "Failed to parse txid";
    return true;
  }

  const std::string address_string = args[1];
  cn::AccountPublicAddress address;
  if (!m_currency.parseAccountAddressString(address_string, address)) {
     fail_msg_writer() << "Failed to parse address " << address_string;
     return true;
  }

  std::string sig_str;
  crypto::SecretKey tx_key = m_wallet->getTransactionDeterministicSecretKey(txid);
  bool r = tx_key != NULL_SECRET_KEY;
  crypto::SecretKey tx_key2;

  if (args.size() == 3)
  {
    crypto::Hash tx_key_hash;
    size_t size;
    if (!common::fromHex(args[2], &tx_key_hash, sizeof(tx_key_hash), size) || size != sizeof(tx_key_hash))
    {
      fail_msg_writer() << "failed to parse tx_key";
      return true;
    }
    tx_key2 = *(struct crypto::SecretKey *)&tx_key_hash;

    if (r && args.size() == 3 && tx_key != tx_key2)
    {
      fail_msg_writer() << "Tx secret key was found for the given txid, but you've also provided another tx secret key which doesn't match the found one.";
      return true;
    }

    tx_key = tx_key2;
  }
  else if (!r)
  {
    fail_msg_writer() << "Tx secret key wasn't found in the wallet file. Provide it as the optional third parameter if you have it elsewhere.";
    return true;
  }

  if (m_wallet->getTxProof(txid, address, tx_key, sig_str))
  {
    success_msg_writer() << "Signature: " << sig_str << std::endl;
  }

  return true;
}

void conceal_wallet::synchronizationCompleted(std::error_code result)
{
  std::unique_lock<std::mutex> lock(m_walletSynchronizedMutex);
  m_walletSynchronized = true;
  m_walletSynchronizedCV.notify_one();
}

void conceal_wallet::synchronizationProgressUpdated(uint32_t processedBlockCount, uint32_t totalBlockCount)
{
  std::unique_lock<std::mutex> lock(m_walletSynchronizedMutex);
  if (!m_walletSynchronized)
  {
    m_refresh_progress_reporter.update(processedBlockCount, false);
  }
}

bool conceal_wallet::show_balance(const std::vector<std::string> &args)
{
  if (!args.empty())
  {
    logger(ERROR) << "Usage: balance";
    return true;
  }
  std::stringstream balances = m_chelper.balances(*m_wallet, m_currency);
  logger(INFO) << balances.str();

  return true;
}

bool conceal_wallet::sign_message(const std::vector<std::string>& args)
{
  if(args.size() < 1)
  {
    fail_msg_writer() << "Use: sign_message <message>";
    return true;
  }
    
  KeyPair keys = m_wallet->getAddressSpendKey(0);

  crypto::Hash message_hash;
  crypto::Signature sig;
  crypto::cn_fast_hash(args[0].data(), args[0].size(), message_hash);
  crypto::generate_signature(message_hash, keys.publicKey, keys.secretKey, sig);
  
  success_msg_writer() << "Sig" << tools::base_58::encode(std::string(reinterpret_cast<char*>(&sig)));

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
  const std::string sig_prefix = "Sig";
  const size_t prefix_size = sig_prefix.size();
  
  if(encodedSig.substr(0, prefix_size) != sig_prefix)
  {
    fail_msg_writer() << "Invalid signature prefix";
    return true;
  } 
  
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
    success_msg_writer() << "Valid";
  else
    success_msg_writer() << "Invalid";
  return true;
}

/* ------------------------------------------------------------------------------------------- */

/* CREATE INTEGRATED ADDRESS */
/* take a payment Id as an argument and generate an integrated wallet address */

bool conceal_wallet::create_integrated(const std::vector<std::string>& args) 
{

  /* check if there is a payment id */
  if (args.empty()) 
  {

    fail_msg_writer() << "Please enter a payment ID";
    return true;
  }

  std::string paymentID = args[0];
  std::regex hexChars("^[0-9a-f]+$");
  if(paymentID.size() != 64 || !regex_match(paymentID, hexChars))
  {
    fail_msg_writer() << "Invalid payment ID";
    return true;
  }

  std::string address = m_wallet->getAddress(0);
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

  return true;
}

/* ---------------------------------------------------------------------------------------- */

bool conceal_wallet::export_keys(const std::vector<std::string> &)
{
  std::cout << get_wallet_keys();
  return true;
}
//----------------------------------------------------------------------------------------------------
bool conceal_wallet::show_incoming_transfers(const std::vector<std::string> &)
{
  bool hasTransfers = false;
  size_t transactionsCount = m_wallet->getTransactionCount();
  for (size_t trantransactionNumber = 0; trantransactionNumber < transactionsCount; ++trantransactionNumber)
  {
    WalletTransaction txInfo = m_wallet->getTransaction(trantransactionNumber);
    if (txInfo.totalAmount < 0)
    {
      continue;
    }
    hasTransfers = true;
    logger(INFO) << "        amount       \t                              tx id";
    logger(INFO, GREEN) << std::setw(21) << m_currency.formatAmount(txInfo.totalAmount) << '\t' << common::podToHex(txInfo.hash);
  }

  if (!hasTransfers)
  {
    success_msg_writer() << "No incoming transfers";
  }
  return true;
}

bool conceal_wallet::listTransfers(const std::vector<std::string>& args) {
  bool haveTransfers = false;
  bool haveBlockHeight = false;
  std::string blockHeightString = ""; 
  uint32_t blockHeight = 0;

  /* get block height from arguments */
  if (args.empty()) 
  {
    haveBlockHeight = false;
  } else {
    blockHeightString = args[0];
    haveBlockHeight = true;
    blockHeight = atoi(blockHeightString.c_str());
  }

  size_t transactionsCount = m_wallet->getTransactionCount();
  for (size_t transactionIndex = 0; transactionIndex < transactionsCount; ++transactionIndex) 
  {
    
    WalletTransaction transaction = m_wallet->getTransaction(transactionIndex);
    if (transaction.state != WalletTransactionState::SUCCEEDED || transaction.blockHeight == WALLET_UNCONFIRMED_TRANSACTION_HEIGHT) {
      continue;
    }

    if (!haveTransfers) {
      printListTransfersHeader(logger);
      haveTransfers = true;
    }

    if (haveBlockHeight == false) {
      printListTransfersItem(logger, transaction, *m_wallet, m_currency, transactionIndex);
    } else {
      if (transaction.blockHeight >= blockHeight) {
        printListTransfersItem(logger, transaction, *m_wallet, m_currency, transactionIndex);

      }

    }
  }

  if (!haveTransfers) {
    success_msg_writer() << "No transfers";
  }

  return true;
}

bool conceal_wallet::show_payments(const std::vector<std::string> &args) {
  if (args.empty()) {
    fail_msg_writer() << "expected at least one payment ID";
    return true;
  }

  try {
    auto hashes = args;
    std::sort(std::begin(hashes), std::end(hashes));
    hashes.erase(std::unique(std::begin(hashes), std::end(hashes)), std::end(hashes));
    std::vector<PaymentId> paymentIds;
    paymentIds.reserve(hashes.size());
    std::transform(std::begin(hashes), std::end(hashes), std::back_inserter(paymentIds), [](const std::string& arg) {
      PaymentId paymentId;
      if (!cn::parsePaymentId(arg, paymentId)) {
        throw std::runtime_error("payment ID has invalid format: \"" + arg + "\", expected 64-character string");
      }

      return paymentId;
    });

    logger(INFO) << "                            payment                             \t" <<
      "                          transaction                           \t" <<
      "  height\t       amount        ";

    auto payments = m_wallet->getTransactionsByPaymentIds(paymentIds);

    for (const auto& payment : payments) {
      for (const auto& transaction : payment.transactions) {
        success_msg_writer(true) <<
          common::podToHex(payment.paymentId) << '\t' <<
          common::podToHex(transaction.hash) << '\t' <<
          std::setw(8) << transaction.blockHeight << '\t' <<
          std::setw(21) << m_currency.formatAmount(transaction.totalAmount);
      }

      if (payment.transactions.empty()) {
        success_msg_writer() << "No payments with id " << common::podToHex(payment.paymentId);
      }
    }
  } catch (std::exception& e) {
    fail_msg_writer() << "show_payments exception: " << e.what();
  }

  return true;
}
//----------------------------------------------------------------------------------------------------
bool conceal_wallet::show_blockchain_height(const std::vector<std::string>& args) {
  try {
    uint64_t bc_height = m_node->getLastLocalBlockHeight();
    success_msg_writer() << bc_height;
  } catch (std::exception &e) {
    fail_msg_writer() << "failed to get blockchain height: " << e.what();
  }

  return true;
}
//----------------------------------------------------------------------------------------------------
bool conceal_wallet::show_num_unlocked_outputs(const std::vector<std::string> &)
{
  try
  {
    std::vector<TransactionOutputInformation> unlocked_outputs = m_wallet->getUnspentOutputs();
    success_msg_writer() << "Count: " << unlocked_outputs.size();
    for (const auto &out : unlocked_outputs)
    {
      success_msg_writer() << "Key: " << out.transactionPublicKey << " amount: " << m_currency.formatAmount(out.amount);
    }
  }
  catch (std::exception &e)
  {
    fail_msg_writer() << "failed to get outputs: " << e.what();
  }
  return true;
}
//----------------------------------------------------------------------------------------------------
bool conceal_wallet::optimize_outputs(const std::vector<std::string>& args) {
  try
  {
    cn::TransactionId tx = m_wallet->createOptimizationTransaction(m_wallet->getAddress(0));
    if (tx == WALLET_INVALID_TRANSACTION_ID)
    {
      fail_msg_writer() << "Can't send money";
      return false;
    }
    cn::WalletTransaction txInfo = m_wallet->getTransaction(tx);
    success_msg_writer(true) << "Money successfully sent, transaction " << common::podToHex(txInfo.hash);
  }
  catch (const std::system_error &e)
  {
    fail_msg_writer() << e.what();
    return false;
  }
  catch (const std::exception &e)
  {
    fail_msg_writer() << e.what();
    return false;
  }

  return true;
}

//----------------------------------------------------------------------------------------------------


bool conceal_wallet::optimize_all_outputs(const std::vector<std::string>& args) {

  uint64_t num_unlocked_outputs = 0;

  try {
    num_unlocked_outputs = m_wallet->getUnspentOutputsCount();
    success_msg_writer() << "Total outputs: " << num_unlocked_outputs;

  } catch (std::exception &e) {
    fail_msg_writer() << "failed to get outputs: " << e.what();
  }

  bool success = true;
  while (success && m_wallet->getUnspentOutputsCount() > 100)
  {
    success = optimize_outputs({});
  }
  return true;
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

/* This extracts the fee address from the remote node */
std::string conceal_wallet::getFeeAddress() {
  
  HttpClient httpClient(m_dispatcher, m_daemon_host, m_daemon_port);

  HttpRequest req;
  HttpResponse res;

  req.setUrl("/feeaddress");
  try {
	  httpClient.request(req, res);
  }
  catch (const std::exception& e) {
	  fail_msg_writer() << "Error connecting to the remote node: " << e.what();
  }

  if (res.getStatus() != HttpResponse::STATUS_200) {
	  fail_msg_writer() << "Remote node returned code " + std::to_string(res.getStatus());
  }

  std::string address;
  if (!processServerFeeAddressResponse(res.getBody(), address)) {
	  fail_msg_writer() << "Failed to parse remote node response";
  }

  return address;
}


bool conceal_wallet::transfer(const std::vector<std::string> &args) {
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
        std::copy(std::move_iterator<std::vector<WalletOrder>::iterator>(kv.second.begin()),
                  std::move_iterator<std::vector<WalletOrder>::iterator>(kv.second.end()),
                  std::back_inserter(cmd.dsts));
      }
    }

    std::vector<WalletMessage> messages;
    for (const auto &dst : cmd.dsts)
    {
      for (const auto &msg : cmd.messages)
      {
        messages.emplace_back(WalletMessage{dst.address, msg});
      }
    }

    uint64_t ttl = 0;
    if (cmd.ttl != 0) {
      ttl = static_cast<uint64_t>(time(nullptr)) + cmd.ttl;
    }

    cn::WalletHelper::SendCompleteResultObserver sent;

    std::string extraString;
    std::copy(cmd.extra.begin(), cmd.extra.end(), std::back_inserter(extraString));

    cmd.fake_outs_count = cn::parameters::MINIMUM_MIXIN;

    /* force minimum fee */
    if (cmd.fee < cn::parameters::MINIMUM_FEE_V2) {
      cmd.fee = cn::parameters::MINIMUM_FEE_V2;
    }

    cn::TransactionParameters sendParams;
    sendParams.destinations = cmd.dsts;
    sendParams.messages = messages;
    sendParams.extra = extraString;
    sendParams.unlockTimestamp = ttl;
    sendParams.changeDestination = m_wallet->getAddress(0);

    crypto::SecretKey transactionSK;
    size_t transactionId = m_wallet->transfer(sendParams, transactionSK);

    cn::WalletTransaction txInfo = m_wallet->getTransaction(transactionId);
    success_msg_writer(true) << "Money successfully sent, transaction hash: " << common::podToHex(txInfo.hash);
    success_msg_writer(true) << "Transaction secret key " << common::podToHex(transactionSK);
  } catch (const std::system_error& e) {
    fail_msg_writer() << e.what();
  } catch (const std::exception& e) {
    fail_msg_writer() << e.what();
  } catch (...) {
    fail_msg_writer() << "unknown error";
  }

  return true;
}
//----------------------------------------------------------------------------------------------------
bool conceal_wallet::run()
{
  {
    std::unique_lock<std::mutex> lock(m_walletSynchronizedMutex);
    while (!m_walletSynchronized)
    {
      m_walletSynchronizedCV.wait(lock, [this]
                                  { return m_walletSynchronized; });
    }
  }

  std::cout << std::endl;

  std::string addr_start = m_wallet->getAddress(0).substr(0, 10);
  m_consoleHandler.start(true, "[" + addr_start + "]: ", common::console::Color::BrightYellow);
  m_stopEvent.wait();
  return true;
}
//----------------------------------------------------------------------------------------------------
void conceal_wallet::stop()
{
  m_dispatcher.remoteSpawn([this]
                           { m_stopEvent.set(); });
  m_consoleHandler.requestStop();
}
//----------------------------------------------------------------------------------------------------
bool conceal_wallet::print_address(const std::vector<std::string> &)
{
  success_msg_writer() << m_wallet->getAddress(0);
  return true;
}
//----------------------------------------------------------------------------------------------------
bool conceal_wallet::process_command(const std::vector<std::string> &args) {
  return m_consoleHandler.runCommand(args);
}

void conceal_wallet::printConnectionError() const {
  fail_msg_writer() << "wallet failed to connect to daemon (" << m_daemon_address << ").";
}

std::string conceal_wallet::get_wallet_keys() const
{
  KeyPair viewKey = m_wallet->getViewKey();
  KeyPair spendKey = m_wallet->getAddressSpendKey(0);
  std::stringstream stream;

  stream << std::endl
         << "ConcealWallet is an open-source, client-side, free wallet which allow you to send and receive CCX instantly on the blockchain. You are  in control of your funds & your keys. When you generate a new wallet, login, send, receive or deposit $CCX everything happens locally. Your seed is never transmitted, received or stored. That's why its imperative to write, print or save your seed somewhere safe. The backup of keys is your responsibility. If you lose your seed, your account can not be recovered. The Conceal Team doesn't take any responsibility for lost funds due to nonexistent/missing/lost private keys." << std::endl
         << std::endl;

  stream << "Private spend key: " << common::podToHex(spendKey.secretKey) << std::endl;
  stream << "Private view key: " << common::podToHex(viewKey.secretKey) << std::endl;

  crypto::PublicKey unused_dummy_variable;
  crypto::SecretKey deterministic_private_view_key;

  AccountBase::generateViewFromSpend(spendKey.secretKey, deterministic_private_view_key, unused_dummy_variable);

  bool deterministic_private_keys = deterministic_private_view_key == viewKey.secretKey;

  /* dont show a mnemonic seed if it is an old non-deterministic wallet */
  if (deterministic_private_keys)
  {
    stream << "Mnemonic seed: " << mnemonics::privateKeyToMnemonic(spendKey.secretKey) << std::endl
           << std::endl;
  }
  return stream.str();
}

bool conceal_wallet::save_keys_to_file(const std::vector<std::string>& args)
{
  if (!args.empty())
  {
    logger(ERROR) <<  "Usage: \"save_keys\"";
    return true;
  }

  std::string backup_filename = common::RemoveExtension(m_wallet_file) + "_conceal_backup.txt";
  std::ofstream backup_file(backup_filename);

  backup_file << "Conceal Keys Backup" << std::endl
              << get_wallet_keys();

  logger(INFO, BRIGHT_GREEN) << "Wallet keys have been saved to the current folder where \"concealwallet\" is located as \""
    << backup_filename << "\".";

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

  /* get total txs in wallet */
  size_t tx_count = m_wallet->getTransactionCount();

  /* check wallet txs before doing work */
  if (tx_count == 0) {
    logger(ERROR, BRIGHT_RED) << "No transfers";
    return true;
  }

  /* tell user about prepped job */
  logger(INFO) << "Preparing file and transactions...";

  /* create filename and file */
  std::string tx_filename = common::RemoveExtension(m_wallet_file) + "_conceal_transactions.txt";
  std::ofstream tx_file(tx_filename);

  /* create header for listed txs */
  std::string header = common::makeCenteredString(32, "timestamp (UTC)") + " | ";
  header += common::makeCenteredString(64, "hash") + " | ";
  header += common::makeCenteredString(20, "total amount") + " | ";
  header += common::makeCenteredString(14, "fee") + " | ";
  header += common::makeCenteredString(8, "block") + " | ";
  header += common::makeCenteredString(12, "unlock time");

  /* make header string to ss so we can use .size() */
  std::stringstream hs(header);
  std::stringstream line(std::string(header.size(), '-'));

  /* push header to start of file */
  tx_file << hs.str() << "\n" << line.str() << "\n";

  /* create line from string */
  std::string listed_tx;

  /* go through tx ids for the amount of transactions in wallet */
  for (TransactionId i = 0; i < tx_count; ++i) 
  {
    /* get tx struct */
    WalletTransaction txInfo;
    /* get tx to list from i */
    txInfo = m_wallet->getTransaction(i);

    /* check tx state */
    if (txInfo.state != WalletTransactionState::SUCCEEDED || txInfo.blockHeight == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT) {
      continue;
    }

    /* grab tx info */
    std::string formatted_item_tx = m_chelper.list_tx_item(txInfo, listed_tx, m_currency);

    /* push info to end of file */
    tx_file << formatted_item_tx;

    /* tell user about progress */
    logger(INFO) << "Transaction: " << i << " was pushed to " << tx_filename;
  }

  /* tell user job complete */
  logger(INFO, BRIGHT_GREEN) << "All transactions have been saved to the current folder where \"concealwallet\" is located as \""
    << tx_filename << "\".";
  
  /* if user uses "save_txs_to_file true" then we go through the deposits */
  if (include_deposits == true)
  {
    /* get total deposits in wallet */
    size_t deposit_count = m_wallet->getWalletDepositCount();

    /* check wallet txs before doing work */
    if (deposit_count == 0) {
      logger(ERROR, BRIGHT_RED) << "No deposits";
      return true;
    }

    /* tell user about prepped job */
    logger(INFO) << "Preparing deposits...";

    /* create new header for listed deposits */
    std::string headerd = common::makeCenteredString(8, "ID") + " | ";
    headerd += common::makeCenteredString(20, "Amount") + " | ";
    headerd += common::makeCenteredString(20, "Interest") + " | ";
    headerd += common::makeCenteredString(16, "Height") + " | ";
    headerd += common::makeCenteredString(16, "Unlock Height") + " | ";
    headerd += common::makeCenteredString(10, "State");

    /* make header string to ss so we can use .size() */
    std::stringstream hds(headerd);
    std::stringstream lined(std::string(headerd.size(), '-'));

    /* push new header to start of file with an extra new line */
    tx_file << "\n\n" << hds.str() << "\n" << lined.str() << "\n";

    /* create line from string */
    std::string listed_deposit;

    /* go through deposits ids for the amount of deposits in wallet */
    for (DepositId id = 0; id < deposit_count; ++id)
    {
      /* get deposit info from id and store it to deposit */
      Deposit deposit = m_wallet->getDeposit(id);
      cn::WalletTransaction txInfo;

      /* get deposit info and use its transaction in the chain */
      txInfo = m_wallet->getTransaction(deposit.creatingTransactionId);

      /* grab deposit info */
      std::string formatted_item_d = m_chelper.list_deposit_item(txInfo, deposit, listed_deposit, id, m_currency);

      /* push info to end of file */
      tx_file << formatted_item_d;

      /* tell user about progress */
      logger(INFO) << "Deposit: " << id << " was pushed to " << tx_filename;
    }

    /* tell user job complete */
    logger(INFO, BRIGHT_GREEN) << "All deposits have been saved to the end of the file current folder where \"concealwallet\" is located as \""
      << tx_filename << "\".";
  }

  return true;
}

bool conceal_wallet::list_deposits(const std::vector<std::string> &)
{
  bool haveDeposits = m_wallet->getWalletDepositCount() > 0;

  if (!haveDeposits)
  {
    success_msg_writer() << "No deposits";
    return true;
  }

  printListDepositsHeader(logger);

  /* go through deposits ids for the amount of deposits in wallet */
  for (DepositId id = 0; id < m_wallet->getWalletDepositCount(); ++id)
  {
    /* get deposit info from id and store it to deposit */
    Deposit deposit = m_wallet->getDeposit(id);
    cn::WalletTransaction txInfo = m_wallet->getTransaction(deposit.creatingTransactionId);

    logger(INFO) << m_chelper.get_deposit_info(deposit, id, m_currency, txInfo.blockHeight);
  }

  return true;
}

bool conceal_wallet::deposit(const std::vector<std::string> &args)
{
  if (args.size() != 2)
  {
    logger(ERROR) << "Usage: deposit <months> <amount>";
    return true;
  }

  try
  {
    /**
     * Change arg to uint64_t using boost then
     * multiply by min_term so user can type in months
    **/
    uint64_t min_term = m_currency.depositMinTermV3();
    uint64_t max_term = m_currency.depositMaxTermV3();
    uint64_t deposit_term = boost::lexical_cast<uint32_t>(args[0]) * min_term;

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

    if (deposit_amount < m_currency.depositMinAmount())
    {
      logger(ERROR, BRIGHT_RED) << "Deposit amount is too small, min=" << m_currency.depositMinAmount()
                                << ", given=" << m_currency.formatAmount(deposit_amount);
      return true;
    }

    if (!m_chelper.confirm_deposit(deposit_term, deposit_amount, m_testnet, m_currency, logger))
    {
      logger(ERROR) << "Deposit is not being created.";
      return true;
    }

    std::string address = m_wallet->getAddress(0);
    std::string tx_hash;
    logger(INFO) << "Creating deposit...";

    m_wallet->createDeposit(deposit_amount, deposit_term, address, address, tx_hash);
    success_msg_writer(true) << "Money successfully sent, transaction hash: " << tx_hash;
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
  if (args.size() != 1)
  {
    logger(ERROR) << "Usage: withdraw <id>";
    return true;
  }

  try
  {
    if (m_wallet->getWalletDepositCount() == 0)
    {
      logger(ERROR) << "No deposits have been made in this wallet.";
      return true;
    }

    uint64_t deposit_id = boost::lexical_cast<uint64_t>(args[0]);
    std::string tx_hash;
    m_wallet->withdrawDeposit(deposit_id, tx_hash);
    success_msg_writer(true) << "Money successfully sent, transaction hash: " << tx_hash;
  }
  catch (std::exception &e)
  {
    fail_msg_writer() << "Failed to withdraw deposit: " << e.what();
  }

  return true;
}

bool conceal_wallet::deposit_info(const std::vector<std::string> &args)
{
  if (args.size() != 1)
  {
    logger(ERROR) << "Usage: withdraw <id>";
    return true;
  }
  uint64_t deposit_id = boost::lexical_cast<uint64_t>(args[0]);
  cn::Deposit deposit;
  try
  {
    deposit = m_wallet->getDeposit(deposit_id);
  }
  catch (const std::exception &e)
  {
    logger(ERROR, BRIGHT_RED) << "Error: " << e.what();
    return false;
  }
  cn::WalletTransaction txInfo = m_wallet->getTransaction(deposit.creatingTransactionId);

  logger(INFO) << m_chelper.get_full_deposit_info(deposit, deposit_id, m_currency, txInfo.blockHeight);

  return true;
}

bool conceal_wallet::check_address(const std::vector<std::string> &args)
{
  if (args.size() != 1) 
  {
    logger(ERROR) << "Usage: check_address <address>";
    return true;
  }

  const std::string addr = args[0];

  if (!cn::validateAddress(addr, m_currency))
  {
    logger(ERROR) << "Invalid wallet address: " << addr;
    return true;
  }

  logger(INFO) << "The wallet " << addr << " seems to be valid, please still be cautious still.";

  return true;
}

void conceal_wallet::initCompleted(std::error_code result)
{
  // Nothing to do here
}
void conceal_wallet::saveCompleted(std::error_code result)
{
  // Nothing to do here
}
void conceal_wallet::actualBalanceUpdated(uint64_t balance)
{
  // Nothing to do here
}
void conceal_wallet::pendingBalanceUpdated(uint64_t balance)
{
  // Nothing to do here
}
void conceal_wallet::actualDepositBalanceUpdated(uint64_t balance)
{
  // Nothing to do here
}
void conceal_wallet::pendingDepositBalanceUpdated(uint64_t balance)
{
  // Nothing to do here
}
void conceal_wallet::actualInvestmentBalanceUpdated(uint64_t balance)
{
  // Nothing to do here
}
void conceal_wallet::pendingInvestmentBalanceUpdated(uint64_t balance)
{
  // Nothing to do here
}
void conceal_wallet::externalTransactionCreated(TransactionId transactionId)
{
  // Nothing to do here
}
void conceal_wallet::sendTransactionCompleted(TransactionId transactionId, std::error_code result)
{
  // Nothing to do here
}
void conceal_wallet::transactionUpdated(TransactionId transactionId)
{
  // Nothing to do here
}
void conceal_wallet::depositUpdated(DepositId depositId)
{
  // Nothing to do here
}
void conceal_wallet::depositsUpdated(const std::vector<DepositId> &depositIds)
{
  // Nothing to do here
}
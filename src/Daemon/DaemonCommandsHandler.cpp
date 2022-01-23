// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2022 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "DaemonCommandsHandler.h"

#include "P2p/NetNode.h"
#include "CryptoNoteCore/Miner.h"
#include "CryptoNoteCore/Core.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "Serialization/SerializationTools.h"
#include "version.h"

namespace
{
  template <typename T>
  static bool print_as_json(const T &obj)
  {
    logging::LoggerManager json_m;
    logging::LoggerRef json_log(json_m, "[JSON]");

    json_log() << " " << cn::storeToJson(obj);

    return true;
  }
} // namespace

DaemonCommandsHandler::DaemonCommandsHandler(cn::core &core, cn::NodeServer &srv, logging::LoggerManager &log) : m_core(core), m_srv(srv), logger(log, "daemon"), m_logManager(log)
{
  m_consoleHandler.setHandler("exit", boost::bind(&DaemonCommandsHandler::exit, this, boost::arg<1>()), "Shutdown the daemon");
  m_consoleHandler.setHandler("help", boost::bind(&DaemonCommandsHandler::help, this, boost::arg<1>()), "Show this help");
  m_consoleHandler.setHandler("save", boost::bind(&DaemonCommandsHandler::save, this, boost::arg<1>()), "Save the Blockchain data safely");
  m_consoleHandler.setHandler("print_pl", boost::bind(&DaemonCommandsHandler::print_pl, this, boost::arg<1>()), "Print peer list");
  m_consoleHandler.setHandler("rollback_chain", boost::bind(&DaemonCommandsHandler::rollback_chain, this, boost::arg<1>()), "Rollback chain to specific height, rollback_chain <height>");
  m_consoleHandler.setHandler("print_cn", boost::bind(&DaemonCommandsHandler::print_cn, this, boost::arg<1>()), "Print connections");
  m_consoleHandler.setHandler("print_bc", boost::bind(&DaemonCommandsHandler::print_bc, this, boost::arg<1>()), "Print blockchain info in a given blocks range, print_bc <begin_height> [<end_height>]");
  m_consoleHandler.setHandler("print_block", boost::bind(&DaemonCommandsHandler::print_block, this, boost::arg<1>()), "Print block, print_block <block_hash> | <block_height>");
  m_consoleHandler.setHandler("print_stat", boost::bind(&DaemonCommandsHandler::print_stat, this, boost::arg<1>()), "Print statistics, print_stat <nothing=last> | <block_hash> | <block_height>");
  m_consoleHandler.setHandler("print_tx", boost::bind(&DaemonCommandsHandler::print_tx, this, boost::arg<1>()), "Print transaction, print_tx <transaction_hash>");
  m_consoleHandler.setHandler("start_mining", boost::bind(&DaemonCommandsHandler::start_mining, this, boost::arg<1>()), "Start mining for specified address, start_mining <addr> [threads=1]");
  m_consoleHandler.setHandler("stop_mining", boost::bind(&DaemonCommandsHandler::stop_mining, this, boost::arg<1>()), "Stop mining");
  m_consoleHandler.setHandler("print_pool", boost::bind(&DaemonCommandsHandler::print_pool, this, boost::arg<1>()), "Print transaction pool (long format)");
  m_consoleHandler.setHandler("print_pool_sh", boost::bind(&DaemonCommandsHandler::print_pool_sh, this, boost::arg<1>()), "Print transaction pool (short format)");
  m_consoleHandler.setHandler("show_hr", boost::bind(&DaemonCommandsHandler::show_hr, this, boost::arg<1>()), "Start showing hash rate");
  m_consoleHandler.setHandler("hide_hr", boost::bind(&DaemonCommandsHandler::hide_hr, this, boost::arg<1>()), "Stop showing hash rate");
  m_consoleHandler.setHandler("set_log", boost::bind(&DaemonCommandsHandler::set_log, this, boost::arg<1>()), "set_log <level> - Change current log level, <level> is a number 0-4");
}

//--------------------------------------------------------------------------------
const std::string DaemonCommandsHandler::get_commands_str()
{
  std::stringstream ss;
  ss << cn::CRYPTONOTE_NAME << " v" << PROJECT_VERSION_LONG << ENDL;
  ss << "Commands: " << ENDL;
  std::string usage = m_consoleHandler.getUsage();
  boost::replace_all(usage, "\n", "\n  ");
  usage.insert(0, "  ");
  ss << usage << ENDL;
  return ss.str();
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::exit(const std::vector<std::string> &args)
{
  if (!args.empty())
  {
    logger(logging::ERROR) << "Usage: \"exit\"";
    return true;
  }
  
  m_consoleHandler.requestStop();
  m_srv.sendStopSignal();

  return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::help(const std::vector<std::string> &args)
{
  if (!args.empty())
  {
    logger(logging::ERROR) << "Usage: \"help\"";
    return true;
  }

  logger(logging::INFO) << get_commands_str();
  return true;
}
//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::save(const std::vector<std::string> &args)
{
  if (!args.empty())
  {
    logger(logging::ERROR) << "Usage: \"save\"";
    return true;
  }

  return m_core.saveBlockchain();
}
//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_pl(const std::vector<std::string> &args)
{
  if (!args.empty())
  {
    logger(logging::ERROR) << "Usage: \"print_pl\"";
    return true;
  }

  m_srv.log_peerlist();

  return true;
}
//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::show_hr(const std::vector<std::string> &args)
{
  if (!args.empty())
  {
    logger(logging::ERROR) << "Usage: \"show_hr\"";
    return true;
  }

  if (!m_core.get_miner().is_mining())
  {
    logger(logging::INFO) << "Mining is not started. You need to start mining before you can see hash rate.";
  }
  else
  {
    m_core.get_miner().do_print_hashrate(true);
  }

  return true;
}
//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::hide_hr(const std::vector<std::string> &args)
{
  if (!args.empty())
  {
    logger(logging::ERROR) << "Usage: \"hide_hr\"";
    return true;
  }

  m_core.get_miner().do_print_hashrate(false);

  return true;
}
//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_bc_outs(const std::vector<std::string> &args)
{
  // TODO implement this command
  if (args.size() != 1)
  {
    logger(logging::ERROR) << "need file path as parameter";
    return true;
  }

  m_core.print_blockchain_outs(args[0]);

  return true;
}
//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_cn(const std::vector<std::string> &args)
{
  if (!args.empty())
  {
    logger(logging::ERROR) << "Usage: \"print_cn\"";
    return true;
  }

  m_srv.get_payload_object().log_connections();

  return true;
}
//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_bc(const std::vector<std::string> &args)
{
  if (!args.size())
  {
    logger(logging::ERROR) << "Usage: \"print_bc <block_from> [<block_to>]\"";
    return false;
  }

  uint32_t start_index = 0;
  uint32_t end_index = 0;
  uint32_t end_block_parametr = m_core.get_current_blockchain_height();
  if (!common::fromString(args[0], start_index))
  {
    logger(logging::ERROR) << "wrong starter block index parameter";
    return false;
  }

  if (args.size() > 1 && !common::fromString(args[1], end_index))
  {
    logger(logging::ERROR) << "wrong end block index parameter";
    return false;
  }

  if (end_index == 0)
  {
    end_index = end_block_parametr;
  }

  if (end_index > end_block_parametr)
  {
    logger(logging::ERROR) << "end block index parameter shouldn't be greater than " << end_block_parametr;
    return false;
  }

  if (end_index <= start_index)
  {
    logger(logging::ERROR) << "end block index should be greater than starter block index";
    return false;
  }

  m_core.print_blockchain(start_index, end_index);

  return true;
}
//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_bci(const std::vector<std::string> &args)
{
  // TODO implement this command
  if (!args.empty())
  {
    logger(logging::ERROR) << "Usage: \"print_bci\"";
    return true;
  }

  m_core.print_blockchain_index();

  return true;
}

bool DaemonCommandsHandler::set_log(const std::vector<std::string> &args)
{
  if (args.size() != 1)
  {
    logger(logging::ERROR) << "Usage: \"set_log <0-5>\" 0-5 being log level.";
    return true;
  }

  uint16_t l = 0;
  if (!common::fromString(args[0], l))
  {
    logger(logging::ERROR) << "wrong number format, use: set_log <log_level_number_0-4>";
    return true;
  }

  ++l;

  if (l > logging::TRACE)
  {
    logger(logging::ERROR) << "wrong number range, use: set_log <log_level_number_0-4>";
    return true;
  }

  m_logManager.setMaxLevel(static_cast<logging::Level>(l));

  return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_block_by_height(uint32_t height)
{
  std::list<cn::Block> blocks;
  m_core.get_blocks(height, 1, blocks);

  if (1 == blocks.size())
  {
    logger(logging::INFO) << "block_id: " << get_block_hash(blocks.front());
    print_as_json(blocks.front());
  }
  else
  {
    uint32_t current_height;
    crypto::Hash top_id;
    m_core.get_blockchain_top(current_height, top_id);

    logger(logging::ERROR) << "block wasn't found. Current block chain height: "
      << current_height << ", requested: " << height;

    return false;
  }

  return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::rollback_chain(const std::vector<std::string> &args)
{
  if (args.empty())
  {
    logger(logging::ERROR) << "Usage: \"rollback_chain <block_height>\"";
    return true;
  }

  const std::string &arg = args.front();
  uint32_t height = boost::lexical_cast<uint32_t>(arg);
  rollbackchainto(height);
  return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::rollbackchainto(uint32_t height)
{
  m_core.rollback_chain_to(height);
  return true;
}
//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_block_by_hash(const std::string &arg)
{
  crypto::Hash block_hash;
  if (!parse_hash256(arg, block_hash))
  {
    return false;
  }

  std::list<crypto::Hash> block_ids;
  block_ids.push_back(block_hash);
  std::list<cn::Block> blocks;
  std::list<crypto::Hash> missed_ids;
  m_core.get_blocks(block_ids, blocks, missed_ids);

  if (1 == blocks.size())
  {
    print_as_json(blocks.front());
  }
  else
  {
    logger(logging::ERROR) << "block wasn't found: " << arg;
    return false;
  }

  return true;
}
//--------------------------------------------------------------------------------
uint64_t DaemonCommandsHandler::calculatePercent(const cn::Currency &currency, uint64_t value, uint64_t total)
{
  double to_calc = (double)currency.coin() * static_cast<double>(value) / static_cast<double>(total);
  return static_cast<uint64_t>(100.0 * to_calc);
}
//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_stat(const std::vector<std::string> &args)
{
  uint32_t height = 0;
  uint32_t maxHeight = m_core.get_current_blockchain_height() - 1;
  if (args.empty())
  {
    height = maxHeight;
  }
  else
  {
    try
    {
      height = boost::lexical_cast<uint32_t>(args.front());
    }
    catch (boost::bad_lexical_cast &)
    {
      crypto::Hash block_hash;
      if (!parse_hash256(args.front(), block_hash) || !m_core.getBlockHeight(block_hash, height))
      {
        return false;
      }
    }
    if (height > maxHeight)
    {
      std::cout << "printing for last available block: " << maxHeight << std::endl;
      height = maxHeight;
    }
  }

  uint64_t totalCoinsInNetwork = m_core.coinsEmittedAtHeight(height);
  uint64_t totalCoinsOnDeposits = m_core.depositAmountAtHeight(height);
  uint64_t amountOfActiveCoins = totalCoinsInNetwork - totalCoinsOnDeposits;

  const auto &currency = m_core.currency();

  std::string print_status = "\n";
  
  print_status += "Block Height: " + std::to_string(height) + "\n";
  print_status += "Block Difficulty: " + std::to_string(m_core.difficultyAtHeight(height)) + "\n";
  print_status += "Coins Minted (Total Supply):  " + currency.formatAmount(totalCoinsInNetwork) + "\n";
  print_status += "Deposits (Locked Coins): " + currency.formatAmount(totalCoinsOnDeposits) + " (" + currency.formatAmount(calculatePercent(currency, totalCoinsOnDeposits, totalCoinsInNetwork)) + "%)\n";
  print_status += "Active Coins (Circulation Supply):  " + currency.formatAmount(amountOfActiveCoins) + " (" + currency.formatAmount(calculatePercent(currency, amountOfActiveCoins, totalCoinsInNetwork)) + "%)\n";
  print_status += "Rewards (Paid Interest): " + currency.formatAmount(m_core.depositInterestAtHeight(height)) + "\n";

  logger(logging::INFO) << print_status;

  return true;
}
//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_block(const std::vector<std::string> &args)
{
  if (args.empty())
  {
    std::cout << "expected: print_block (<block_hash> | <block_height>)" << std::endl;
    return true;
  }

  const std::string &arg = args.front();
  try
  {
    uint32_t height = boost::lexical_cast<uint32_t>(arg);
    print_block_by_height(height);
  }
  catch (boost::bad_lexical_cast &)
  {
    print_block_by_hash(arg);
  }

  return true;
}
//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_tx(const std::vector<std::string> &args)
{
  if (args.empty())
  {
    std::cout << "expected: print_tx <transaction hash>" << std::endl;
    return true;
  }

  const std::string &str_hash = args.front();
  crypto::Hash tx_hash;
  if (!parse_hash256(str_hash, tx_hash))
  {
    return true;
  }

  std::vector<crypto::Hash> tx_ids;
  tx_ids.push_back(tx_hash);
  std::list<cn::Transaction> txs;
  std::list<crypto::Hash> missed_ids;
  m_core.getTransactions(tx_ids, txs, missed_ids, true);

  if (1 == txs.size())
  {
    print_as_json(txs.front());
  }
  else
  {
    std::cout << "transaction wasn't found: <" << str_hash << '>' << std::endl;
  }

  return true;
}
//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_pool(const std::vector<std::string> &args)
{
  if (!args.empty())
  {
    logger(logging::ERROR) << "Usage: \"print_pool\"";
    return true;
  }

  logger(logging::INFO) << "Pool state: " << ENDL << m_core.print_pool(false);

  return true;
}
//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::print_pool_sh(const std::vector<std::string> &args)
{
  if (!args.empty())
  {
    logger(logging::ERROR) << "Usage: \"print_pool_sh\"";
    return true;
  }

  logger(logging::INFO) << "Pool state: " << ENDL << m_core.print_pool(true);

  return true;
}
//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::start_mining(const std::vector<std::string> &args)
{
  if (!args.size())
  {
    std::cout << "Please, specify wallet address to mine for: start_mining <addr> [threads=1]" << std::endl;
    return true;
  }

  cn::AccountPublicAddress adr;
  if (!m_core.currency().parseAccountAddressString(args.front(), adr))
  {
    std::cout << "target account address has wrong format" << std::endl;
    return true;
  }

  size_t threads_count = 1;
  if (args.size() > 1)
  {
    bool ok = common::fromString(args[1], threads_count);
    threads_count = (ok && 0 < threads_count) ? threads_count : 1;
  }

  m_core.get_miner().start(adr, threads_count);
  return true;
}

//--------------------------------------------------------------------------------
bool DaemonCommandsHandler::stop_mining(const std::vector<std::string> &args)
{
  if (!args.empty())
  {
    logger(logging::ERROR) << "Usage: \"stop_mining\"";
    return true;
  }

  m_core.get_miner().stop();

  return true;
}


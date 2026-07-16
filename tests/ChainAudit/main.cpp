// Copyright (c) 2018-2026 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license.

#include "ChainAudit/ChainAudit.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <list>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <boost/variant/get.hpp>

#include "Common/Math.h"
#include "Common/StringTools.h"
#include "Common/Util.h"
#include "CryptoNoteCore/Checkpoints.h"
#include "CryptoNoteCore/Core.h"
#include "CryptoNoteCore/CoreConfig.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/MinerConfig.h"
#include "Logging/LoggerManager.h"

namespace
{
  struct Config
  {
    std::string dataDir;
    std::string outFile;
    bool testnet = false;
    bool includeInfo = false;
    uint32_t startHeight = 0;
    uint32_t endHeight = 0;
  };

  void usage()
  {
    std::cout
        << "conceal-chain-audit --data-dir <path> [--start N] [--end N] [--testnet] [--include-info] [--out file]\n"
        << "\n"
        << "Read-only monetary scanner for local Conceal chain data.\n"
        << "Classic SwappedVector data dirs only; refuses --data-dir that contains mdbx_blocks.\n";
  }

  bool parseUint32(const std::string &value, uint32_t &out)
  {
    uint64_t parsed = 0;
    if (!common::fromString(value, parsed) || parsed > UINT32_MAX)
    {
      return false;
    }
    out = static_cast<uint32_t>(parsed);
    return true;
  }

  Config parseArgs(int argc, char **argv)
  {
    Config cfg;
    cfg.dataDir = tools::getDefaultDataDirectory(false);

    for (int i = 1; i < argc; ++i)
    {
      std::string arg(argv[i]);
      if (arg == "--help" || arg == "-h")
      {
        usage();
        std::exit(0);
      }
      else if (arg == "--testnet")
      {
        cfg.testnet = true;
        if (cfg.dataDir == tools::getDefaultDataDirectory(false))
        {
          cfg.dataDir = tools::getDefaultDataDirectory(true);
        }
      }
      else if (arg == "--include-info")
      {
        cfg.includeInfo = true;
      }
      else if ((arg == "--data-dir" || arg == "--out" || arg == "--start" ||
                arg == "--end") &&
               i + 1 >= argc)
      {
        throw std::runtime_error(arg + " requires a value");
      }
      else if (arg == "--data-dir")
      {
        cfg.dataDir = argv[++i];
      }
      else if (arg == "--out")
      {
        cfg.outFile = argv[++i];
      }
      else if (arg == "--start")
      {
        if (!parseUint32(argv[++i], cfg.startHeight))
        {
          throw std::runtime_error("invalid --start value");
        }
      }
      else if (arg == "--end")
      {
        if (!parseUint32(argv[++i], cfg.endHeight))
        {
          throw std::runtime_error("invalid --end value");
        }
      }
      else
      {
        throw std::runtime_error("unknown argument: " + arg);
      }
    }

    return cfg;
  }

  std::string jsonEscape(const std::string &input)
  {
    std::ostringstream out;
    for (char c : input)
    {
      switch (c)
      {
      case '\\':
        out << "\\\\";
        break;
      case '"':
        out << "\\\"";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        out << c;
        break;
      }
    }
    return out.str();
  }

  std::string findingJson(const cn::chain_audit::Finding &finding)
  {
    std::ostringstream out;
    out << "{"
        << "\"severity\":\"" << cn::chain_audit::severityName(finding.severity) << "\","
        << "\"code\":\"" << finding.code << "\","
        << "\"height\":" << finding.height << ","
        << "\"tx_index\":" << finding.txIndex << ","
        << "\"output_index\":" << finding.outputIndex << ","
        << "\"block_hash\":\"" << common::podToHex(finding.blockHash) << "\","
        << "\"tx_hash\":\"" << common::podToHex(finding.txHash) << "\","
        << "\"message\":\"" << jsonEscape(finding.message) << "\""
        ;
    if (finding.hasAmountDelta)
    {
      out << ",\"actual_amount_atomic\":" << finding.actualAmount
          << ",\"expected_amount_atomic\":" << finding.expectedAmount
          << ",\"delta_amount_atomic\":" << finding.deltaAmount
          << ",\"consensus_tolerance_atomic\":"
          << finding.consensusTolerance
          << ",\"consensus_accepted\":"
          << (finding.consensusAccepted ? "true" : "false");
    }
    out << "}";
    return out.str();
  }

  bool shouldEmit(const cn::chain_audit::Finding &finding, bool includeInfo)
  {
    return includeInfo || finding.severity != cn::chain_audit::Severity::Info;
  }

  bool vectorFromList(const std::list<cn::Transaction> &in,
                      std::vector<cn::Transaction> &out)
  {
    out.clear();
    out.reserve(in.size());
    for (const auto &tx : in)
    {
      out.push_back(tx);
    }
    return true;
  }

  void auditCheckpointReferences(cn::core &core, const cn::Transaction &tx,
                                 uint32_t height, uint32_t txIndex,
                                 const crypto::Hash &blockHash,
                                 std::vector<cn::chain_audit::Finding> &findings)
  {
    const crypto::Hash txHash = cn::getObjectHash(tx);
    for (const auto &input : tx.inputs)
    {
      if (input.type() == typeid(cn::KeyInput))
      {
        std::list<std::pair<crypto::Hash, size_t>> refs;
        if (!core.scanOutputkeysForIndices(boost::get<cn::KeyInput>(input), refs))
        {
          cn::chain_audit::Finding finding;
          finding.severity = cn::chain_audit::Severity::Critical;
          finding.code = "CHECKPOINT_KEY_INPUT_BAD_REFERENCE";
          finding.height = height;
          finding.txIndex = txIndex;
          finding.outputIndex = 0;
          finding.blockHash = blockHash;
          finding.txHash = txHash;
          finding.message = "checkpoint-zone KeyInput fails structural output reference lookup";
          findings.push_back(finding);
        }
      }
      else if (input.type() == typeid(cn::MultisignatureInput))
      {
        std::pair<crypto::Hash, size_t> ref;
        if (!core.getMultisigOutputReference(boost::get<cn::MultisignatureInput>(input), ref))
        {
          cn::chain_audit::Finding finding;
          finding.severity = cn::chain_audit::Severity::Critical;
          finding.code = "CHECKPOINT_MULTISIG_INPUT_BAD_REFERENCE";
          finding.height = height;
          finding.txIndex = txIndex;
          finding.outputIndex = 0;
          finding.blockHash = blockHash;
          finding.txHash = txHash;
          finding.message = "checkpoint-zone MultisignatureInput fails deposit-cell reference lookup";
          findings.push_back(finding);
        }
      }
    }
  }
}

int main(int argc, char **argv)
{
  try
  {
    Config cfg = parseArgs(argc, argv);

    logging::LoggerManager logger;
    cn::Currency currency = cn::CurrencyBuilder(logger).testnet(cfg.testnet).currency();
    cn::core core(currency, nullptr, logger, false, false);

    cn::CoreConfig coreConfig;
    coreConfig.configFolder = cfg.dataDir;
    coreConfig.configFolderDefaulted = false;
    coreConfig.testnet = cfg.testnet;

    cn::Checkpoints checkpoints(logger);
    checkpoints.set_testnet(cfg.testnet);
    checkpoints.load_checkpoints();
    const std::vector<uint32_t> checkpointHeights = checkpoints.getCheckpointHeights();
    const uint32_t checkpointZoneEnd =
        checkpointHeights.empty() ? 0 : checkpointHeights.back();
    core.set_checkpoints(std::move(checkpoints));

    // This SwappedVector build must not open MDBX data dirs: core.init() would create/overwrite
    // classic blocks.dat beside mdbx_blocks (empty chain / height 1) instead of reading MDBX.
    {
      const std::string mdbxBlocks = cfg.dataDir + "/mdbx_blocks";
      std::ifstream mdbxProbe(mdbxBlocks.c_str());
      if (mdbxProbe.good())
      {
        std::cerr
            << mdbxBlocks << " found. \n"
            << "This conceal-chain-audit build uses SwappedVector storage; "
               "not suitable for an MDBX chain. "
               "Point --data-dir at a classic SwappedVector data dir."
            << std::endl;
        return 2;
      }
    }

    cn::MinerConfig minerConfig;
    if (!core.init(coreConfig, minerConfig, true))
    {
      std::cerr << "failed to open chain at " << cfg.dataDir << std::endl;
      return 2;
    }

    const uint32_t chainHeight = core.get_current_blockchain_height();
    if (chainHeight == 0)
    {
      std::cerr << "chain is empty at " << cfg.dataDir << std::endl;
      return 2;
    }

    uint32_t endHeight = cfg.endHeight == 0 ? chainHeight - 1 : cfg.endHeight;
    if (cfg.startHeight >= chainHeight || endHeight >= chainHeight ||
        cfg.startHeight > endHeight)
    {
      std::cerr << "invalid scan range for chain height " << chainHeight << std::endl;
      return 2;
    }

    std::vector<size_t> recentBlockSizes;
    recentBlockSizes.reserve(currency.rewardBlocksWindow());

    uint64_t previousGeneratedCoins = 0;
    if (cfg.startHeight > 0)
    {
      std::list<cn::Block> previousBlocks;
      std::list<cn::Transaction> previousTxsList;
      if (!core.get_blocks(cfg.startHeight - 1, 1, previousBlocks,
                           previousTxsList) ||
          previousBlocks.empty())
      {
        std::cerr << "failed to read block " << (cfg.startHeight - 1)
                  << std::endl;
        return 2;
      }
      const crypto::Hash previousBlockHash =
          cn::get_block_hash(previousBlocks.front());
      if (!core.getAlreadyGeneratedCoins(previousBlockHash,
                                         previousGeneratedCoins))
      {
        std::cerr << "failed to read emitted coins for block "
                  << (cfg.startHeight - 1) << std::endl;
        return 2;
      }
    }

    const uint32_t medianSeedStart =
        cfg.startHeight > currency.rewardBlocksWindow()
            ? cfg.startHeight -
                  static_cast<uint32_t>(currency.rewardBlocksWindow())
            : 0;
    for (uint32_t h = medianSeedStart; h < cfg.startHeight; ++h)
    {
      std::list<cn::Block> blocks;
      std::list<cn::Transaction> txsList;
      if (!core.get_blocks(h, 1, blocks, txsList) || blocks.empty())
      {
        std::cerr << "failed to read block " << h << std::endl;
        return 2;
      }
      std::vector<cn::Transaction> txs;
      vectorFromList(txsList, txs);

      recentBlockSizes.push_back(cn::chain_audit::cumulativeBlockSize(blocks.front(), txs));
      if (recentBlockSizes.size() > currency.rewardBlocksWindow())
      {
        recentBlockSizes.erase(recentBlockSizes.begin());
      }
    }

    std::vector<cn::chain_audit::Finding> findings;
    uint64_t scannedBlocks = 0;
    uint64_t scannedTransactions = 0;

    for (uint32_t h = cfg.startHeight; h <= endHeight; ++h)
    {
      std::list<cn::Block> blocks;
      std::list<cn::Transaction> txsList;
      if (!core.get_blocks(h, 1, blocks, txsList) || blocks.empty())
      {
        std::cerr << "failed to read block " << h << std::endl;
        return 2;
      }

      std::vector<cn::Transaction> txs;
      vectorFromList(txsList, txs);
      const cn::Block &block = blocks.front();
      const crypto::Hash blockHash = cn::get_block_hash(block);
      uint64_t storedGeneratedCoins = 0;
      if (!core.getAlreadyGeneratedCoins(blockHash, storedGeneratedCoins))
      {
        std::cerr << "failed to read emitted coins for block " << h << std::endl;
        return 2;
      }

      std::vector<size_t> medianSource = recentBlockSizes;
      const size_t medianBlockSize = common::medianValue(medianSource);
      const size_t blockSize = cn::chain_audit::cumulativeBlockSize(block, txs);

      cn::chain_audit::BlockAuditContext context;
      context.height = h;
      context.blockHash = blockHash;
      context.previousGeneratedCoins = previousGeneratedCoins;
      context.storedGeneratedCoins = storedGeneratedCoins;
      context.medianBlockSize = medianBlockSize;
      context.cumulativeBlockSize = blockSize;
      context.inCheckpointZone = checkpointZoneEnd != 0 && h <= checkpointZoneEnd;
      context.collectInfoFindings = cfg.includeInfo;

      cn::chain_audit::auditBlock(currency, block, txs, context, findings);

      if (context.inCheckpointZone && cfg.includeInfo)
      {
        for (size_t i = 0; i < txs.size(); ++i)
        {
          auditCheckpointReferences(core, txs[i], h, static_cast<uint32_t>(i + 1),
                                    blockHash, findings);
        }
      }

      previousGeneratedCoins = storedGeneratedCoins;
      recentBlockSizes.push_back(blockSize);
      if (recentBlockSizes.size() > currency.rewardBlocksWindow())
      {
        recentBlockSizes.erase(recentBlockSizes.begin());
      }
      ++scannedBlocks;
      scannedTransactions += txs.size() + 1;

      if (h % 10000 == 0)
      {
        std::cerr << "scanned height " << h << " / " << endHeight << "\r";
      }
    }
    std::cerr << std::endl;

    std::ostringstream report;
    report << "{\n"
           << "  \"tool\": \"conceal-chain-audit\",\n"
           << "  \"data_dir\": \"" << jsonEscape(cfg.dataDir) << "\",\n"
           << "  \"network\": \"" << (cfg.testnet ? "testnet" : "mainnet") << "\",\n"
           << "  \"start_height\": " << cfg.startHeight << ",\n"
           << "  \"end_height\": " << endHeight << ",\n"
           << "  \"chain_height\": " << chainHeight << ",\n"
           << "  \"checkpoint_zone_end\": " << checkpointZoneEnd << ",\n"
           << "  \"blocks_scanned\": " << scannedBlocks << ",\n"
           << "  \"transactions_scanned\": " << scannedTransactions << ",\n"
           << "  \"findings\": [\n";

    bool first = true;
    uint64_t emitted = 0;
    uint64_t actionableEmitted = 0;
    for (const auto &finding : findings)
    {
      if (!shouldEmit(finding, cfg.includeInfo))
      {
        continue;
      }
      if (!first)
      {
        report << ",\n";
      }
      first = false;
      ++emitted;
      if (finding.severity != cn::chain_audit::Severity::Info)
      {
        ++actionableEmitted;
      }
      report << "    " << findingJson(finding);
    }
    report << "\n  ],\n"
           << "  \"findings_total\": " << findings.size() << ",\n"
           << "  \"findings_emitted\": " << emitted << ",\n"
           << "  \"findings_actionable_emitted\": " << actionableEmitted << ",\n"
           << "  \"findings_suppressed\": " << findings.size() - emitted << "\n"
           << "}\n";

    if (!cfg.outFile.empty())
    {
      std::ofstream out(cfg.outFile.c_str(), std::ios::out | std::ios::trunc);
      if (!out)
      {
        std::cerr << "failed to open output file " << cfg.outFile << std::endl;
        return 2;
      }
      out << report.str();
    }
    else
    {
      std::cout << report.str();
    }

    core.deinit();
    return actionableEmitted == 0 ? 0 : 1;
  }
  catch (const std::exception &e)
  {
    std::cerr << "conceal-chain-audit: " << e.what() << std::endl;
    return 2;
  }
}

// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "WalletService.h"

#include <future>
#include <assert.h>
#include <sstream>
#include <unordered_set>

#include <boost/filesystem/operations.hpp>

#include <System/Timer.h>
#include <System/InterruptedException.h>
#include "Common/Util.h"
#include "CryptoNoteCore/Account.h"
#include "crypto/crypto.h"
#include "CryptoNote.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/TransactionExtra.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/Currency.h"
#include <System/EventLock.h>

#include "PaymentServiceJsonRpcMessages.h"
#include "NodeFactory.h"

#include "Wallet/WalletGreen.h"
#include "Wallet/LegacyKeysImporter.h"
#include "Wallet/WalletErrors.h"
#include "Wallet/WalletUtils.h"
#include "WalletServiceErrorCategory.h"
#include "CryptoNoteCore/CryptoNoteTools.h"

#include "Common/CommandLine.h"
#include "Common/StringTools.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/Account.h"
#include "crypto/hash.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "WalletLegacy/WalletHelper.h"
#include "Common/Base58.h"
#include "Common/CommandLine.h"
#include "Common/SignalHandler.h"
#include "Common/StringTools.h"
#include "Common/PathTools.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"

using namespace CryptoNote;

namespace PaymentService
{

  namespace
  {

    bool checkPaymentId(const std::string &paymentId)
    {
      if (paymentId.size() != 64)
      {
        return false;
      }

      return std::all_of(paymentId.begin(), paymentId.end(), [](const char c) {
        if (c >= '0' && c <= '9')
        {
          return true;
        }

        if (c >= 'a' && c <= 'f')
        {
          return true;
        }

        if (c >= 'A' && c <= 'F')
        {
          return true;
        }

        return false;
      });
    }

    Crypto::Hash parsePaymentId(const std::string &paymentIdStr)
    {
      if (!checkPaymentId(paymentIdStr))
      {
        throw std::system_error(make_error_code(CryptoNote::error::WalletServiceErrorCode::WRONG_PAYMENT_ID_FORMAT));
      }

      Crypto::Hash paymentId;
      bool r = Common::podFromHex(paymentIdStr, paymentId);
      assert(r);

      return paymentId;
    }

    bool getPaymentIdFromExtra(const std::string &binaryString, Crypto::Hash &paymentId)
    {
      return CryptoNote::getPaymentIdFromTxExtra(Common::asBinaryArray(binaryString), paymentId);
    }

    std::string getPaymentIdStringFromExtra(const std::string &binaryString)
    {
      Crypto::Hash paymentId;

      if (!getPaymentIdFromExtra(binaryString, paymentId))
      {
        return std::string();
      }

      return Common::podToHex(paymentId);
    }

  } // namespace

  struct TransactionsInBlockInfoFilter
  {
    TransactionsInBlockInfoFilter(const std::vector<std::string> &addressesVec, const std::string &paymentIdStr)
    {
      addresses.insert(addressesVec.begin(), addressesVec.end());

      if (!paymentIdStr.empty())
      {
        paymentId = parsePaymentId(paymentIdStr);
        havePaymentId = true;
      }
      else
      {
        havePaymentId = false;
      }
    }

    bool checkTransaction(const CryptoNote::WalletTransactionWithTransfers &transaction) const
    {
      if (havePaymentId)
      {
        Crypto::Hash transactionPaymentId;
        if (!getPaymentIdFromExtra(transaction.transaction.extra, transactionPaymentId))
        {
          return false;
        }

        if (paymentId != transactionPaymentId)
        {
          return false;
        }
      }

      if (addresses.empty())
      {
        return true;
      }

      bool haveAddress = false;
      for (const CryptoNote::WalletTransfer &transfer : transaction.transfers)
      {
        if (addresses.find(transfer.address) != addresses.end())
        {
          haveAddress = true;
          break;
        }
      }

      return haveAddress;
    }

    std::unordered_set<std::string> addresses;
    bool havePaymentId = false;
    Crypto::Hash paymentId;
  };

  namespace
  {

    void addPaymentIdToExtra(const std::string &paymentId, std::string &extra)
    {
      std::vector<uint8_t> extraVector;
      if (!CryptoNote::createTxExtraWithPaymentId(paymentId, extraVector))
      {
        throw std::runtime_error("Couldn't add payment id to extra");
      }

      std::copy(extraVector.begin(), extraVector.end(), std::back_inserter(extra));
    }

    void validatePaymentId(const std::string &paymentId, Logging::LoggerRef logger)
    {
      if (!checkPaymentId(paymentId))
      {
        logger(Logging::WARNING) << "Can't validate payment id: " << paymentId;
        throw std::system_error(make_error_code(CryptoNote::error::WalletServiceErrorCode::WRONG_PAYMENT_ID_FORMAT));
      }
    }

    bool createOutputBinaryFile(const std::string &filename, std::fstream &file)
    {
      file.open(filename.c_str(), std::fstream::in | std::fstream::out | std::ofstream::binary);
      if (file)
      {
        file.close();
        return false;
      }

      file.open(filename.c_str(), std::fstream::out | std::fstream::binary);
      return true;
    }

    std::string createTemporaryFile(const std::string &path, std::fstream &tempFile)
    {
      bool created = false;
      std::string temporaryName;

      for (size_t i = 1; i < 100; i++)
      {
        temporaryName = path + "." + std::to_string(i++);

        if (createOutputBinaryFile(temporaryName, tempFile))
        {
          created = true;
          break;
        }
      }

      if (!created)
      {
        throw std::runtime_error("Couldn't create temporary file: " + temporaryName);
      }

      return temporaryName;
    }

    //returns true on success
    bool deleteFile(const std::string &filename)
    {
      boost::system::error_code err;
      return boost::filesystem::remove(filename, err) && !err;
    }

    void replaceWalletFiles(const std::string &path, const std::string &tempFilePath)
    {
      Tools::replace_file(tempFilePath, path);
    }

    Crypto::Hash parseHash(const std::string &hashString, Logging::LoggerRef logger)
    {
      Crypto::Hash hash;

      if (!Common::podFromHex(hashString, hash))
      {
        logger(Logging::WARNING) << "Can't parse hash string " << hashString;
        throw std::system_error(make_error_code(CryptoNote::error::WalletServiceErrorCode::WRONG_HASH_FORMAT));
      }

      return hash;
    }

    std::vector<CryptoNote::TransactionsInBlockInfo> filterTransactions(
        const std::vector<CryptoNote::TransactionsInBlockInfo> &blocks,
        const TransactionsInBlockInfoFilter &filter)
    {
      std::vector<CryptoNote::TransactionsInBlockInfo> result;

      for (const auto &block : blocks)
      {
        CryptoNote::TransactionsInBlockInfo item;
        item.blockHash = block.blockHash;

        for (const auto &transaction : block.transactions)
        {
          if (transaction.transaction.state != CryptoNote::WalletTransactionState::DELETED && filter.checkTransaction(transaction))
          {
            item.transactions.push_back(transaction);
          }
        }

        if (!block.transactions.empty())
        {
          result.push_back(std::move(item));
        }
      }

      return result;
    }

    //KD2

    PaymentService::TransactionRpcInfo convertTransactionWithTransfersToTransactionRpcInfo(const CryptoNote::WalletTransactionWithTransfers &transactionWithTransfers)
    {
      PaymentService::TransactionRpcInfo transactionInfo;
      transactionInfo.state = static_cast<uint8_t>(transactionWithTransfers.transaction.state);
      transactionInfo.transactionHash = Common::podToHex(transactionWithTransfers.transaction.hash);
      transactionInfo.blockIndex = transactionWithTransfers.transaction.blockHeight;
      transactionInfo.timestamp = transactionWithTransfers.transaction.timestamp;
      transactionInfo.isBase = transactionWithTransfers.transaction.isBase;
      transactionInfo.depositCount = transactionWithTransfers.transaction.depositCount;
      transactionInfo.firstDepositId = transactionWithTransfers.transaction.firstDepositId;
      transactionInfo.unlockTime = transactionWithTransfers.transaction.unlockTime;
      transactionInfo.amount = transactionWithTransfers.transaction.totalAmount;
      transactionInfo.fee = transactionWithTransfers.transaction.fee;
      transactionInfo.extra = Common::toHex(transactionWithTransfers.transaction.extra.data(), transactionWithTransfers.transaction.extra.size());
      transactionInfo.paymentId = getPaymentIdStringFromExtra(transactionWithTransfers.transaction.extra);

      for (const CryptoNote::WalletTransfer &transfer : transactionWithTransfers.transfers)
      {
        PaymentService::TransferRpcInfo rpcTransfer;
        rpcTransfer.address = transfer.address;
        rpcTransfer.amount = transfer.amount;
        rpcTransfer.type = static_cast<uint8_t>(transfer.type);
        transactionInfo.transfers.push_back(std::move(rpcTransfer));
      }
      return transactionInfo;
    }

    std::vector<PaymentService::TransactionsInBlockRpcInfo> convertTransactionsInBlockInfoToTransactionsInBlockRpcInfo(
        const std::vector<CryptoNote::TransactionsInBlockInfo> &blocks, uint32_t &knownBlockCount)
    {
      std::vector<PaymentService::TransactionsInBlockRpcInfo> rpcBlocks;
      rpcBlocks.reserve(blocks.size());
      for (const auto &block : blocks)
      {
        PaymentService::TransactionsInBlockRpcInfo rpcBlock;
        rpcBlock.blockHash = Common::podToHex(block.blockHash);

        for (const CryptoNote::WalletTransactionWithTransfers &transactionWithTransfers : block.transactions)
        {
          PaymentService::TransactionRpcInfo transactionInfo = convertTransactionWithTransfersToTransactionRpcInfo(transactionWithTransfers);
          transactionInfo.confirmations = knownBlockCount - transactionInfo.blockIndex;
          rpcBlock.transactions.push_back(std::move(transactionInfo));
        }

        rpcBlocks.push_back(std::move(rpcBlock));
      }

      return rpcBlocks;
    }

    std::vector<PaymentService::TransactionHashesInBlockRpcInfo> convertTransactionsInBlockInfoToTransactionHashesInBlockRpcInfo(
        const std::vector<CryptoNote::TransactionsInBlockInfo> &blocks)
    {

      std::vector<PaymentService::TransactionHashesInBlockRpcInfo> transactionHashes;
      transactionHashes.reserve(blocks.size());
      for (const CryptoNote::TransactionsInBlockInfo &block : blocks)
      {
        PaymentService::TransactionHashesInBlockRpcInfo item;
        item.blockHash = Common::podToHex(block.blockHash);

        for (const CryptoNote::WalletTransactionWithTransfers &transaction : block.transactions)
        {
          item.transactionHashes.emplace_back(Common::podToHex(transaction.transaction.hash));
        }

        transactionHashes.push_back(std::move(item));
      }

      return transactionHashes;
    }

    void validateAddresses(const std::vector<std::string> &addresses, const CryptoNote::Currency &currency, Logging::LoggerRef logger)
    {
      for (const auto &address : addresses)
      {
        if (!CryptoNote::validateAddress(address, currency))
        {
          logger(Logging::WARNING) << "Can't validate address " << address;
          throw std::system_error(make_error_code(CryptoNote::error::BAD_ADDRESS));
        }
      }
    }

    std::vector<std::string> collectDestinationAddresses(const std::vector<PaymentService::WalletRpcOrder> &orders)
    {
      std::vector<std::string> result;

      result.reserve(orders.size());
      for (const auto &order : orders)
      {
        result.push_back(order.address);
      }

      return result;
    }

    std::vector<PaymentService::WalletRpcMessage> collectMessages(const std::vector<PaymentService::WalletRpcOrder> &orders)
    {
      std::vector<PaymentService::WalletRpcMessage> result;

      result.reserve(orders.size());
      for (const auto &order : orders)
      {
        if (!order.message.empty())
        {
          result.push_back({order.address, order.message});
        }
      }

      return result;
    }

    std::vector<CryptoNote::WalletOrder> convertWalletRpcOrdersToWalletOrders(const std::vector<PaymentService::WalletRpcOrder> &orders)
    {
      std::vector<CryptoNote::WalletOrder> result;
      result.reserve(orders.size());

      for (const auto &order : orders)
      {
        result.emplace_back(CryptoNote::WalletOrder{order.address, order.amount});
      }

      return result;
    }

    std::vector<CryptoNote::WalletMessage> convertWalletRpcMessagesToWalletMessages(const std::vector<PaymentService::WalletRpcMessage> &messages)
    {
      std::vector<CryptoNote::WalletMessage> result;
      result.reserve(messages.size());

      for (const auto &message : messages)
      {
        result.emplace_back(CryptoNote::WalletMessage{message.address, message.message});
      }

      return result;
    }

  } // namespace

  void createWalletFile(std::fstream &walletFile, const std::string &filename)
  {
    boost::filesystem::path pathToWalletFile(filename);
    boost::filesystem::path directory = pathToWalletFile.parent_path();
    if (!directory.empty() && !Tools::directoryExists(directory.string()))
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

  void saveWallet(CryptoNote::IWallet &wallet, std::fstream &walletFile, bool saveDetailed = true, bool saveCache = true)
  {
    wallet.save();
    walletFile.flush();
  }

  void secureSaveWallet(CryptoNote::IWallet &wallet, const std::string &path, bool saveDetailed = true, bool saveCache = true)
  {
    std::fstream tempFile;
    std::string tempFilePath = createTemporaryFile(path, tempFile);

    try
    {
      saveWallet(wallet, tempFile, saveDetailed, saveCache);
    }
    catch (std::exception &)
    {
      deleteFile(tempFilePath);
      tempFile.close();
      throw;
    }
    tempFile.close();

    replaceWalletFiles(path, tempFilePath);
  }

  /* Generate a new wallet (-g) or import a new wallet if the secret keys have been specified */
  void generateNewWallet(
      const CryptoNote::Currency &currency,
      const WalletConfiguration &conf,
      Logging::ILogger &logger,
      System::Dispatcher &dispatcher)
  {
    Logging::LoggerRef log(logger, "generateNewWallet");

    CryptoNote::INode *nodeStub = NodeFactory::createNodeStub();
    std::unique_ptr<CryptoNote::INode> nodeGuard(nodeStub);

    CryptoNote::IWallet *wallet = new CryptoNote::WalletGreen(dispatcher, currency, *nodeStub, logger);
    std::unique_ptr<CryptoNote::IWallet> walletGuard(wallet);

    std::string address;

    /* Create a new address and container since both view key and spend key
     have not been specified */
    if (conf.secretSpendKey.empty() && conf.secretViewKey.empty())
    {
      log(Logging::INFO, Logging::BRIGHT_WHITE) << "Generating new deterministic wallet";

      Crypto::SecretKey private_view_key;
      CryptoNote::KeyPair spendKey;

      Crypto::generate_keys(spendKey.publicKey, spendKey.secretKey);

      Crypto::PublicKey unused_dummy_variable;

      CryptoNote::AccountBase::generateViewFromSpend(spendKey.secretKey, private_view_key, unused_dummy_variable);

      wallet->initializeWithViewKey(conf.walletFile, conf.walletPassword, private_view_key);
      address = wallet->createAddress(spendKey.secretKey);

      log(Logging::INFO, Logging::BRIGHT_WHITE) << "New deterministic wallet is generated. Address: " << address;

      //TODO make this a cout
      log(Logging::INFO, Logging::BRIGHT_WHITE) << "New wallet generated.";
      log(Logging::INFO, Logging::BRIGHT_WHITE) << "Address: " << address;
      log(Logging::INFO, Logging::BRIGHT_WHITE) << "Secret spend key: " << Common::podToHex(spendKey.secretKey);
      log(Logging::INFO, Logging::BRIGHT_WHITE) << "Secret view key: " << Common::podToHex(private_view_key);
    }
    /* We need both secret keys to import the wallet and create the container
     so in the absence of either, display and error message and return */
    else if (conf.secretSpendKey.empty() || conf.secretViewKey.empty())
    {
      log(Logging::ERROR, Logging::BRIGHT_RED) << "Need both secret spend key and secret view key.";
      return;
    }
    /* Both keys are present so attempt to import the wallet */
    else
    {
      log(Logging::INFO, Logging::BRIGHT_WHITE) << "Attemping to create container from keys";
      Crypto::Hash private_spend_key_hash;
      Crypto::Hash private_view_key_hash;
      size_t size;

      /* Check if both keys are valid */
      if (!Common::fromHex(conf.secretSpendKey, &private_spend_key_hash, sizeof(private_spend_key_hash), size) || size != sizeof(private_spend_key_hash))
      {
        log(Logging::ERROR, Logging::BRIGHT_RED) << "Spend key is invalid";
        return;
      }
      if (!Common::fromHex(conf.secretViewKey, &private_view_key_hash, sizeof(private_view_key_hash), size) || size != sizeof(private_spend_key_hash))
      {
        log(Logging::ERROR, Logging::BRIGHT_RED) << "View key is invalid";
        return;
      }

      Crypto::SecretKey private_spend_key = *(struct Crypto::SecretKey *)&private_spend_key_hash;
      Crypto::SecretKey private_view_key = *(struct Crypto::SecretKey *)&private_view_key_hash;

      wallet->initializeWithViewKey(conf.walletFile, conf.walletPassword, private_view_key);
      address = wallet->createAddress(private_spend_key);
      log(Logging::INFO, Logging::BRIGHT_WHITE) << "Imported wallet successfully.";
      log(Logging::INFO, Logging::BRIGHT_WHITE) << "Address: " << address;
    }

    /* Save the container and exit */
    wallet->save(CryptoNote::WalletSaveLevel::SAVE_KEYS_ONLY);
    log(Logging::INFO) << "Wallet is saved";
  } // namespace PaymentService

  void importLegacyKeys(const std::string &legacyKeysFile, const WalletConfiguration &conf)
  {
    std::stringstream archive;

    CryptoNote::importLegacyKeys(legacyKeysFile, conf.walletPassword, archive);

    std::fstream walletFile;
    createWalletFile(walletFile, conf.walletFile);

    archive.flush();
    walletFile << archive.rdbuf();
    walletFile.flush();
  }

  WalletService::WalletService(
      const CryptoNote::Currency &currency,
      System::Dispatcher &sys,
      CryptoNote::INode &node,
      CryptoNote::IWallet &wallet,
      CryptoNote::IFusionManager &fusionManager,
      const WalletConfiguration &conf,
      Logging::ILogger &logger) : currency(currency),
                                  wallet(wallet),
                                  fusionManager(fusionManager),
                                  node(node),
                                  config(conf),
                                  inited(false),
                                  logger(logger, "WalletService"),
                                  dispatcher(sys),
                                  readyEvent(dispatcher),
                                  refreshContext(dispatcher)
  {
    readyEvent.set();
  }

  WalletService::~WalletService()
  {
    if (inited)
    {
      wallet.stop();
      refreshContext.wait();
      wallet.shutdown();
    }
  }

  void WalletService::init()
  {
    loadWallet();
    loadTransactionIdIndex();

    refreshContext.spawn([this] { refresh(); });

    inited = true;
  }

  void WalletService::saveWallet()
  {
    wallet.save();
    logger(Logging::INFO, Logging::BRIGHT_WHITE) << "Wallet is saved";
  }

  std::error_code WalletService::saveWalletNoThrow()
  {
    try
    {
      System::EventLock lk(readyEvent);

      logger(Logging::INFO, Logging::BRIGHT_WHITE) << "Saving wallet...";

      if (!inited)
      {
        logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Save impossible: Wallet Service is not initialized";
        return make_error_code(CryptoNote::error::NOT_INITIALIZED);
      }

      saveWallet();
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while saving wallet: " << x.what();
      return x.code();
    }
    catch (std::exception &x)
    {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while saving wallet: " << x.what();
      return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
    }

    return std::error_code();
  }

  void WalletService::loadWallet()
  {
    logger(Logging::INFO, Logging::BRIGHT_WHITE) << "Loading wallet";
    wallet.load(config.walletFile, config.walletPassword);
    logger(Logging::INFO, Logging::BRIGHT_WHITE) << "Wallet loading is finished.";
  }

  void WalletService::loadTransactionIdIndex()
  {
    transactionIdIndex.clear();

    for (size_t i = 0; i < wallet.getTransactionCount(); ++i)
    {
      transactionIdIndex.emplace(Common::podToHex(wallet.getTransaction(i).hash), i);
    }
  }

  std::error_code WalletService::resetWallet()
  {
    try
    {
      System::EventLock lk(readyEvent);

      logger(Logging::INFO, Logging::BRIGHT_WHITE) << "Resetting wallet";

      if (!inited)
      {
        logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Reset impossible: Wallet Service is not initialized";
        return make_error_code(CryptoNote::error::NOT_INITIALIZED);
      }

      reset();
      logger(Logging::INFO, Logging::BRIGHT_WHITE) << "Wallet has been reset";
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while resetting wallet: " << x.what();
      return x.code();
    }
    catch (std::exception &x)
    {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while resetting wallet: " << x.what();
      return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
    }

    return std::error_code();
  }

  std::error_code WalletService::exportWallet(const std::string &fileName)
  {
    try
    {
      System::EventLock lk(readyEvent);

      saveWallet();

      if (!inited)
      {
        logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Export impossible: Wallet Service is not initialized";
        return make_error_code(CryptoNote::error::NOT_INITIALIZED);
      }

      boost::filesystem::path walletPath(config.walletFile);
      boost::filesystem::path exportPath = walletPath.parent_path() / fileName;

      logger(Logging::INFO, Logging::BRIGHT_WHITE) << "Exporting wallet to filename" << exportPath.string();

      logger(Logging::INFO, Logging::BRIGHT_WHITE) << "Exporting wallet to " << exportPath.string();
      wallet.exportWallet(exportPath.string());
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while exporting wallet: " << x.what();
      return x.code();
    }
    catch (std::exception &x)
    {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while exporting wallet: " << x.what();
      return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
    }

    return std::error_code();
  }

  std::error_code WalletService::exportWalletKeys(const std::string &fileName)
  {

    try
    {
      System::EventLock lk(readyEvent);

      saveWallet();

      if (!inited)
      {
        logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Export impossible: Wallet Service is not initialized";
        return make_error_code(CryptoNote::error::NOT_INITIALIZED);
      }

      boost::filesystem::path walletPath(config.walletFile);
      boost::filesystem::path exportPath = walletPath.parent_path() / fileName;

      logger(Logging::INFO, Logging::BRIGHT_WHITE) << "Exporting wallet keys to filename" << exportPath.string();

      logger(Logging::INFO, Logging::BRIGHT_WHITE) << "Exporting wallet keys to " << exportPath.string();
      wallet.exportWalletKeys(exportPath.string());
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while exporting wallet: " << x.what();
      return x.code();
    }
    catch (std::exception &x)
    {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while exporting wallet: " << x.what();
      return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
    }

    return std::error_code();
  }

  std::error_code WalletService::resetWallet(const uint32_t scanHeight)
  {
    try
    {
      System::EventLock lk(readyEvent);

      logger(Logging::INFO, Logging::BRIGHT_WHITE) << "Resetting wallet";

      if (!inited)
      {
        logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Reset impossible: Wallet Service is not initialized";
        return make_error_code(CryptoNote::error::NOT_INITIALIZED);
      }

      wallet.reset(scanHeight);
      logger(Logging::INFO, Logging::BRIGHT_WHITE) << "Wallet has been reset starting scanning from height " << scanHeight;
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while resetting wallet: " << x.what();
      return x.code();
    }
    catch (std::exception &x)
    {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while resetting wallet: " << x.what();
      return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
    }

    return std::error_code();
  }

  std::error_code WalletService::createAddress(const std::string &spendSecretKeyText, std::string &address)
  {
    try
    {
      System::EventLock lk(readyEvent);

      logger(Logging::DEBUGGING) << "Creating address";

      saveWallet();

      Crypto::SecretKey secretKey;
      if (!Common::podFromHex(spendSecretKeyText, secretKey))
      {
        logger(Logging::WARNING) << "Wrong key format: " << spendSecretKeyText;
        return make_error_code(CryptoNote::error::WalletServiceErrorCode::WRONG_KEY_FORMAT);
      }

      address = wallet.createAddress(secretKey);
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING) << "Error while creating address: " << x.what();
      return x.code();
    }

    logger(Logging::DEBUGGING) << "Created address " << address;

    return std::error_code();
  }

  std::error_code WalletService::createAddress(std::string &address)
  {
    try
    {
      System::EventLock lk(readyEvent);

      logger(Logging::DEBUGGING) << "Creating address";

      address = wallet.createAddress();
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while creating address: " << x.what();
      return x.code();
    }

    logger(Logging::DEBUGGING) << "Created address " << address;

    return std::error_code();
  }

  std::error_code WalletService::createAddressList(const std::vector<std::string> &spendSecretKeysText, bool reset, std::vector<std::string> &addresses)
  {
    try
    {
      System::EventLock lk(readyEvent);
      logger(Logging::DEBUGGING) << "Creating " << spendSecretKeysText.size() << " addresses...";
      std::vector<Crypto::SecretKey> secretKeys;
      std::unordered_set<std::string> unique;
      secretKeys.reserve(spendSecretKeysText.size());
      unique.reserve(spendSecretKeysText.size());
      for (auto &keyText : spendSecretKeysText)
      {
        auto insertResult = unique.insert(keyText);
        if (!insertResult.second)
        {
          logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Not unique key";
          return make_error_code(CryptoNote::error::WalletServiceErrorCode::DUPLICATE_KEY);
        }

        Crypto::SecretKey key;
        if (!Common::podFromHex(keyText, key))
        {
          logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Wrong key format: " << keyText;
          return make_error_code(CryptoNote::error::WalletServiceErrorCode::WRONG_KEY_FORMAT);
        }
        secretKeys.push_back(std::move(key));
      }
      addresses = wallet.createAddressList(secretKeys, reset);
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while creating addresses: " << x.what();
      return x.code();
    }

    logger(Logging::DEBUGGING) << "Created " << addresses.size() << " addresses";
    return std::error_code();
  }

  std::error_code WalletService::createTrackingAddress(const std::string &spendPublicKeyText, std::string &address)
  {
    try
    {
      System::EventLock lk(readyEvent);

      logger(Logging::DEBUGGING) << "Creating tracking address";

      Crypto::PublicKey publicKey;
      if (!Common::podFromHex(spendPublicKeyText, publicKey))
      {
        logger(Logging::WARNING) << "Wrong key format: " << spendPublicKeyText;
        return make_error_code(CryptoNote::error::WalletServiceErrorCode::WRONG_KEY_FORMAT);
      }

      address = wallet.createAddress(publicKey);
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING) << "Error while creating tracking address: " << x.what();
      return x.code();
    }

    logger(Logging::DEBUGGING) << "Created address " << address;
    return std::error_code();
  }

  std::error_code WalletService::deleteAddress(const std::string &address)
  {
    try
    {
      System::EventLock lk(readyEvent);

      logger(Logging::DEBUGGING) << "Delete address request came";
      wallet.deleteAddress(address);
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while deleting address: " << x.what();
      return x.code();
    }

    logger(Logging::DEBUGGING) << "Address " << address << " successfully deleted";
    return std::error_code();
  }

  std::error_code WalletService::getSpendkeys(const std::string &address, std::string &publicSpendKeyText, std::string &secretSpendKeyText)
  {
    try
    {
      System::EventLock lk(readyEvent);

      CryptoNote::KeyPair key = wallet.getAddressSpendKey(address);

      publicSpendKeyText = Common::podToHex(key.publicKey);
      secretSpendKeyText = Common::podToHex(key.secretKey);
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING) << "Error while getting spend key: " << x.what();
      return x.code();
    }

    return std::error_code();
  }

  std::error_code WalletService::getBalance(const std::string &address, uint64_t &availableBalance, uint64_t &lockedAmount, uint64_t &lockedDepositBalance, uint64_t &unlockedDepositBalance)
  {
    try
    {
      System::EventLock lk(readyEvent);
      logger(Logging::DEBUGGING) << "Getting balance for address " << address;

      availableBalance = wallet.getActualBalance(address);
      lockedAmount = wallet.getPendingBalance(address);
      lockedDepositBalance = wallet.getLockedDepositBalance(address);
      unlockedDepositBalance = wallet.getUnlockedDepositBalance(address);
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING) << "Error while getting balance: " << x.what();
      return x.code();
    }

    logger(Logging::DEBUGGING) << address << " actual balance: " << availableBalance << ", pending: " << lockedAmount;
    return std::error_code();
  }

  std::error_code WalletService::getBalance(uint64_t &availableBalance, uint64_t &lockedAmount, uint64_t &lockedDepositBalance, uint64_t &unlockedDepositBalance)
  {
    try
    {
      System::EventLock lk(readyEvent);
      logger(Logging::DEBUGGING) << "Getting wallet balance";

      availableBalance = wallet.getActualBalance();
      lockedAmount = wallet.getPendingBalance();
      lockedDepositBalance = wallet.getLockedDepositBalance();
      unlockedDepositBalance = wallet.getUnlockedDepositBalance();
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING) << "Error while getting balance: " << x.what();
      return x.code();
    }

    logger(Logging::DEBUGGING) << "Wallet actual balance: " << availableBalance << ", pending: " << lockedAmount;
    return std::error_code();
  }

  std::error_code WalletService::getBlockHashes(uint32_t firstBlockIndex, uint32_t blockCount, std::vector<std::string> &blockHashes)
  {
    try
    {
      System::EventLock lk(readyEvent);
      std::vector<Crypto::Hash> hashes = wallet.getBlockHashes(firstBlockIndex, blockCount);

      blockHashes.reserve(hashes.size());
      for (const auto &hash : hashes)
      {
        blockHashes.push_back(Common::podToHex(hash));
      }
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING) << "Error while getting block hashes: " << x.what();
      return x.code();
    }

    return std::error_code();
  }

  std::error_code WalletService::getViewKey(std::string &viewSecretKey)
  {
    try
    {
      System::EventLock lk(readyEvent);
      CryptoNote::KeyPair viewKey = wallet.getViewKey();
      viewSecretKey = Common::podToHex(viewKey.secretKey);
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting view key: " << x.what();
      return x.code();
    }

    return std::error_code();
  }

  std::error_code WalletService::getTransactionHashes(
      const std::vector<std::string> &addresses,
      const std::string &blockHashString,
      uint32_t blockCount, const std::string &paymentId,
      std::vector<TransactionHashesInBlockRpcInfo> &transactionHashes)
  {
    try
    {
      System::EventLock lk(readyEvent);
      validateAddresses(addresses, currency, logger);

      if (!paymentId.empty())
      {
        validatePaymentId(paymentId, logger);
      }

      TransactionsInBlockInfoFilter transactionFilter(addresses, paymentId);
      Crypto::Hash blockHash = parseHash(blockHashString, logger);

      transactionHashes = getRpcTransactionHashes(blockHash, blockCount, transactionFilter);
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING) << "Error while getting transactions: " << x.what();
      return x.code();
    }
    catch (std::exception &x)
    {
      logger(Logging::WARNING) << "Error while getting transactions: " << x.what();
      return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
    }

    return std::error_code();
  }

  std::error_code WalletService::getTransactionHashes(const std::vector<std::string> &addresses, uint32_t firstBlockIndex,
                                                      uint32_t blockCount, const std::string &paymentId, std::vector<TransactionHashesInBlockRpcInfo> &transactionHashes)
  {
    try
    {
      System::EventLock lk(readyEvent);
      validateAddresses(addresses, currency, logger);

      if (!paymentId.empty())
      {
        validatePaymentId(paymentId, logger);
      }

      TransactionsInBlockInfoFilter transactionFilter(addresses, paymentId);
      transactionHashes = getRpcTransactionHashes(firstBlockIndex, blockCount, transactionFilter);
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING) << "Error while getting transactions: " << x.what();
      return x.code();
    }
    catch (std::exception &x)
    {
      logger(Logging::WARNING) << "Error while getting transactions: " << x.what();
      return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
    }

    return std::error_code();
  }

  std::error_code WalletService::getDeposit(uint64_t depositId, uint64_t &amount, uint64_t &term, uint64_t &interest, std::string &creatingTransactionHash, std::string &spendingTransactionHash, bool &locked, uint64_t &height, uint64_t &unlockHeight, std::string &address)
  {
    try
    {
      System::EventLock lk(readyEvent);
      Deposit deposit = wallet.getDeposit(depositId);
      amount = deposit.amount;
      term = deposit.term;
      interest = deposit.interest;
      height = deposit.height;
      unlockHeight = deposit.unlockHeight;

      WalletTransaction wallettx = wallet.getTransaction(deposit.creatingTransactionId);
      creatingTransactionHash = Common::podToHex(wallettx.hash);

      WalletTransfer transfer = wallet.getTransactionTransfer(deposit.creatingTransactionId, 0);

      address = transfer.address;

      if (deposit.spendingTransactionId != WALLET_INVALID_TRANSACTION_ID)
      {
        WalletTransaction walletstx = wallet.getTransaction(deposit.spendingTransactionId);
        spendingTransactionHash = Common::podToHex(walletstx.hash);
      }

      bool state = true;
      uint32_t knownBlockCount = node.getKnownBlockCount();
      if (knownBlockCount > unlockHeight)
      {
        locked = false;
      }
    }
    catch (std::exception &x)
    {
      logger(Logging::WARNING) << "Error while getting deposit: " << x.what();
      return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
    }

    return std::error_code();
  }

  std::error_code WalletService::getTransactions(
      const std::vector<std::string> &addresses,
      const std::string &blockHashString,
      uint32_t blockCount,
      const std::string &paymentId,
      std::vector<TransactionsInBlockRpcInfo> &transactions)
  {
    try
    {
      System::EventLock lk(readyEvent);
      validateAddresses(addresses, currency, logger);

      if (!paymentId.empty())
      {
        validatePaymentId(paymentId, logger);
      }

      TransactionsInBlockInfoFilter transactionFilter(addresses, paymentId);

      Crypto::Hash blockHash = parseHash(blockHashString, logger);

      transactions = getRpcTransactions(blockHash, blockCount, transactionFilter);
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING) << "Error while getting transactions: " << x.what();
      return x.code();
    }
    catch (std::exception &x)
    {
      logger(Logging::WARNING) << "Error while getting transactions: " << x.what();
      return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
    }

    return std::error_code();
  }

  std::error_code WalletService::getTransactions(const std::vector<std::string> &addresses, uint32_t firstBlockIndex,
                                                 uint32_t blockCount, const std::string &paymentId, std::vector<TransactionsInBlockRpcInfo> &transactions)
  {
    try
    {
      System::EventLock lk(readyEvent);
      validateAddresses(addresses, currency, logger);

      if (!paymentId.empty())
      {
        validatePaymentId(paymentId, logger);
      }

      TransactionsInBlockInfoFilter transactionFilter(addresses, paymentId);

      transactions = getRpcTransactions(firstBlockIndex, blockCount, transactionFilter);
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING) << "Error while getting transactions: " << x.what();
      return x.code();
    }
    catch (std::exception &x)
    {
      logger(Logging::WARNING) << "Error while getting transactions: " << x.what();
      return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
    }

    return std::error_code();
  }

  //KD1

  std::error_code WalletService::getTransaction(const std::string &transactionHash, TransactionRpcInfo &transaction)
  {
    try
    {
      System::EventLock lk(readyEvent);
      Crypto::Hash hash = parseHash(transactionHash, logger);

      CryptoNote::WalletTransactionWithTransfers transactionWithTransfers = wallet.getTransaction(hash);

      if (transactionWithTransfers.transaction.state == CryptoNote::WalletTransactionState::DELETED)
      {
        logger(Logging::WARNING) << "Transaction " << transactionHash << " is deleted";
        return make_error_code(CryptoNote::error::OBJECT_NOT_FOUND);
      }

      /* Pull all the transaction information and add it to the transaction reponse */
      transaction.state = static_cast<uint8_t>(transactionWithTransfers.transaction.state);
      transaction.transactionHash = Common::podToHex(transactionWithTransfers.transaction.hash);
      transaction.blockIndex = transactionWithTransfers.transaction.blockHeight;
      transaction.timestamp = transactionWithTransfers.transaction.timestamp;
      transaction.isBase = transactionWithTransfers.transaction.isBase;
      transaction.unlockTime = transactionWithTransfers.transaction.unlockTime;
      transaction.amount = transactionWithTransfers.transaction.totalAmount;
      transaction.fee = transactionWithTransfers.transaction.fee;
      transaction.firstDepositId = transactionWithTransfers.transaction.firstDepositId;
      transaction.depositCount = transactionWithTransfers.transaction.depositCount;
      transaction.extra = Common::toHex(transactionWithTransfers.transaction.extra.data(), transactionWithTransfers.transaction.extra.size());
      transaction.paymentId = getPaymentIdStringFromExtra(transactionWithTransfers.transaction.extra);

      /* Calculate the number of confirmations for the transaction */
      uint32_t knownBlockCount = node.getKnownBlockCount();
      transaction.confirmations = knownBlockCount - transaction.blockIndex;

      /* Cycle through all the transfers in the transaction and extract the address, 
       amount, and pull any messages from Extra */
      std::vector<std::string> messages;
      std::vector<uint8_t> extraBin = Common::fromHex(transaction.extra);
      Crypto::PublicKey publicKey = CryptoNote::getTransactionPublicKeyFromExtra(extraBin);
      messages.clear();

      for (const CryptoNote::WalletTransfer &transfer : transactionWithTransfers.transfers)
      {
        PaymentService::TransferRpcInfo rpcTransfer;
        rpcTransfer.address = transfer.address;
        rpcTransfer.amount = transfer.amount;
        rpcTransfer.type = static_cast<uint8_t>(transfer.type);

        for (size_t i = 0; i < wallet.getAddressCount(); ++i)
        {
          if (wallet.getAddress(i) == rpcTransfer.address)
          {
            Crypto::SecretKey secretKey = wallet.getAddressSpendKey(wallet.getAddress(i)).secretKey;
            std::vector<std::string> m = CryptoNote::get_messages_from_extra(extraBin, publicKey, &secretKey);
            if (!m.empty())
            {
              rpcTransfer.message = m[0];
            }
          }
        }
        transaction.transfers.push_back(std::move(rpcTransfer));
      }
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING) << "Error while getting transaction: " << transactionHash << x.what();
      return x.code();
    }
    catch (std::exception &x)
    {
      logger(Logging::WARNING) << "Error while getting transaction: " << transactionHash << x.what();
      return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
    }
    return std::error_code();
  }

  std::error_code WalletService::getAddresses(std::vector<std::string> &addresses)
  {
    try
    {
      System::EventLock lk(readyEvent);

      addresses.clear();
      addresses.reserve(wallet.getAddressCount());

      for (size_t i = 0; i < wallet.getAddressCount(); ++i)
      {
        addresses.push_back(wallet.getAddress(i));
      }
    }
    catch (std::exception &e)
    {
      logger(Logging::WARNING) << "Can't get addresses: " << e.what();
      return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
    }

    return std::error_code();
  }

  std::error_code WalletService::sendTransaction(const SendTransaction::Request &request, std::string &transactionHash, std::string &transactionSecretKey)
  {

    try
    {
      System::EventLock lk(readyEvent);

      uint64_t knownBlockCount = node.getKnownBlockCount();
      uint64_t localBlockCount = node.getLocalBlockCount();
      uint64_t diff = knownBlockCount - localBlockCount;
      if ((localBlockCount == 0) || (diff > 2))
      {
        logger(Logging::WARNING) << "Daemon is not synchronized";
        return make_error_code(CryptoNote::error::DAEMON_NOT_SYNCED);
      }

      validateAddresses(request.sourceAddresses, currency, logger);
      validateAddresses(collectDestinationAddresses(request.transfers), currency, logger);
      std::vector<PaymentService::WalletRpcMessage> messages = collectMessages(request.transfers);
      if (!request.changeAddress.empty())
      {
        validateAddresses({request.changeAddress}, currency, logger);
      }

      CryptoNote::TransactionParameters sendParams;
      if (!request.paymentId.empty())
      {
        addPaymentIdToExtra(request.paymentId, sendParams.extra);
      }
      else
      {
        sendParams.extra = Common::asString(Common::fromHex(request.extra));
      }

      sendParams.sourceAddresses = request.sourceAddresses;
      sendParams.destinations = convertWalletRpcOrdersToWalletOrders(request.transfers);
      sendParams.messages = convertWalletRpcMessagesToWalletMessages(messages);
      sendParams.fee = 1000;
      sendParams.mixIn = parameters::MINIMUM_MIXIN;
      sendParams.unlockTimestamp = request.unlockTime;
      sendParams.changeDestination = request.changeAddress;

      Crypto::SecretKey transactionSK;
      size_t transactionId = wallet.transfer(sendParams, transactionSK);
      transactionHash = Common::podToHex(wallet.getTransaction(transactionId).hash);
      transactionSecretKey = Common::podToHex(transactionSK);
      logger(Logging::DEBUGGING) << "Transaction " << transactionHash << " has been sent";
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING) << "Error while sending transaction: " << x.what();
      return x.code();
    }
    catch (std::exception &x)
    {
      logger(Logging::WARNING) << "Error while sending transaction: " << x.what();
      return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
    }

    return std::error_code();
  }

  std::error_code WalletService::createDelayedTransaction(const CreateDelayedTransaction::Request &request, std::string &transactionHash)
  {
    try
    {
      System::EventLock lk(readyEvent);

      validateAddresses(request.addresses, currency, logger);
      validateAddresses(collectDestinationAddresses(request.transfers), currency, logger);
      std::vector<PaymentService::WalletRpcMessage> messages = collectMessages(request.transfers);
      if (!request.changeAddress.empty())
      {
        validateAddresses({request.changeAddress}, currency, logger);
      }

      CryptoNote::TransactionParameters sendParams;
      if (!request.paymentId.empty())
      {
        addPaymentIdToExtra(request.paymentId, sendParams.extra);
      }
      else
      {
        sendParams.extra = Common::asString(Common::fromHex(request.extra));
      }

      sendParams.sourceAddresses = request.addresses;
      sendParams.destinations = convertWalletRpcOrdersToWalletOrders(request.transfers);
      sendParams.messages = convertWalletRpcMessagesToWalletMessages(messages);
      sendParams.fee = request.fee;
      sendParams.mixIn = request.anonymity;
      sendParams.unlockTimestamp = request.unlockTime;
      sendParams.changeDestination = request.changeAddress;

      size_t transactionId = wallet.makeTransaction(sendParams);
      transactionHash = Common::podToHex(wallet.getTransaction(transactionId).hash);

      logger(Logging::DEBUGGING) << "Delayed transaction " << transactionHash << " has been created";
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING) << "Error while creating delayed transaction: " << x.what();
      return x.code();
    }
    catch (std::exception &x)
    {
      logger(Logging::WARNING) << "Error while creating delayed transaction: " << x.what();
      return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
    }

    return std::error_code();
  }

  std::error_code WalletService::createIntegratedAddress(const CreateIntegrated::Request &request, std::string &integrated_address)
  {
    std::string payment_id_str = request.payment_id;
    std::string address_str = request.address;

    uint64_t prefix;
    CryptoNote::AccountPublicAddress addr;

    /* Get the spend and view public keys from the address */
    const bool valid = CryptoNote::parseAccountAddressString(prefix,
                                                             addr,
                                                             address_str);

    CryptoNote::BinaryArray ba;
    CryptoNote::toBinaryArray(addr, ba);
    std::string keys = Common::asString(ba);

    logger(Logging::INFO) << "keys:" + keys;

    /* Create the integrated address the same way you make a public address */
    integrated_address = Tools::Base58::encode_addr(
        CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
        payment_id_str + keys);

    return std::error_code();
  }

  std::error_code WalletService::splitIntegratedAddress(const SplitIntegrated::Request &request, std::string &address, std::string &payment_id)
  {
    std::string integrated_address_str = request.integrated_address;

    /* Check that the integrated address the correct length */
    if (integrated_address_str.length() != 186)
    {
      return make_error_code(CryptoNote::error::BAD_INTEGRATED_ADDRESS);
    }

    /* Decode the address and extract the payment id */
    std::string decoded;
    uint64_t prefix;
    if (Tools::Base58::decode_addr(integrated_address_str, prefix, decoded))
    {
      payment_id = decoded.substr(0, 64);
    }

    /* Check if the prefix is correct */
    if (prefix != CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX)
    {
      return make_error_code(CryptoNote::error::BAD_PREFIX);
    }

    /* Create the address from the public keys */
    std::string keys = decoded.substr(64, 192);
    CryptoNote::AccountPublicAddress addr;
    CryptoNote::BinaryArray ba = Common::asBinaryArray(keys);

    /* Make sure the address is valid */
    if (!CryptoNote::fromBinaryArray(addr, ba))
    {
      return make_error_code(CryptoNote::error::BAD_ADDRESS);
    }

    /* Build the address */
    address = CryptoNote::getAccountAddressAsStr(prefix, addr);

    return std::error_code();
  }

  std::error_code WalletService::getDelayedTransactionHashes(std::vector<std::string> &transactionHashes)
  {
    try
    {
      System::EventLock lk(readyEvent);

      std::vector<size_t> transactionIds = wallet.getDelayedTransactionIds();
      transactionHashes.reserve(transactionIds.size());

      for (auto id : transactionIds)
      {
        transactionHashes.emplace_back(Common::podToHex(wallet.getTransaction(id).hash));
      }
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING) << "Error while getting delayed transaction hashes: " << x.what();
      return x.code();
    }
    catch (std::exception &x)
    {
      logger(Logging::WARNING) << "Error while getting delayed transaction hashes: " << x.what();
      return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
    }

    return std::error_code();
  }

  std::error_code WalletService::deleteDelayedTransaction(const std::string &transactionHash)
  {
    try
    {
      System::EventLock lk(readyEvent);

      parseHash(transactionHash, logger); //validate transactionHash parameter

      auto idIt = transactionIdIndex.find(transactionHash);
      if (idIt == transactionIdIndex.end())
      {
        return make_error_code(CryptoNote::error::WalletServiceErrorCode::OBJECT_NOT_FOUND);
      }

      size_t transactionId = idIt->second;
      wallet.rollbackUncommitedTransaction(transactionId);

      logger(Logging::DEBUGGING) << "Delayed transaction " << transactionHash << " has been canceled";
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING) << "Error while deleting delayed transaction hashes: " << x.what();
      return x.code();
    }
    catch (std::exception &x)
    {
      logger(Logging::WARNING) << "Error while deleting delayed transaction hashes: " << x.what();
      return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
    }

    return std::error_code();
  }

  std::error_code WalletService::sendDelayedTransaction(const std::string &transactionHash)
  {
    try
    {
      System::EventLock lk(readyEvent);

      parseHash(transactionHash, logger); //validate transactionHash parameter

      auto idIt = transactionIdIndex.find(transactionHash);
      if (idIt == transactionIdIndex.end())
      {
        return make_error_code(CryptoNote::error::WalletServiceErrorCode::OBJECT_NOT_FOUND);
      }

      size_t transactionId = idIt->second;
      wallet.commitTransaction(transactionId);

      logger(Logging::DEBUGGING) << "Delayed transaction " << transactionHash << " has been sent";
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING) << "Error while sending delayed transaction hashes: " << x.what();
      return x.code();
    }
    catch (std::exception &x)
    {
      logger(Logging::WARNING) << "Error while sending delayed transaction hashes: " << x.what();
      return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
    }

    return std::error_code();
  }

  std::error_code WalletService::getUnconfirmedTransactionHashes(const std::vector<std::string> &addresses, std::vector<std::string> &transactionHashes)
  {
    try
    {
      System::EventLock lk(readyEvent);

      validateAddresses(addresses, currency, logger);

      std::vector<CryptoNote::WalletTransactionWithTransfers> transactions = wallet.getUnconfirmedTransactions();

      TransactionsInBlockInfoFilter transactionFilter(addresses, "");

      for (const auto &transaction : transactions)
      {
        if (transactionFilter.checkTransaction(transaction))
        {
          transactionHashes.emplace_back(Common::podToHex(transaction.transaction.hash));
        }
      }
    }
    catch (std::system_error &x)
    {
      logger(Logging::WARNING) << "Error while getting unconfirmed transaction hashes: " << x.what();
      return x.code();
    }
    catch (std::exception &x)
    {
      logger(Logging::WARNING) << "Error while getting unconfirmed transaction hashes: " << x.what();
      return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
    }

    return std::error_code();
  }

  std::error_code WalletService::getStatus(
      uint32_t &blockCount,
      uint32_t &knownBlockCount,
      std::string &lastBlockHash,
      uint32_t &peerCount,
      uint32_t &depositCount,
      uint32_t &transactionCount,
      uint32_t &addressCount)
  {
    try
    {
      System::EventLock lk(readyEvent);

      auto estimateResult = fusionManager.estimate(1000000, {});
      knownBlockCount = node.getKnownBlockCount();
      peerCount = static_cast<uint32_t>(node.getPeerCount());
      blockCount = wallet.getBlockCount();
      depositCount = static_cast<uint32_t>(wallet.getWalletDepositCount());
      transactionCount = static_cast<uint32_t>(wallet.getTransactionCount());
      addressCount = static_cast<uint32_t>(wallet.getAddressCount());
      auto lastHashes = wallet.getBlockHashes(blockCount - 1, 1);
      lastBlockHash = Common::podToHex(lastHashes.back());
    }
      catch (std::system_error &x)
      {
        logger(Logging::WARNING) << "Error while getting status: " << x.what();
        return x.code();
      }
      catch (std::exception &x)
      {
        logger(Logging::WARNING) << "Error while getting status: " << x.what();
        return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
      }

      return std::error_code();
  }

    /* Create a new deposit for the wallet address specified. */
    std::error_code WalletService::createDeposit(
        uint64_t amount,
        uint64_t term,
        std::string sourceAddress,
        std::string & transactionHash)
    {
    
      try
      {

        uint64_t knownBlockCount = node.getKnownBlockCount();
        uint64_t localBlockCount = node.getLocalBlockCount();
        uint64_t diff = knownBlockCount - localBlockCount;
        if ((localBlockCount == 0) || (diff > 2))
        {
          logger(Logging::WARNING) << "Daemon is not synchronized";
          return make_error_code(CryptoNote::error::DAEMON_NOT_SYNCED);
        }
        
        System::EventLock lk(readyEvent);

        /* Validate the source addresse if it is are not empty */
        if (!sourceAddress.empty())
        {
          validateAddresses({sourceAddress}, currency, logger);
        }

        /* Now validate the deposit term and the amount */

        /* Deposits should be multiples of 21,900 blocks */
        if (term % CryptoNote::parameters::DEPOSIT_MIN_TERM_V3 != 0)
        {
          return make_error_code(CryptoNote::error::DEPOSIT_WRONG_TERM);
        }

        /* The minimum term should be 21,900 */
        if (term < CryptoNote::parameters::DEPOSIT_MIN_TERM_V3)
        {
          return make_error_code(CryptoNote::error::DEPOSIT_TERM_TOO_BIG);
        }

        /* Current deposit rates are for a maximum term of one year, 262800 */
        if (term > CryptoNote::parameters::DEPOSIT_MAX_TERM_V3)
        {
          return make_error_code(CryptoNote::error::DEPOSIT_TERM_TOO_BIG);
        }

        /* The minimum deposit amount is 1 CCX */
        if (amount < CryptoNote::parameters::DEPOSIT_MIN_AMOUNT)
        {
          return make_error_code(CryptoNote::error::DEPOSIT_AMOUNT_TOO_SMALL);
        }

        /* Create or send the deposit */
        wallet.createDeposit(amount, term, sourceAddress, sourceAddress, transactionHash);
      }

      catch (std::system_error &x)
      {
        logger(Logging::WARNING) << "Error: " << x.what();
        return x.code();
      }
      catch (std::exception &x)
      {
        logger(Logging::WARNING) << "Error : " << x.what();
        return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
      }

      return std::error_code();
    }

    std::error_code WalletService::withdrawDeposit(
        uint64_t depositId,
        std::string & transactionHash)

    {
      // TODO try and catch
      wallet.withdrawDeposit(depositId, transactionHash);
      return std::error_code();
    }

    /* Create and send a deposit to another wallet address, the deposit then will appear in their
   wallet upon confirmation. */
    std::error_code WalletService::sendDeposit(
        uint64_t amount,
        uint64_t term,
        std::string sourceAddress,
        std::string destinationAddress,
        std::string & transactionHash)
    {
      try
      {
        System::EventLock lk(readyEvent);

        /* Validate both the source and destination addresses
       if they are not empty */

        if (!sourceAddress.empty())
        {
          validateAddresses({sourceAddress}, currency, logger);
        }

        if (!destinationAddress.empty())
        {
          validateAddresses({destinationAddress}, currency, logger);
        }

        /* Now validate the deposit term and the amount */

        if (term < CryptoNote::parameters::DEPOSIT_MIN_TERM_V3)
        {
          return make_error_code(CryptoNote::error::DEPOSIT_TERM_TOO_SMALL);
        }

        if (term > CryptoNote::parameters::DEPOSIT_MAX_TERM_V3)
        {
          return make_error_code(CryptoNote::error::DEPOSIT_TERM_TOO_BIG);
        }

        if (amount < CryptoNote::parameters::DEPOSIT_MIN_AMOUNT)
        {
          return make_error_code(CryptoNote::error::DEPOSIT_AMOUNT_TOO_SMALL);
        }

        /* Create and send the deposit */
        wallet.createDeposit(amount, term, sourceAddress, destinationAddress, transactionHash);
      }

      catch (std::system_error &x)
      {
        logger(Logging::WARNING) << "Error: " << x.what();
        return x.code();
      }
      catch (std::exception &x)
      {
        logger(Logging::WARNING) << "Error : " << x.what();
        return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
      }
      return std::error_code();
    }

    std::error_code WalletService::getMessagesFromExtra(const std::string &extra, std::vector<std::string> &messages)
    {
      try
      {
        System::EventLock lk(readyEvent);

        std::vector<uint8_t> extraBin = Common::fromHex(extra);
        Crypto::PublicKey publicKey = CryptoNote::getTransactionPublicKeyFromExtra(extraBin);
        messages.clear();
        for (size_t i = 0; i < wallet.getAddressCount(); ++i)
        {
          Crypto::SecretKey secretKey = wallet.getAddressSpendKey(wallet.getAddress(i)).secretKey;
          std::vector<std::string> m = CryptoNote::get_messages_from_extra(extraBin, publicKey, &secretKey);
          messages.insert(std::end(messages), std::begin(m), std::end(m));
        }
      }
      catch (std::exception &e)
      {
        logger(Logging::WARNING) << "getMessagesFromExtra warning: " << e.what();
        return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
      }

      return std::error_code();
    }

    void WalletService::refresh()
    {
      try
      {
        logger(Logging::DEBUGGING) << "Refresh is started";
        for (;;)
        {
          auto event = wallet.getEvent();
          if (event.type == CryptoNote::TRANSACTION_CREATED)
          {
            size_t transactionId = event.transactionCreated.transactionIndex;
            transactionIdIndex.emplace(Common::podToHex(wallet.getTransaction(transactionId).hash), transactionId);
          }
        }
      }
      catch (std::system_error &e)
      {
        logger(Logging::DEBUGGING) << "refresh is stopped: " << e.what();
      }
      catch (std::exception &e)
      {
        logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "exception thrown in refresh(): " << e.what();
      }
    }

    std::error_code WalletService::estimateFusion(uint64_t threshold, const std::vector<std::string> &addresses,
                                                  uint32_t &fusionReadyCount, uint32_t &totalOutputCount)
    {

      uint64_t knownBlockCount = node.getKnownBlockCount();
      uint64_t localBlockCount = node.getLocalBlockCount();
      uint64_t diff = knownBlockCount - localBlockCount;
      if ((localBlockCount == 0) || (diff > 2))
      {
        logger(Logging::WARNING) << "Daemon is not synchronized";
        return make_error_code(CryptoNote::error::DAEMON_NOT_SYNCED);
      }

      try
      {
        System::EventLock lk(readyEvent);

        validateAddresses(addresses, currency, logger);

        auto estimateResult = fusionManager.estimate(threshold, addresses);
        fusionReadyCount = static_cast<uint32_t>(estimateResult.fusionReadyCount);
        totalOutputCount = static_cast<uint32_t>(estimateResult.totalOutputCount);
      }
      catch (std::system_error &x)
      {
        logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Failed to estimate number of fusion outputs: " << x.what();
        return x.code();
      }
      catch (std::exception &x)
      {
        logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Failed to estimate number of fusion outputs: " << x.what();
        return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
      }

      return std::error_code();
    }

    std::error_code WalletService::sendFusionTransaction(uint64_t threshold, uint32_t anonymity, const std::vector<std::string> &addresses,
                                                         const std::string &destinationAddress, std::string &transactionHash)
    {
      try
      {
        System::EventLock lk(readyEvent);

        uint64_t knownBlockCount = node.getKnownBlockCount();
        uint64_t localBlockCount = node.getLocalBlockCount();
        uint64_t diff = knownBlockCount - localBlockCount;
        if ((localBlockCount == 0) || (diff > 2))
        {
          logger(Logging::WARNING) << "Daemon is not synchronized";
          return make_error_code(CryptoNote::error::DAEMON_NOT_SYNCED);
        }

        validateAddresses(addresses, currency, logger);
        if (!destinationAddress.empty())
        {
          validateAddresses({destinationAddress}, currency, logger);
        }

        size_t transactionId = fusionManager.createFusionTransaction(threshold, 0, addresses, destinationAddress);
        transactionHash = Common::podToHex(wallet.getTransaction(transactionId).hash);

        logger(Logging::INFO) << "Fusion transaction " << transactionHash << " has been sent";
      }
      catch (std::system_error &x)
      {
        logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while sending fusion transaction: " << x.what();
        return x.code();
      }
      catch (std::exception &x)
      {
        logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while sending fusion transaction: " << x.what();
        return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
      }

      return std::error_code();
    }

    void WalletService::reset()
    {
      wallet.save(CryptoNote::WalletSaveLevel::SAVE_KEYS_ONLY);
      wallet.stop();
      wallet.shutdown();
      inited = false;
      refreshContext.wait();

      wallet.start();
      init();
    }

    void WalletService::replaceWithNewWallet(const Crypto::SecretKey &viewSecretKey)
    {
      wallet.stop();
      wallet.shutdown();
      inited = false;
      refreshContext.wait();

      transactionIdIndex.clear();

      size_t i = 0;
      for (;;)
      {
        boost::system::error_code ec;
        std::string backup = config.walletFile + ".backup";
        if (i != 0)
        {
          backup += "." + std::to_string(i);
        }

        if (!boost::filesystem::exists(backup))
        {
          boost::filesystem::rename(config.walletFile, backup);
          logger(Logging::DEBUGGING) << "Walletd file '" << config.walletFile << "' backed up to '" << backup << '\'';
          break;
        }
      }

      wallet.start();
      wallet.initializeWithViewKey(config.walletFile, config.walletPassword, viewSecretKey);
      inited = true;
    }

    std::error_code WalletService::replaceWithNewWallet(const std::string &viewSecretKeyText)
    {
      try
      {
        System::EventLock lk(readyEvent);

        Crypto::SecretKey viewSecretKey;
        if (!Common::podFromHex(viewSecretKeyText, viewSecretKey))
        {
          logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Cannot restore view secret key: " << viewSecretKeyText;
          return make_error_code(CryptoNote::error::WalletServiceErrorCode::WRONG_KEY_FORMAT);
        }

        Crypto::PublicKey viewPublicKey;
        if (!Crypto::secret_key_to_public_key(viewSecretKey, viewPublicKey))
        {
          logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Cannot derive view public key, wrong secret key: " << viewSecretKeyText;
          return make_error_code(CryptoNote::error::WalletServiceErrorCode::WRONG_KEY_FORMAT);
        }

        replaceWithNewWallet(viewSecretKey);
        logger(Logging::INFO, Logging::BRIGHT_WHITE) << "The container has been replaced";
      }
      catch (std::system_error &x)
      {
        logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while replacing container: " << x.what();
        return x.code();
      }
      catch (std::exception &x)
      {
        logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while replacing container: " << x.what();
        return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
      }

      return std::error_code();
    }

    std::vector<CryptoNote::TransactionsInBlockInfo> WalletService::getTransactions(const Crypto::Hash &blockHash, size_t blockCount) const
    {
      std::vector<CryptoNote::TransactionsInBlockInfo> result = wallet.getTransactions(blockHash, blockCount);
      if (result.empty())
      {
        throw std::system_error(make_error_code(CryptoNote::error::WalletServiceErrorCode::OBJECT_NOT_FOUND));
      }

      return result;
    }

    std::vector<CryptoNote::TransactionsInBlockInfo> WalletService::getTransactions(uint32_t firstBlockIndex, size_t blockCount) const
    {
      std::vector<CryptoNote::TransactionsInBlockInfo> result = wallet.getTransactions(firstBlockIndex, blockCount);
      if (result.empty())
      {
        throw std::system_error(make_error_code(CryptoNote::error::WalletServiceErrorCode::OBJECT_NOT_FOUND));
      }

      return result;
    }

    std::vector<CryptoNote::DepositsInBlockInfo> WalletService::getDeposits(const Crypto::Hash &blockHash, size_t blockCount) const
    {
      std::vector<CryptoNote::DepositsInBlockInfo> result = wallet.getDeposits(blockHash, blockCount);
      if (result.empty())
      {
        throw std::system_error(make_error_code(CryptoNote::error::WalletServiceErrorCode::OBJECT_NOT_FOUND));
      }

      return result;
    }

    std::vector<CryptoNote::DepositsInBlockInfo> WalletService::getDeposits(uint32_t firstBlockIndex, size_t blockCount) const
    {
      std::vector<CryptoNote::DepositsInBlockInfo> result = wallet.getDeposits(firstBlockIndex, blockCount);
      if (result.empty())
      {
        throw std::system_error(make_error_code(CryptoNote::error::WalletServiceErrorCode::OBJECT_NOT_FOUND));
      }

      return result;
    }

    std::vector<TransactionHashesInBlockRpcInfo> WalletService::getRpcTransactionHashes(const Crypto::Hash &blockHash, size_t blockCount, const TransactionsInBlockInfoFilter &filter) const
    {
      std::vector<CryptoNote::TransactionsInBlockInfo> allTransactions = getTransactions(blockHash, blockCount);
      std::vector<CryptoNote::TransactionsInBlockInfo> filteredTransactions = filterTransactions(allTransactions, filter);
      return convertTransactionsInBlockInfoToTransactionHashesInBlockRpcInfo(filteredTransactions);
    }

    std::vector<TransactionHashesInBlockRpcInfo> WalletService::getRpcTransactionHashes(uint32_t firstBlockIndex, size_t blockCount, const TransactionsInBlockInfoFilter &filter) const
    {
      std::vector<CryptoNote::TransactionsInBlockInfo> allTransactions = getTransactions(firstBlockIndex, blockCount);
      std::vector<CryptoNote::TransactionsInBlockInfo> filteredTransactions = filterTransactions(allTransactions, filter);
      return convertTransactionsInBlockInfoToTransactionHashesInBlockRpcInfo(filteredTransactions);
    }

    std::vector<TransactionsInBlockRpcInfo> WalletService::getRpcTransactions(const Crypto::Hash &blockHash, size_t blockCount, const TransactionsInBlockInfoFilter &filter) const
    {
      uint32_t knownBlockCount = node.getKnownBlockCount();
      std::vector<CryptoNote::TransactionsInBlockInfo> allTransactions = getTransactions(blockHash, blockCount);
      std::vector<CryptoNote::TransactionsInBlockInfo> filteredTransactions = filterTransactions(allTransactions, filter);
      return convertTransactionsInBlockInfoToTransactionsInBlockRpcInfo(filteredTransactions, knownBlockCount);
    }

    std::vector<TransactionsInBlockRpcInfo> WalletService::getRpcTransactions(uint32_t firstBlockIndex, size_t blockCount, const TransactionsInBlockInfoFilter &filter) const
    {
      uint32_t knownBlockCount = node.getKnownBlockCount();
      std::vector<CryptoNote::TransactionsInBlockInfo> allTransactions = getTransactions(firstBlockIndex, blockCount);
      std::vector<CryptoNote::TransactionsInBlockInfo> filteredTransactions = filterTransactions(allTransactions, filter);
      return convertTransactionsInBlockInfoToTransactionsInBlockRpcInfo(filteredTransactions, knownBlockCount);
    }

  } //namespace PaymentService

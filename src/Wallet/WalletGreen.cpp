// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "WalletGreen.h"

#include <algorithm>
#include <ctime>
#include <cassert>
#include <numeric>
#include <random>
#include <set>
#include <tuple>
#include <utility>
#include <fstream>
#include <System/EventLock.h>
#include <System/RemoteContext.h>

#include "ITransaction.h"

#include "Common/Base58.h"
#include "Common/ScopeExit.h"
#include "Common/ShuffleGenerator.h"
#include "Common/StdInputStream.h"
#include "Common/StdOutputStream.h"
#include "Common/StringTools.h"
#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/TransactionApi.h"
#include <CryptoNoteCore/TransactionExtra.h>
#include "crypto/crypto.h"
#include "Transfers/TransfersContainer.h"
#include "WalletSerializationV1.h"
#include "WalletSerializationV2.h"
#include "WalletErrors.h"
#include "WalletUtils.h"

using namespace common;
using namespace crypto;
using namespace cn;
using namespace logging;

namespace
{

  std::vector<uint64_t> split(uint64_t amount, uint64_t dustThreshold)
  {
    std::vector<uint64_t> amounts;

    decompose_amount_into_digits(
        amount, dustThreshold,
        [&amounts](uint64_t chunk) { amounts.push_back(chunk); },
        [&amounts](uint64_t dust) { amounts.push_back(dust); });

    return amounts;
  }

  uint64_t calculateDepositsAmount(
      const std::vector<cn::TransactionOutputInformation> &transfers,
      const cn::Currency &currency,
      const std::vector<uint32_t> &heights)
  {
    int index = 0;
    return std::accumulate(transfers.begin(), transfers.end(), static_cast<uint64_t>(0), [&currency, &index, &heights](uint64_t sum, const cn::TransactionOutputInformation &deposit) {
      return sum + deposit.amount + currency.calculateInterest(deposit.amount, deposit.term, heights[index++]);
    });
  }

  void asyncRequestCompletion(platform_system::Event &requestFinished)
  {
    requestFinished.set();
  }

  void parseAddressString(
      const std::string &string,
      const cn::Currency &currency,
      cn::AccountPublicAddress &address)
  {
    if (!currency.parseAccountAddressString(string, address))
    {
      throw std::system_error(make_error_code(cn::error::BAD_ADDRESS));
    }
  }

  uint64_t countNeededMoney(
      const std::vector<cn::WalletTransfer> &destinations,
      uint64_t fee)
  {
    uint64_t neededMoney = 0;
    for (const auto &transfer : destinations)
    {
      if (transfer.amount == 0)
      {
        throw std::system_error(make_error_code(cn::error::ZERO_DESTINATION));
      }
      else if (transfer.amount < 0)
      {
        throw std::system_error(make_error_code(std::errc::invalid_argument));
      }

      //to supress warning
      auto amount = static_cast<uint64_t>(transfer.amount);
      neededMoney += amount;
      if (neededMoney < amount)
      {
        throw std::system_error(make_error_code(cn::error::SUM_OVERFLOW));
      }
    }

    neededMoney += fee;
    if (neededMoney < fee)
    {
      throw std::system_error(make_error_code(cn::error::SUM_OVERFLOW));
    }

    return neededMoney;
  }

  void checkIfEnoughMixins(std::vector<outs_for_amount> &mixinResult, uint64_t mixIn)
  {
    auto notEnoughIt = std::find_if(mixinResult.begin(), mixinResult.end(),
                                    [mixIn](const outs_for_amount &ofa) { return ofa.outs.size() < mixIn; });

    if (mixIn == 0 && mixinResult.empty())
    {
      throw std::system_error(make_error_code(cn::error::MIXIN_COUNT_TOO_BIG));
    }

    if (notEnoughIt != mixinResult.end())
    {
      throw std::system_error(make_error_code(cn::error::MIXIN_COUNT_TOO_BIG));
    }
  }

  size_t getTransactionSize(const ITransactionReader &transaction)
  {
    return transaction.getTransactionData().size();
  }

  std::vector<WalletTransfer> convertOrdersToTransfers(const std::vector<WalletOrder> &orders)
  {
    std::vector<WalletTransfer> transfers;
    transfers.reserve(orders.size());

    for (const auto &order : orders)
    {
      WalletTransfer transfer;

      if (order.amount > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
      {
        throw std::system_error(make_error_code(cn::error::WRONG_AMOUNT),
                                "Order amount must not exceed " + std::to_string(std::numeric_limits<decltype(transfer.amount)>::max()));
      }

      transfer.type = WalletTransferType::USUAL;
      transfer.address = order.address;
      transfer.amount = static_cast<int64_t>(order.amount);

      transfers.emplace_back(std::move(transfer));
    }

    return transfers;
  }

  uint64_t calculateDonationAmount(uint64_t freeAmount, uint64_t donationThreshold, uint64_t dustThreshold)
  {
    std::vector<uint64_t> decomposedAmounts;
    decomposeAmount(freeAmount, dustThreshold, decomposedAmounts);

    std::sort(decomposedAmounts.begin(), decomposedAmounts.end(), std::greater<uint64_t>());

    uint64_t donationAmount = 0;
    for (auto amount : decomposedAmounts)
    {
      if (amount > donationThreshold - donationAmount)
      {
        continue;
      }

      donationAmount += amount;
    }

    assert(donationAmount <= freeAmount);
    return donationAmount;
  }

  uint64_t pushDonationTransferIfPossible(const DonationSettings &donation, uint64_t freeAmount, uint64_t dustThreshold, std::vector<WalletTransfer> &destinations)
  {
    uint64_t donationAmount = 0;
    if (!donation.address.empty() && donation.threshold != 0)
    {
      if (donation.threshold > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
      {
        throw std::system_error(make_error_code(error::WRONG_AMOUNT),
                                "Donation threshold must not exceed " + std::to_string(std::numeric_limits<int64_t>::max()));
      }

      donationAmount = calculateDonationAmount(freeAmount, donation.threshold, dustThreshold);
      if (donationAmount != 0)
      {
        destinations.emplace_back(WalletTransfer{WalletTransferType::DONATION, donation.address, static_cast<int64_t>(donationAmount)});
      }
    }

    return donationAmount;
  }

  cn::AccountPublicAddress parseAccountAddressString(
      const std::string &addressString,
      const cn::Currency &currency)
  {
    cn::AccountPublicAddress address;

    if (!currency.parseAccountAddressString(addressString, address))
    {
      throw std::system_error(make_error_code(cn::error::BAD_ADDRESS));
    }

    return address;
  }

} // namespace

namespace cn
{

  WalletGreen::WalletGreen(platform_system::Dispatcher &dispatcher, const Currency &currency, INode &node, logging::ILogger &logger, uint32_t transactionSoftLockTime) : m_dispatcher(dispatcher),
                                                                                                                                                                m_currency(currency),
                                                                                                                                                                m_node(node),
                                                                                                                                                                m_logger(logger, "WalletGreen"),
                                                                                                                                                                m_blockchainSynchronizer(node, currency.genesisBlockHash()),
                                                                                                                                                                m_synchronizer(currency, logger, m_blockchainSynchronizer, node),
                                                                                                                                                                m_eventOccurred(m_dispatcher),
                                                                                                                                                                m_readyEvent(m_dispatcher),
                                                                                                                                                                m_transactionSoftLockTime(transactionSoftLockTime)
  {
    m_upperTransactionSizeLimit = m_currency.transactionMaxSize();
    m_readyEvent.set();
  }

  WalletGreen::~WalletGreen()
  {
    if (m_state == WalletState::INITIALIZED)
    {
      doShutdown();
    }

    m_dispatcher.yield(); //let remote spawns finish
  }

  void WalletGreen::initialize(
      const std::string &path,
      const std::string &password)
  {
    crypto::PublicKey viewPublicKey;
    crypto::SecretKey viewSecretKey;
    crypto::generate_keys(viewPublicKey, viewSecretKey);
    initWithKeys(path, password, viewPublicKey, viewSecretKey);
    m_logger(DEBUGGING, BRIGHT_WHITE) << "New container initialized, public view key " << common::podToHex(viewPublicKey);
  }

  void WalletGreen::withdrawDeposit(
      DepositId depositId,
      std::string &transactionHash)
  {

    throwIfNotInitialized();
    throwIfTrackingMode();
    throwIfStopped();

    /* Check for the existance of the deposit */
    if (m_deposits.size() <= depositId)
    {
      throw std::system_error(make_error_code(cn::error::DEPOSIT_DOESNOT_EXIST));
    }

    /* Get the details of the deposit, and the address */
    Deposit deposit = getDeposit(depositId);
    WalletTransfer firstTransfer = getTransactionTransfer(deposit.creatingTransactionId, 0);
    std::string address = firstTransfer.address;

    uint64_t blockCount = getBlockCount();

    /* Is the deposit unlocked */
    if (deposit.unlockHeight > blockCount)
    {
      throw std::system_error(make_error_code(cn::error::DEPOSIT_LOCKED));
    }

    /* Create the transaction */
    std::unique_ptr<ITransaction> transaction = createTransaction();

    std::vector<TransactionOutputInformation> selectedTransfers;

    const auto &wallet = getWalletRecord(address);
    const ITransfersContainer *container = wallet.container;
    AccountKeys account = makeAccountKeys(wallet);
    ITransfersContainer::TransferState state;
    TransactionOutputInformation transfer;

    uint64_t foundMoney = 0;
    foundMoney += deposit.amount + deposit.interest;
    m_logger(DEBUGGING, WHITE) << "found money " << foundMoney;

    container->getTransfer(deposit.transactionHash, deposit.outputInTransaction, transfer, state);

    if (state != ITransfersContainer::TransferState::TransferAvailable) 
    {
      throw std::system_error(make_error_code(cn::error::DEPOSIT_LOCKED));
    }

    selectedTransfers.push_back(std::move(transfer));
    m_logger(DEBUGGING, BRIGHT_WHITE) << "Withdraw deposit, id " << depositId << " found transfer for " << transfer.amount << " with a global output index of " << transfer.globalOutputIndex;

    std::vector<MultisignatureInput> inputs = prepareMultisignatureInputs(selectedTransfers);

    for (const auto &input : inputs)
    {
      transaction->addInput(input);
    }

    std::vector<uint64_t> outputAmounts = split(foundMoney - 10, parameters::DEFAULT_DUST_THRESHOLD);

    for (auto amount : outputAmounts)
    {
      transaction->addOutput(amount, account.address);
    }

    transaction->setUnlockTime(0);
    crypto::SecretKey transactionSK;
    transaction->getTransactionSecretKey(transactionSK);

    assert(inputs.size() == selectedTransfers.size());
    for (size_t i = 0; i < inputs.size(); ++i)
    {
      transaction->signInputMultisignature(i, selectedTransfers[i].transactionPublicKey, selectedTransfers[i].outputInTransaction, account);
    }

    transactionHash = common::podToHex(transaction->getTransactionHash());
    validateSaveAndSendTransaction(*transaction, {}, false, true);
  }

  crypto::SecretKey WalletGreen::getTransactionDeterministicSecretKey(crypto::Hash &transactionHash) const
  {
    throwIfNotInitialized();
    throwIfStopped();

    crypto::SecretKey txKey = cn::NULL_SECRET_KEY;

    auto getTransactionCompleted = std::promise<std::error_code>();
    auto getTransactionWaitFuture = getTransactionCompleted.get_future();
    cn::Transaction tx;
    m_node.getTransaction(transactionHash, std::ref(tx),
                          [&getTransactionCompleted](std::error_code ec) {
                            auto detachedPromise = std::move(getTransactionCompleted);
                            detachedPromise.set_value(ec);
                          });
    std::error_code ec = getTransactionWaitFuture.get();
    if (ec)
    {
      m_logger(ERROR) << "Failed to get tx: " << ec << ", " << ec.message();
      return cn::NULL_SECRET_KEY;
    }

    crypto::PublicKey txPubKey = getTransactionPublicKeyFromExtra(tx.extra);
    KeyPair deterministicTxKeys;
    bool ok = generateDeterministicTransactionKeys(tx, m_viewSecretKey, deterministicTxKeys) && deterministicTxKeys.publicKey == txPubKey;

    return ok ? deterministicTxKeys.secretKey : cn::NULL_SECRET_KEY;

    return txKey;
  }

  std::vector<MultisignatureInput> WalletGreen::prepareMultisignatureInputs(const std::vector<TransactionOutputInformation> &selectedTransfers) const
  {
    std::vector<MultisignatureInput> inputs;
    inputs.reserve(selectedTransfers.size());

    for (const auto &output : selectedTransfers)
    {
      assert(output.type == transaction_types::OutputType::Multisignature);
      assert(output.requiredSignatures == 1); //Other types are currently unsupported

      MultisignatureInput input;
      input.amount = output.amount;
      input.signatureCount = static_cast<uint8_t>(output.requiredSignatures);
      input.outputIndex = output.globalOutputIndex;
      input.term = output.term;

      inputs.emplace_back(std::move(input));
    }

    return inputs;
  }

  void WalletGreen::createDeposit(
      uint64_t amount,
      uint32_t term,
      std::string sourceAddress,
      std::string destinationAddress,
      std::string &transactionHash)
  {

    throwIfNotInitialized();
    throwIfTrackingMode();
    throwIfStopped();

    /* If a source address is not specified, use the primary (first) wallet
       address for the creation of the deposit */
    if (sourceAddress.empty())
    {
      sourceAddress = getAddress(0);
    }

    if (destinationAddress.empty())
    {
      destinationAddress = sourceAddress;
    }

    /* Ensure that the address is valid and a part of this container */
    validateSourceAddresses({sourceAddress});

    cn::AccountPublicAddress sourceAddr = parseAddress(sourceAddress);
    cn::AccountPublicAddress destAddr = parseAddress(destinationAddress);

    /* Create the transaction */
    std::unique_ptr<ITransaction> transaction = createTransaction();

    /* Select the wallet - If no source address was specified then it will pick funds from anywhere
     and the change will go to the primary address of the wallet container */
    std::vector<WalletOuts> wallets;
    wallets = pickWallets({sourceAddress});

    /* Select the transfers */
    uint64_t fee = 1000;
    uint64_t neededMoney = amount + fee;
    std::vector<OutputToTransfer> selectedTransfers;
    uint64_t foundMoney = selectTransfers(neededMoney,
                                          m_currency.defaultDustThreshold(),
                                          wallets,
                                          selectedTransfers);

    /* Do we have enough funds */
    if (foundMoney < neededMoney)
    {
      throw std::system_error(make_error_code(error::WRONG_AMOUNT));
    }

    /* Now we add the outputs to the transaction, starting with the deposits output
     which includes the term, and then after that the change outputs */

    /* Add the deposit outputs to the transaction */
    transaction->addOutput(
        neededMoney - fee,
        {destAddr},
        1,
        term);

    /* Let's add the change outputs to the transaction */
    std::vector<uint64_t> decomposedChange = split(foundMoney - neededMoney, m_currency.defaultDustThreshold());

    /* Now pair each of those amounts to the change address
     which in the case of a deposit is the source address */
    using AmountToAddress = std::pair<const AccountPublicAddress *, uint64_t>;
    std::vector<AmountToAddress> amountsToAddresses;
    for (const auto &output : decomposedChange)
    {
      amountsToAddresses.emplace_back(AmountToAddress{&sourceAddr, output});
    }

    /* For the sake of privacy, we shuffle the output order randomly */
    std::shuffle(amountsToAddresses.begin(), amountsToAddresses.end(), std::default_random_engine{crypto::rand<std::default_random_engine::result_type>()});
    std::sort(amountsToAddresses.begin(), amountsToAddresses.end(), [](const AmountToAddress &left, const AmountToAddress &right) {
      return left.second < right.second;
    });

    /* Add the change outputs to the transaction */
    try
    {
      for (const auto &amountToAddress : amountsToAddresses)
      {
        transaction->addOutput(amountToAddress.second,
                               *amountToAddress.first);
      }
    }

    catch (const std::exception &e)
    {
      std::cerr << e.what() << '\n';
    }

    /* Now add the other components of the transaction such as the transaction secret key, unlocktime
     since this is a deposit, we don't need to add messages or added extras beyond the transaction publick key */
    crypto::SecretKey transactionSK;
    transaction->getTransactionSecretKey(transactionSK);
    transaction->setUnlockTime(0);

    /* Prepare the inputs */

    /* Get additional inputs for the mixin */
    std::vector<outs_for_amount> mixinResult;
    requestMixinOuts(selectedTransfers, cn::parameters::MINIMUM_MIXIN, mixinResult);
    std::vector<InputInfo> keysInfo;
    prepareInputs(selectedTransfers, mixinResult, cn::parameters::MINIMUM_MIXIN, keysInfo);

    /* Add the inputs to the transaction */
    std::vector<KeyPair> ephKeys;
    for (auto &input : keysInfo)
    {
      transaction->addInput(makeAccountKeys(*input.walletRecord), input.keyInfo, input.ephKeys);
    }

    /* Now sign the inputs so we can proceed with the transaction */
    size_t i = 0;
    for (const auto &input : keysInfo)
    {
      transaction->signInputKey(i++, input.keyInfo, input.ephKeys);
    }

    /* Return the transaction hash */
    transactionHash = common::podToHex(transaction->getTransactionHash());
    validateSaveAndSendTransaction(*transaction, {}, false, true);
  }

  void WalletGreen::validateOrders(const std::vector<WalletOrder> &orders) const
  {
    for (const auto &order : orders)
    {
      if (!cn::validateAddress(order.address, m_currency))
      {
        throw std::system_error(make_error_code(cn::error::BAD_ADDRESS));
      }

      if (order.amount >= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
      {
        std::string message = "Order amount must not exceed " + m_currency.formatAmount(std::numeric_limits<int64_t>::max());
        throw std::system_error(make_error_code(cn::error::WRONG_AMOUNT), message);
      }
    }
  }

  void WalletGreen::decryptKeyPair(const EncryptedWalletRecord &cipher, PublicKey &publicKey, SecretKey &secretKey,
                                   uint64_t &creationTimestamp, const crypto::chacha8_key &key)
  {

    std::array<char, sizeof(cipher.data)> buffer;
    chacha8(cipher.data, sizeof(cipher.data), key, cipher.iv, buffer.data());

    MemoryInputStream stream(buffer.data(), buffer.size());
    BinaryInputStreamSerializer serializer(stream);

    serializer(publicKey, "publicKey");
    serializer(secretKey, "secretKey");
    serializer.binary(&creationTimestamp, sizeof(uint64_t), "creationTimestamp");
  }

  void WalletGreen::decryptKeyPair(const EncryptedWalletRecord &cipher, PublicKey &publicKey, SecretKey &secretKey, uint64_t &creationTimestamp) const
  {
    decryptKeyPair(cipher, publicKey, secretKey, creationTimestamp, m_key);
  }

  EncryptedWalletRecord WalletGreen::encryptKeyPair(const PublicKey &publicKey, const SecretKey &secretKey, uint64_t creationTimestamp, const crypto::chacha8_key &key, const crypto::chacha8_iv &iv)
  {

    EncryptedWalletRecord result;

    std::string serializedKeys;
    StringOutputStream outputStream(serializedKeys);
    BinaryOutputStreamSerializer serializer(outputStream);

    serializer(const_cast<PublicKey &>(publicKey), "publicKey");
    serializer(const_cast<SecretKey &>(secretKey), "secretKey");
    serializer.binary(&creationTimestamp, sizeof(uint64_t), "creationTimestamp");

    assert(serializedKeys.size() == sizeof(result.data));

    result.iv = iv;
    chacha8(serializedKeys.data(), serializedKeys.size(), key, result.iv, reinterpret_cast<char *>(result.data));

    return result;
  }

  crypto::chacha8_iv WalletGreen::getNextIv() const
  {
    const auto *prefix = reinterpret_cast<const ContainerStoragePrefix *>(m_containerStorage.prefix());
    return prefix->nextIv;
  }

  EncryptedWalletRecord WalletGreen::encryptKeyPair(const PublicKey &publicKey, const SecretKey &secretKey, uint64_t creationTimestamp) const
  {
    return encryptKeyPair(publicKey, secretKey, creationTimestamp, m_key, getNextIv());
  }

  void WalletGreen::loadSpendKeys()
  {
    bool isTrackingMode;
    for (size_t i = 0; i < m_containerStorage.size(); ++i)
    {
      WalletRecord wallet;
      uint64_t creationTimestamp;
      decryptKeyPair(m_containerStorage[i], wallet.spendPublicKey, wallet.spendSecretKey, creationTimestamp);
      wallet.creationTimestamp = creationTimestamp;

      if (i == 0)
      {
        isTrackingMode = wallet.spendSecretKey == NULL_SECRET_KEY;
      }
      else if ((isTrackingMode && wallet.spendSecretKey != NULL_SECRET_KEY) || (!isTrackingMode && wallet.spendSecretKey == NULL_SECRET_KEY))
      {
        throw std::system_error(make_error_code(error::BAD_ADDRESS), "All addresses must be whether tracking or not");
      }

      if (wallet.spendSecretKey != NULL_SECRET_KEY)
      {
        throwIfKeysMissmatch(wallet.spendSecretKey, wallet.spendPublicKey, "Restored spend public key doesn't correspond to secret key");
      }
      else
      {
        if (!crypto::check_key(wallet.spendPublicKey))
        {
          throw std::system_error(make_error_code(error::WRONG_PASSWORD), "Public spend key is incorrect");
        }
      }

      wallet.actualBalance = 0;
      wallet.pendingBalance = 0;
      wallet.lockedDepositBalance = 0;
      wallet.unlockedDepositBalance = 0;
      wallet.container = reinterpret_cast<cn::ITransfersContainer *>(i); //dirty hack. container field must be unique

      m_walletsContainer.emplace_back(std::move(wallet));
    }
  }

  void WalletGreen::validateAddresses(const std::vector<std::string> &addresses) const
  {
    for (const auto &address : addresses)
    {
      if (!cn::validateAddress(address, m_currency))
      {
        throw std::system_error(make_error_code(cn::error::BAD_ADDRESS));
      }
    }
  }

  void WalletGreen::initializeWithViewKey(const std::string &path, const std::string &password, const crypto::SecretKey &viewSecretKey)
  {
    crypto::PublicKey viewPublicKey;
    if (!crypto::secret_key_to_public_key(viewSecretKey, viewPublicKey))
    {
      m_logger(ERROR, BRIGHT_RED) << "initializeWithViewKey(" << common::podToHex(viewSecretKey) << ") Failed to convert secret key to public key";
      throw std::system_error(make_error_code(cn::error::KEY_GENERATION_ERROR));
    }

    initWithKeys(path, password, viewPublicKey, viewSecretKey);
    m_logger(INFO, BRIGHT_WHITE) << "Container initialized with view secret key, public view key " << common::podToHex(viewPublicKey);
  }

  void WalletGreen::generateNewWallet(const std::string &path, const std::string &password)
  {
    crypto::SecretKey viewSecretKey;
    cn::KeyPair spendKey;

    crypto::generate_keys(spendKey.publicKey, spendKey.secretKey);

    crypto::PublicKey viewPublicKey;

    cn::AccountBase::generateViewFromSpend(spendKey.secretKey, viewSecretKey, viewPublicKey);

    initializeWithViewKey(path, password, viewSecretKey);
    createAddress(spendKey.secretKey);
  }

  void WalletGreen::shutdown()
  {
    throwIfNotInitialized();
    doShutdown();

    m_dispatcher.yield(); //let remote spawns finish
  }

  void WalletGreen::initBlockchain(const crypto::PublicKey &viewPublicKey)
  {
    std::vector<crypto::Hash> blockchain = m_synchronizer.getViewKeyKnownBlocks(viewPublicKey);
    m_blockchain.insert(m_blockchain.end(), blockchain.begin(), blockchain.end());
  }

  void WalletGreen::deleteOrphanTransactions(const std::unordered_set<crypto::PublicKey> &deletedKeys)
  {
    for (const auto &spendPublicKey : deletedKeys)
    {
      AccountPublicAddress deletedAccountAddress;
      deletedAccountAddress.spendPublicKey = spendPublicKey;
      deletedAccountAddress.viewPublicKey = m_viewPublicKey;
      auto deletedAddressString = m_currency.accountAddressAsString(deletedAccountAddress);

      std::vector<size_t> deletedTransactions;
      std::vector<size_t> updatedTransactions = deleteTransfersForAddress(deletedAddressString, deletedTransactions);
      deleteFromUncommitedTransactions(deletedTransactions);
    }
  }

  void WalletGreen::saveWalletCache(ContainerStorage &storage, const crypto::chacha8_key &key, WalletSaveLevel saveLevel, const std::string &extra)
  {
    m_logger(INFO) << "Saving cache...";

    WalletTransactions transactions;
    WalletTransfers transfers;
    if (saveLevel == WalletSaveLevel::SAVE_KEYS_AND_TRANSACTIONS)
    {
      filterOutTransactions(transactions, transfers, [](const WalletTransaction &tx) {
        return tx.state == WalletTransactionState::CREATED || tx.state == WalletTransactionState::DELETED;
      });

      for (auto it = transactions.begin(); it != transactions.end(); ++it)
      {
        transactions.modify(it, [](WalletTransaction &tx) {
          tx.state = WalletTransactionState::CANCELLED;
          tx.blockHeight = WALLET_UNCONFIRMED_TRANSACTION_HEIGHT;
        });
      }
    }
    else if (saveLevel == WalletSaveLevel::SAVE_ALL)
    {
      filterOutTransactions(transactions, transfers, [](const WalletTransaction &tx) {
        return tx.state == WalletTransactionState::DELETED;
      });
    }

    std::string containerData;
    common::StringOutputStream containerStream(containerData);
    WalletSerializerV2 s(
        *this,
        m_viewPublicKey,
        m_viewSecretKey,
        m_actualBalance,
        m_pendingBalance,
        m_lockedDepositBalance,
        m_unlockedDepositBalance,
        m_walletsContainer,
        m_synchronizer,
        m_unlockTransactionsJob,
        transactions,
        transfers,
        m_deposits,
        m_uncommitedTransactions,
        const_cast<std::string &>(extra),
        m_transactionSoftLockTime);
    s.save(containerStream, saveLevel);
    encryptAndSaveContainerData(storage, key, containerData.data(), containerData.size());
    storage.flush();

    m_extra = extra;

    m_logger(INFO) << "Container saving finished";
  }

  void WalletGreen::doShutdown()
  {
    if (m_walletsContainer.size() != 0)
    {
      m_synchronizer.unsubscribeConsumerNotifications(m_viewPublicKey, this);
    }

    stopBlockchainSynchronizer();
    m_blockchainSynchronizer.removeObserver(this);

    m_containerStorage.close();
    m_walletsContainer.clear();
    clearCaches(true, true);

    std::queue<WalletEvent> noEvents;
    std::swap(m_events, noEvents);

    m_state = WalletState::NOT_INITIALIZED;
  }

  void WalletGreen::initTransactionPool()
  {
    std::unordered_set<crypto::Hash> uncommitedTransactionsSet;
    std::transform(m_uncommitedTransactions.begin(), m_uncommitedTransactions.end(), std::inserter(uncommitedTransactionsSet, uncommitedTransactionsSet.end()),
                   [](const UncommitedTransactions::value_type &pair) {
                     return getObjectHash(pair.second);
                   });
    m_synchronizer.initTransactionPool(uncommitedTransactionsSet);
  }

  void WalletGreen::initWithKeys(const std::string &path, const std::string &password,
                                 const crypto::PublicKey &viewPublicKey, const crypto::SecretKey &viewSecretKey)
  {

    if (m_state != WalletState::NOT_INITIALIZED)
    {
      m_logger(ERROR, BRIGHT_RED) << "Failed to initialize with keys: already initialized.";
      throw std::system_error(make_error_code(cn::error::ALREADY_INITIALIZED));
    }

    throwIfStopped();

    ContainerStorage newStorage(path, common::FileMappedVectorOpenMode::CREATE, sizeof(ContainerStoragePrefix));
    ContainerStoragePrefix *prefix = reinterpret_cast<ContainerStoragePrefix *>(newStorage.prefix());
    prefix->version = WalletSerializerV2::SERIALIZATION_VERSION;
    prefix->nextIv = crypto::rand<crypto::chacha8_iv>();

    crypto::cn_context cnContext;
    crypto::generate_chacha8_key(cnContext, password, m_key);

    uint64_t creationTimestamp = time(nullptr);
    prefix->encryptedViewKeys = encryptKeyPair(viewPublicKey, viewSecretKey, creationTimestamp, m_key, prefix->nextIv);

    newStorage.flush();
    m_containerStorage.swap(newStorage);
    incNextIv();

    m_viewPublicKey = viewPublicKey;
    m_viewSecretKey = viewSecretKey;
    m_password = password;
    m_path = path;
    m_logger = logging::LoggerRef(m_logger.getLogger(), "WalletGreen/" + podToHex(m_viewPublicKey).substr(0, 5));

    assert(m_blockchain.empty());
    m_blockchain.push_back(m_currency.genesisBlockHash());

    m_blockchainSynchronizer.addObserver(this);

    m_state = WalletState::INITIALIZED;
  }

  void WalletGreen::save(WalletSaveLevel saveLevel, const std::string &extra)
  {
    m_logger(INFO, BRIGHT_WHITE) << "Saving container...";

    throwIfNotInitialized();
    throwIfStopped();

    stopBlockchainSynchronizer();

    try
    {
      saveWalletCache(m_containerStorage, m_key, saveLevel, extra);
    }
    catch (const std::exception &e)
    {
      m_logger(ERROR, BRIGHT_RED) << "Failed to save container: " << e.what();
      m_observerManager.notify(&IWalletObserver::saveCompleted, make_error_code(cn::error::INTERNAL_WALLET_ERROR));
      startBlockchainSynchronizer();
      throw;
    }

    startBlockchainSynchronizer();
    m_logger(INFO, BRIGHT_WHITE) << "Container saved";
    m_observerManager.notify(&IWalletObserver::saveCompleted, std::error_code());
  }

  void WalletGreen::copyContainerStorageKeys(const ContainerStorage &src, const chacha8_key &srcKey, ContainerStorage &dst, const chacha8_key &dstKey) const
  {
    dst.reserve(src.size());

    dst.setAutoFlush(false);
    tools::ScopeExit exitHandler([&dst] {
      dst.setAutoFlush(true);
      dst.flush();
    });

    for (const auto &encryptedSpendKeys : src)
    {
      crypto::PublicKey publicKey;
      crypto::SecretKey secretKey;
      uint64_t creationTimestamp;
      decryptKeyPair(encryptedSpendKeys, publicKey, secretKey, creationTimestamp, srcKey);

      // push_back() can resize container, and dstPrefix address can be changed, so it is requested for each key pair
      ContainerStoragePrefix *dstPrefix = reinterpret_cast<ContainerStoragePrefix *>(dst.prefix());
      crypto::chacha8_iv keyPairIv = dstPrefix->nextIv;
      incIv(dstPrefix->nextIv);

      dst.push_back(encryptKeyPair(publicKey, secretKey, creationTimestamp, dstKey, keyPairIv));
    }
  }

  void WalletGreen::copyContainerStoragePrefix(ContainerStorage &src, const chacha8_key &srcKey, ContainerStorage &dst, const chacha8_key &dstKey)
  {
    const ContainerStoragePrefix *srcPrefix = reinterpret_cast<ContainerStoragePrefix *>(src.prefix());
    ContainerStoragePrefix *dstPrefix = reinterpret_cast<ContainerStoragePrefix *>(dst.prefix());
    dstPrefix->version = srcPrefix->version;
    dstPrefix->nextIv = crypto::randomChachaIV();

    crypto::PublicKey publicKey;
    crypto::SecretKey secretKey;
    uint64_t creationTimestamp;
    decryptKeyPair(srcPrefix->encryptedViewKeys, publicKey, secretKey, creationTimestamp, srcKey);
    dstPrefix->encryptedViewKeys = encryptKeyPair(publicKey, secretKey, creationTimestamp, dstKey, dstPrefix->nextIv);
    incIv(dstPrefix->nextIv);
  }

  void WalletGreen::exportWallet(const std::string &path, WalletSaveLevel saveLevel, bool encrypt, const std::string &extra)
  {
    m_logger(INFO, BRIGHT_WHITE) << "Exporting container...";

    throwIfNotInitialized();
    throwIfStopped();
    stopBlockchainSynchronizer();

    try
    {
      bool storageCreated = false;
      tools::ScopeExit failExitHandler([path, &storageCreated] {
        // Don't delete file if it has existed
        if (storageCreated)
        {
          boost::system::error_code ignore;
          boost::filesystem::remove(path, ignore);
        }
      });

      ContainerStorage newStorage(path, FileMappedVectorOpenMode::CREATE, m_containerStorage.prefixSize());
      storageCreated = true;

      chacha8_key newStorageKey;
      if (encrypt)
      {
        newStorageKey = m_key;
      }
      else
      {
        cn_context cnContext;
        generate_chacha8_key(cnContext, "", newStorageKey);
      }

      copyContainerStoragePrefix(m_containerStorage, m_key, newStorage, newStorageKey);
      copyContainerStorageKeys(m_containerStorage, m_key, newStorage, newStorageKey);
      saveWalletCache(newStorage, newStorageKey, saveLevel, extra);

      failExitHandler.cancel();

      m_logger(INFO) << "Container export finished";
    }
    catch (const std::exception &e)
    {
      m_logger(ERROR, BRIGHT_RED) << "Failed to export container: " << e.what();
      startBlockchainSynchronizer();
      throw;
    }

    startBlockchainSynchronizer();
    m_logger(INFO, BRIGHT_WHITE) << "Container exported";
  }

  void WalletGreen::convertAndLoadWalletFile(const std::string &path, std::ifstream &&walletFileStream)
  {

    WalletSerializer s(
        *this,
        m_viewPublicKey,
        m_viewSecretKey,
        m_actualBalance,
        m_pendingBalance,
        m_walletsContainer,
        m_synchronizer,
        m_unlockTransactionsJob,
        m_transactions,
        m_transfers,
        m_transactionSoftLockTime,
        m_uncommitedTransactions);

    StdInputStream stream(walletFileStream);
    s.load(m_key, stream);
    walletFileStream.close();

    boost::filesystem::path bakPath = path + ".backup";
    boost::filesystem::path tmpPath = boost::filesystem::unique_path(path + ".tmp.%%%%-%%%%");
    if (boost::filesystem::exists(bakPath))
    {
      m_logger(INFO) << "Wallet backup already exists! Creating random file name backup.";
      bakPath = boost::filesystem::unique_path(path + ".%%%%-%%%%" + ".backup");
    }

    tools::ScopeExit tmpFileDeleter([&tmpPath] {
      boost::system::error_code ignore;
      boost::filesystem::remove(tmpPath, ignore);
    });
    m_containerStorage.open(tmpPath.string(), common::FileMappedVectorOpenMode::CREATE, sizeof(ContainerStoragePrefix));
    ContainerStoragePrefix *prefix = reinterpret_cast<ContainerStoragePrefix *>(m_containerStorage.prefix());
    prefix->version = WalletSerializerV2::SERIALIZATION_VERSION;
    prefix->nextIv = crypto::randomChachaIV();
    uint64_t creationTimestamp = time(nullptr);
    prefix->encryptedViewKeys = encryptKeyPair(m_viewPublicKey, m_viewSecretKey, creationTimestamp);
    for (const auto &spendKeys : m_walletsContainer.get<RandomAccessIndex>())
    {
      m_containerStorage.push_back(encryptKeyPair(spendKeys.spendPublicKey, spendKeys.spendSecretKey, spendKeys.creationTimestamp));
      incNextIv();
    }
    saveWalletCache(m_containerStorage, m_key, WalletSaveLevel::SAVE_ALL, "");
    boost::filesystem::rename(path, bakPath);
    std::error_code ec;
    m_containerStorage.rename(path, ec);
    if (ec)
    {
      m_logger(ERROR, BRIGHT_RED) << "Failed to rename " << tmpPath << " to " << path;

      boost::system::error_code ignore;
      boost::filesystem::rename(bakPath, path, ignore);
      throw std::system_error(ec, "Failed to replace wallet file");
    }

    tmpFileDeleter.cancel();
    m_logger(INFO, BRIGHT_WHITE) << "Wallet file converted! Previous version: " << bakPath;
  }

  void WalletGreen::incNextIv()
  {
    static_assert(sizeof(uint64_t) == sizeof(crypto::chacha8_iv), "Bad crypto::chacha8_iv size");
    auto *prefix = reinterpret_cast<ContainerStoragePrefix *>(m_containerStorage.prefix());
    incIv(prefix->nextIv);
  }

  void WalletGreen::loadAndDecryptContainerData(ContainerStorage &storage, const crypto::chacha8_key &key, BinaryArray &containerData)
  {
    common::MemoryInputStream suffixStream(storage.suffix(), storage.suffixSize());
    BinaryInputStreamSerializer suffixSerializer(suffixStream);
    crypto::chacha8_iv suffixIv;
    BinaryArray encryptedContainer;
    suffixSerializer(suffixIv, "suffixIv");
    suffixSerializer(encryptedContainer, "encryptedContainer");

    containerData.resize(encryptedContainer.size());
    chacha8(encryptedContainer.data(), encryptedContainer.size(), key, suffixIv, reinterpret_cast<char *>(containerData.data()));
  }

  void WalletGreen::loadWalletCache(std::unordered_set<crypto::PublicKey> &addedKeys, std::unordered_set<crypto::PublicKey> &deletedKeys, std::string &extra)
  {
    assert(m_containerStorage.isOpened());

    BinaryArray contanerData;
    loadAndDecryptContainerData(m_containerStorage, m_key, contanerData);

    WalletSerializerV2 s(
        *this,
        m_viewPublicKey,
        m_viewSecretKey,
        m_actualBalance,
        m_pendingBalance,
        m_lockedDepositBalance,
        m_unlockedDepositBalance,
        m_walletsContainer,
        m_synchronizer,
        m_unlockTransactionsJob,
        m_transactions,
        m_transfers,
        m_deposits,
        m_uncommitedTransactions,
        extra,
        m_transactionSoftLockTime);

    common::MemoryInputStream containerStream(contanerData.data(), contanerData.size());
    s.load(containerStream, reinterpret_cast<const ContainerStoragePrefix *>(m_containerStorage.prefix())->version);
    addedKeys = std::move(s.addedKeys());
    deletedKeys = std::move(s.deletedKeys());

    m_logger(INFO) << "Container cache loaded";
  }

  void WalletGreen::loadContainerStorage(const std::string &path)
  {
    try
    {
      m_containerStorage.open(path, FileMappedVectorOpenMode::OPEN, sizeof(ContainerStoragePrefix));

      const ContainerStoragePrefix *prefix = reinterpret_cast<ContainerStoragePrefix *>(m_containerStorage.prefix());
      assert(prefix->version >= WalletSerializerV2::MIN_VERSION);

      uint64_t creationTimestamp;
      decryptKeyPair(prefix->encryptedViewKeys, m_viewPublicKey, m_viewSecretKey, creationTimestamp);
      throwIfKeysMissmatch(m_viewSecretKey, m_viewPublicKey, "Restored view public key doesn't correspond to secret key");
      m_logger = logging::LoggerRef(m_logger.getLogger(), "WalletGreen/" + podToHex(m_viewPublicKey).substr(0, 5));

      loadSpendKeys();

      m_logger(DEBUGGING) << "Container keys were successfully loaded";
    }
    catch (const std::exception &e)
    {
      m_logger(ERROR, BRIGHT_RED) << "Failed to load container keys: " << e.what();

      m_walletsContainer.clear();
      m_containerStorage.close();

      throw;
    }
  }

  void WalletGreen::encryptAndSaveContainerData(ContainerStorage &storage, const crypto::chacha8_key &key, const void *containerData, size_t containerDataSize)
  {
    ContainerStoragePrefix *prefix = reinterpret_cast<ContainerStoragePrefix *>(storage.prefix());

    crypto::chacha8_iv suffixIv = prefix->nextIv;
    incIv(prefix->nextIv);

    BinaryArray encryptedContainer;
    encryptedContainer.resize(containerDataSize);
    chacha8(containerData, containerDataSize, key, suffixIv, reinterpret_cast<char *>(encryptedContainer.data()));

    std::string suffix;
    common::StringOutputStream suffixStream(suffix);
    BinaryOutputStreamSerializer suffixSerializer(suffixStream);
    suffixSerializer(suffixIv, "suffixIv");
    suffixSerializer(encryptedContainer, "encryptedContainer");

    storage.resizeSuffix(suffix.size());
    std::copy(suffix.begin(), suffix.end(), storage.suffix());
  }

  void WalletGreen::incIv(crypto::chacha8_iv &iv)
  {
    static_assert(sizeof(uint64_t) == sizeof(crypto::chacha8_iv), "Bad crypto::chacha8_iv size");
    uint64_t *i = reinterpret_cast<uint64_t *>(&iv);
    if (*i < std::numeric_limits<uint64_t>::max())
    {
      ++(*i);
    }
    else
    {
      *i = 0;
    }
  }

  void WalletGreen::load(const std::string &path, const std::string &password, std::string &extra)
  {
    m_logger(INFO, BRIGHT_WHITE) << "Loading container...";

    if (m_state != WalletState::NOT_INITIALIZED)
    {
      m_logger(ERROR, BRIGHT_RED) << "Failed to load: already initialized.";
      throw std::system_error(make_error_code(error::WRONG_STATE));
    }

    throwIfStopped();

    stopBlockchainSynchronizer();

    crypto::cn_context cnContext;
    generate_chacha8_key(cnContext, password, m_key);

    std::ifstream walletFileStream(path, std::ios_base::binary);
    int version = walletFileStream.peek();
    if (version == EOF)
    {
      m_logger(ERROR, BRIGHT_RED) << "Failed to read wallet version";
      throw std::system_error(make_error_code(error::WRONG_VERSION), "Failed to read wallet version");
    }

    if (version < WalletSerializerV2::MIN_VERSION)
    {
      convertAndLoadWalletFile(path, std::move(walletFileStream));
    }
    else
    {
      walletFileStream.close();

      if (version > WalletSerializerV2::SERIALIZATION_VERSION)
      {
        m_logger(ERROR, BRIGHT_RED) << "Unsupported wallet version: " << version;
        throw std::system_error(make_error_code(error::WRONG_VERSION), "Unsupported wallet version");
      }

      loadContainerStorage(path);
      subscribeWallets();

      if (m_containerStorage.suffixSize() > 0)
      {
        try
        {
          std::unordered_set<crypto::PublicKey> addedSpendKeys;
          std::unordered_set<crypto::PublicKey> deletedSpendKeys;
          loadWalletCache(addedSpendKeys, deletedSpendKeys, extra);

          if (!addedSpendKeys.empty())
          {
            m_logger(WARNING, BRIGHT_YELLOW) << "Found addresses not saved in container cache. Resynchronize container";
            clearCaches(false, true);
            subscribeWallets();
          }

          if (!deletedSpendKeys.empty())
          {
            m_logger(WARNING, BRIGHT_YELLOW) << "Found deleted addresses saved in container cache. Remove its transactions";
            deleteOrphanTransactions(deletedSpendKeys);
          }

          if (!addedSpendKeys.empty() || !deletedSpendKeys.empty())
          {
            saveWalletCache(m_containerStorage, m_key, WalletSaveLevel::SAVE_ALL, extra);
          }
        }
        catch (const std::exception &e)
        {
          m_logger(ERROR, BRIGHT_RED) << "Failed to load cache: " << e.what() << ", reset wallet data";
          clearCaches(true, true);
          subscribeWallets();
        }
      }
    }

    // Read all output keys cache
    try
    {
      std::vector<AccountPublicAddress> subscriptionList;
      m_synchronizer.getSubscriptions(subscriptionList);
      for (const auto &addr : subscriptionList)
      {
        auto sub = m_synchronizer.getSubscription(addr);
        if (sub != nullptr)
        {
          std::vector<TransactionOutputInformation> allTransfers;
          const ITransfersContainer *container = &sub->getContainer();
          container->getOutputs(allTransfers, ITransfersContainer::IncludeAll);
          m_logger(INFO, BRIGHT_WHITE) << "Known Transfers " << allTransfers.size();
          for (const auto &o : allTransfers)
          {
            if (o.type != transaction_types::OutputType::Invalid)
            {
              m_synchronizer.addPublicKeysSeen(addr, o.transactionHash, o.outputKey);
            }
          }
        }
      }
    }
    catch (const std::exception &e)
    {
      m_logger(ERROR, BRIGHT_RED) << "Failed to read output keys!! Continue without output keys: " << e.what();
    }

    m_blockchainSynchronizer.addObserver(this);

    initTransactionPool();

    assert(m_blockchain.empty());
    if (m_walletsContainer.get<RandomAccessIndex>().size() != 0)
    {
      m_synchronizer.subscribeConsumerNotifications(m_viewPublicKey, this);
      initBlockchain(m_viewPublicKey);

      startBlockchainSynchronizer();
    }
    else
    {
      m_blockchain.push_back(m_currency.genesisBlockHash());
      m_logger(DEBUGGING) << "Add genesis block hash to blockchain";
    }

    m_password = password;
    m_path = path;
    m_extra = extra;

    m_state = WalletState::INITIALIZED;
    m_logger(INFO, BRIGHT_WHITE) << "Container loaded, view public key " << common::podToHex(m_viewPublicKey) << ", wallet count " << m_walletsContainer.size() << ", actual balance " << m_currency.formatAmount(m_actualBalance) << ", pending balance " << m_currency.formatAmount(m_pendingBalance);
  }

  void WalletGreen::clearCaches(bool clearTransactions, bool clearCachedData)
  {
    if (clearTransactions)
    {
      m_transactions.clear();
      m_transfers.clear();
      m_deposits.clear();
    }

    if (clearCachedData)
    {
      size_t walletIndex = 0;
      for (auto it = m_walletsContainer.begin(); it != m_walletsContainer.end(); ++it)
      {
        m_walletsContainer.modify(it, [&walletIndex](WalletRecord &wallet) {
          wallet.actualBalance = 0;
          wallet.pendingBalance = 0;
          wallet.lockedDepositBalance = 0;
          wallet.unlockedDepositBalance = 0;
          wallet.container = reinterpret_cast<cn::ITransfersContainer *>(walletIndex++); //dirty hack. container field must be unique
        });
      }

      if (!clearTransactions)
      {
        for (auto it = m_transactions.begin(); it != m_transactions.end(); ++it)
        {
          m_transactions.modify(it, [](WalletTransaction &tx) {
            tx.state = WalletTransactionState::CANCELLED;
            tx.blockHeight = WALLET_UNCONFIRMED_TRANSACTION_HEIGHT;
          });
        }
      }

      std::vector<AccountPublicAddress> subscriptions;
      m_synchronizer.getSubscriptions(subscriptions);
      std::for_each(subscriptions.begin(), subscriptions.end(), [this](const AccountPublicAddress &address) { m_synchronizer.removeSubscription(address); });

      m_uncommitedTransactions.clear();
      m_unlockTransactionsJob.clear();
      m_actualBalance = 0;
      m_pendingBalance = 0;
      m_lockedDepositBalance = 0;
      m_unlockedDepositBalance = 0;
      m_fusionTxsCache.clear();
      m_blockchain.clear();
    }
  }

  void WalletGreen::subscribeWallets()
  {
    try
    {
      auto &index = m_walletsContainer.get<RandomAccessIndex>();

      for (auto it = index.begin(); it != index.end(); ++it)
      {
        const auto &wallet = *it;

        AccountSubscription sub;
        sub.keys.address.viewPublicKey = m_viewPublicKey;
        sub.keys.address.spendPublicKey = wallet.spendPublicKey;
        sub.keys.viewSecretKey = m_viewSecretKey;
        sub.keys.spendSecretKey = wallet.spendSecretKey;
        sub.transactionSpendableAge = m_transactionSoftLockTime;
        sub.syncStart.height = 0;
        sub.syncStart.timestamp = std::max(static_cast<uint64_t>(wallet.creationTimestamp), ACCOUNT_CREATE_TIME_ACCURACY) - ACCOUNT_CREATE_TIME_ACCURACY;

        auto &subscription = m_synchronizer.addSubscription(sub);
        bool r = index.modify(it, [&subscription](WalletRecord &rec) { rec.container = &subscription.getContainer(); });
        (void)r;
        assert(r);
        subscription.addObserver(this);
      }
    }
    catch (const std::exception &e)
    {
      m_logger(ERROR, BRIGHT_RED) << "Failed to subscribe wallets: " << e.what();

      std::vector<AccountPublicAddress> subscriptionList;
      m_synchronizer.getSubscriptions(subscriptionList);
      for (const auto &subscription : subscriptionList)
      {
        m_synchronizer.removeSubscription(subscription);
      }

      throw;
    }
  }

  void WalletGreen::load(const std::string &path, const std::string &password)
  {
    std::string extra;
    load(path, password, extra);
  }

  void WalletGreen::changePassword(const std::string &oldPassword, const std::string &newPassword)
  {
    throwIfNotInitialized();
    throwIfStopped();

    if (m_password.compare(oldPassword))
    {
      throw std::system_error(make_error_code(error::WRONG_PASSWORD));
    }

    m_password = newPassword;
  }

  size_t WalletGreen::getAddressCount() const
  {
    throwIfNotInitialized();
    throwIfStopped();

    return m_walletsContainer.get<RandomAccessIndex>().size();
  }

  size_t WalletGreen::getWalletDepositCount() const
  {
    throwIfNotInitialized();
    throwIfStopped();

    return m_deposits.get<RandomAccessIndex>().size();
  }

  std::string WalletGreen::getAddress(size_t index) const
  {
    throwIfNotInitialized();
    throwIfStopped();

    if (index >= m_walletsContainer.get<RandomAccessIndex>().size())
    {
      throw std::system_error(make_error_code(std::errc::invalid_argument));
    }

    const WalletRecord &wallet = m_walletsContainer.get<RandomAccessIndex>()[index];
    return m_currency.accountAddressAsString({wallet.spendPublicKey, m_viewPublicKey});
  }

  KeyPair WalletGreen::getAddressSpendKey(size_t index) const
  {
    throwIfNotInitialized();
    throwIfStopped();

    if (index >= m_walletsContainer.get<RandomAccessIndex>().size())
    {
      throw std::system_error(make_error_code(std::errc::invalid_argument));
    }

    const WalletRecord &wallet = m_walletsContainer.get<RandomAccessIndex>()[index];
    return {wallet.spendPublicKey, wallet.spendSecretKey};
  }

  KeyPair WalletGreen::getAddressSpendKey(const std::string &address) const
  {
    throwIfNotInitialized();
    throwIfStopped();

    cn::AccountPublicAddress pubAddr = parseAddress(address);

    auto it = m_walletsContainer.get<KeysIndex>().find(pubAddr.spendPublicKey);
    if (it == m_walletsContainer.get<KeysIndex>().end())
    {
      throw std::system_error(make_error_code(error::OBJECT_NOT_FOUND));
    }

    return {it->spendPublicKey, it->spendSecretKey};
  }

  KeyPair WalletGreen::getViewKey() const
  {
    throwIfNotInitialized();
    throwIfStopped();

    return {m_viewPublicKey, m_viewSecretKey};
  }

  std::string WalletGreen::createAddress()
  {
    KeyPair spendKey;
    crypto::generate_keys(spendKey.publicKey, spendKey.secretKey);
    auto creationTimestamp = static_cast<uint64_t>(time(nullptr));

    return doCreateAddress(spendKey.publicKey, spendKey.secretKey, creationTimestamp);
  }

  std::string WalletGreen::createAddress(const crypto::SecretKey &spendSecretKey)
  {
    crypto::PublicKey spendPublicKey;
    if (!crypto::secret_key_to_public_key(spendSecretKey, spendPublicKey))
    {
      throw std::system_error(make_error_code(cn::error::KEY_GENERATION_ERROR));
    }
    auto creationTimestamp = static_cast<uint64_t>(time(nullptr));
    return doCreateAddress(spendPublicKey, spendSecretKey, creationTimestamp);
  }

  std::string WalletGreen::createAddress(const crypto::PublicKey &spendPublicKey)
  {
    if (!crypto::check_key(spendPublicKey))
    {
      throw std::system_error(make_error_code(error::WRONG_PARAMETERS), "Wrong public key format");
    }
    auto creationTimestamp = static_cast<uint64_t>(time(nullptr));
    return doCreateAddress(spendPublicKey, NULL_SECRET_KEY, creationTimestamp);
  }

  std::vector<std::string> WalletGreen::createAddressList(const std::vector<crypto::SecretKey> &spendSecretKeys, bool reset)
  {
    std::vector<NewAddressData> addressDataList(spendSecretKeys.size());
    for (size_t i = 0; i < spendSecretKeys.size(); ++i)
    {
      crypto::PublicKey spendPublicKey;
      if (!crypto::secret_key_to_public_key(spendSecretKeys[i], spendPublicKey))
      {
        m_logger(ERROR) << "createAddressList(): failed to convert secret key to public key";
        throw std::system_error(make_error_code(cn::error::KEY_GENERATION_ERROR));
      }

      addressDataList[i].spendSecretKey = spendSecretKeys[i];
      addressDataList[i].spendPublicKey = spendPublicKey;
      addressDataList[i].creationTimestamp = reset ? 0 : static_cast<uint64_t>(time(nullptr));
    }

    return doCreateAddressList(addressDataList);
  }

  std::vector<std::string> WalletGreen::doCreateAddressList(const std::vector<NewAddressData> &addressDataList)
  {
    throwIfNotInitialized();
    throwIfStopped();

    stopBlockchainSynchronizer();

    std::vector<std::string> addresses;
    try
    {
      uint64_t minCreationTimestamp = std::numeric_limits<uint64_t>::max();

      {
        if (addressDataList.size() > 1)
        {
          m_containerStorage.setAutoFlush(false);
        }

        tools::ScopeExit exitHandler([this] {
          if (!m_containerStorage.getAutoFlush())
          {
            m_containerStorage.setAutoFlush(true);
            m_containerStorage.flush();
          }
        });

        for (auto &addressData : addressDataList)
        {
          assert(addressData.creationTimestamp <= std::numeric_limits<uint64_t>::max() - m_currency.blockFutureTimeLimit());
          std::string address = addWallet(addressData.spendPublicKey, addressData.spendSecretKey, addressData.creationTimestamp);
          m_logger(INFO, BRIGHT_WHITE) << "New wallet added " << address << ", creation timestamp " << addressData.creationTimestamp;
          addresses.push_back(std::move(address));

          minCreationTimestamp = std::min(minCreationTimestamp, addressData.creationTimestamp);
        }
      }

      m_containerStorage.setAutoFlush(true);
      auto currentTime = static_cast<uint64_t>(time(nullptr));
      if (minCreationTimestamp + m_currency.blockFutureTimeLimit() < currentTime)
      {
        m_logger(DEBUGGING) << "Reset is required";
        save(WalletSaveLevel::SAVE_KEYS_AND_TRANSACTIONS, m_extra);
        shutdown();
        load(m_path, m_password);
      }
    }
    catch (const std::exception &e)
    {
      m_logger(ERROR, BRIGHT_RED) << "Failed to add wallets: " << e.what();
      startBlockchainSynchronizer();
      throw;
    }

    startBlockchainSynchronizer();

    return addresses;
  }

  std::string WalletGreen::doCreateAddress(const crypto::PublicKey &spendPublicKey, const crypto::SecretKey &spendSecretKey, uint64_t creationTimestamp)
  {
    assert(creationTimestamp <= std::numeric_limits<uint64_t>::max() - m_currency.blockFutureTimeLimit());

    std::vector<NewAddressData> addressDataList;
    addressDataList.push_back(NewAddressData{spendPublicKey, spendSecretKey, creationTimestamp});
    std::vector<std::string> addresses = doCreateAddressList(addressDataList);
    assert(addresses.size() == 1);

    return addresses.front();
  }

  std::string WalletGreen::addWallet(const crypto::PublicKey &spendPublicKey, const crypto::SecretKey &spendSecretKey, uint64_t creationTimestamp)
  {
    auto &index = m_walletsContainer.get<KeysIndex>();

    auto trackingMode = getTrackingMode();

    if ((trackingMode == WalletTrackingMode::TRACKING && spendSecretKey != NULL_SECRET_KEY) ||
        (trackingMode == WalletTrackingMode::NOT_TRACKING && spendSecretKey == NULL_SECRET_KEY))
    {

      throw std::system_error(make_error_code(error::WRONG_PARAMETERS));
    }

    auto insertIt = index.find(spendPublicKey);
    if (insertIt != index.end())
    {
      m_logger(ERROR, BRIGHT_RED) << "Failed to add wallet: address already exists, " << m_currency.accountAddressAsString(AccountPublicAddress{spendPublicKey, m_viewPublicKey});
      throw std::system_error(make_error_code(error::ADDRESS_ALREADY_EXISTS));
    }

    m_containerStorage.push_back(encryptKeyPair(spendPublicKey, spendSecretKey, creationTimestamp));
    incNextIv();

    try
    {
      AccountSubscription sub;
      sub.keys.address.viewPublicKey = m_viewPublicKey;
      sub.keys.address.spendPublicKey = spendPublicKey;
      sub.keys.viewSecretKey = m_viewSecretKey;
      sub.keys.spendSecretKey = spendSecretKey;
      sub.transactionSpendableAge = m_transactionSoftLockTime;
      sub.syncStart.height = 0;
      sub.syncStart.timestamp = std::max(creationTimestamp, ACCOUNT_CREATE_TIME_ACCURACY) - ACCOUNT_CREATE_TIME_ACCURACY;

      auto &trSubscription = m_synchronizer.addSubscription(sub);
      ITransfersContainer *container = &trSubscription.getContainer();

      WalletRecord wallet;
      wallet.spendPublicKey = spendPublicKey;
      wallet.spendSecretKey = spendSecretKey;
      wallet.container = container;
      wallet.creationTimestamp = static_cast<time_t>(creationTimestamp);
      trSubscription.addObserver(this);

      index.insert(insertIt, std::move(wallet));
      m_logger(DEBUGGING) << "Wallet count " << m_walletsContainer.size();

      if (index.size() == 1)
      {
        m_synchronizer.subscribeConsumerNotifications(m_viewPublicKey, this);
        initBlockchain(m_viewPublicKey);
      }

      auto address = m_currency.accountAddressAsString({spendPublicKey, m_viewPublicKey});
      m_logger(DEBUGGING) << "Wallet added " << address << ", creation timestamp " << creationTimestamp;
      return address;
    }
    catch (const std::exception &e)
    {
      m_logger(ERROR) << "Failed to add wallet: " << e.what();

      try
      {
        m_containerStorage.pop_back();
      }
      catch (...)
      {
        m_logger(ERROR) << "Failed to rollback adding wallet to storage";
      }

      throw;
    }
  }

  void WalletGreen::deleteAddress(const std::string &address)
  {
    throwIfNotInitialized();
    throwIfStopped();

    cn::AccountPublicAddress pubAddr = parseAddress(address);

    auto it = m_walletsContainer.get<KeysIndex>().find(pubAddr.spendPublicKey);
    if (it == m_walletsContainer.get<KeysIndex>().end())
    {
      throw std::system_error(make_error_code(error::OBJECT_NOT_FOUND));
    }

    stopBlockchainSynchronizer();

    m_actualBalance -= it->actualBalance;
    m_pendingBalance -= it->pendingBalance;

    m_synchronizer.removeSubscription(pubAddr);

    deleteContainerFromUnlockTransactionJobs(it->container);
    std::vector<size_t> deletedTransactions;
    std::vector<size_t> updatedTransactions = deleteTransfersForAddress(address, deletedTransactions);
    deleteFromUncommitedTransactions(deletedTransactions);

    m_walletsContainer.get<KeysIndex>().erase(it);

    auto addressIndex = std::distance(
        m_walletsContainer.get<RandomAccessIndex>().begin(), m_walletsContainer.project<RandomAccessIndex>(it));

    m_containerStorage.erase(std::next(m_containerStorage.begin(), addressIndex));

    if (m_walletsContainer.get<RandomAccessIndex>().size() != 0)
    {
      startBlockchainSynchronizer();
    }
    else
    {
      m_blockchain.clear();
      m_blockchain.push_back(m_currency.genesisBlockHash());
    }

    for (auto transactionId : updatedTransactions)
    {
      pushEvent(makeTransactionUpdatedEvent(transactionId));
    }
  }

  uint64_t WalletGreen::getActualBalance() const
  {
    throwIfNotInitialized();
    throwIfStopped();

    return m_actualBalance;
  }

  uint64_t WalletGreen::getActualBalance(const std::string &address) const
  {
    throwIfNotInitialized();
    throwIfStopped();

    const auto &wallet = getWalletRecord(address);
    return wallet.actualBalance;
  }

  uint64_t WalletGreen::getPendingBalance() const
  {
    throwIfNotInitialized();
    throwIfStopped();

    return m_pendingBalance;
  }

  uint64_t WalletGreen::getLockedDepositBalance(const std::string &address) const
  {
    throwIfNotInitialized();
    throwIfStopped();

    const auto &wallet = getWalletRecord(address);
    return wallet.lockedDepositBalance;
  }

  uint64_t WalletGreen::getUnlockedDepositBalance(const std::string &address) const
  {
    throwIfNotInitialized();
    throwIfStopped();

    const auto &wallet = getWalletRecord(address);
    return wallet.unlockedDepositBalance;
  }

  uint64_t WalletGreen::getLockedDepositBalance() const
  {
    throwIfNotInitialized();
    throwIfStopped();

    return m_lockedDepositBalance;
  }

  uint64_t WalletGreen::getUnlockedDepositBalance() const
  {
    throwIfNotInitialized();
    throwIfStopped();

    return m_unlockedDepositBalance;
  }

  uint64_t WalletGreen::getPendingBalance(const std::string &address) const
  {
    throwIfNotInitialized();
    throwIfStopped();

    const auto &wallet = getWalletRecord(address);
    return wallet.pendingBalance;
  }

  uint64_t WalletGreen::getDustBalance() const
  {
    throwIfNotInitialized();
    throwIfStopped();

    const auto &walletsIndex = m_walletsContainer.get<RandomAccessIndex>();
    uint64_t money = 0;
    for (const auto &wallet : walletsIndex)
    {
      const ITransfersContainer *container = wallet.container;
      std::vector<TransactionOutputInformation> outputs;
      container->getOutputs(outputs, ITransfersContainer::IncludeKeyUnlocked);
      for (const auto &output : outputs)
      {
        if (output.amount < m_currency.defaultDustThreshold())
        {
          money += output.amount;
        }
      }
    }
    return money;
  }

  uint64_t WalletGreen::getDustBalance(const std::string &address) const
  {
    throwIfNotInitialized();
    throwIfStopped();

    const auto &wallet = getWalletRecord(address);
    uint64_t money = 0;
    const ITransfersContainer *container = wallet.container;
    std::vector<TransactionOutputInformation> outputs;
    container->getOutputs(outputs, ITransfersContainer::IncludeKeyUnlocked);
    for (const auto &output : outputs)
    {
      if (output.amount < m_currency.defaultDustThreshold())
      {
        money += output.amount;
      }
    }
    return money;
  }

  size_t WalletGreen::getTransactionCount() const
  {
    throwIfNotInitialized();
    throwIfStopped();

    return m_transactions.get<RandomAccessIndex>().size();
  }

  WalletTransaction WalletGreen::getTransaction(size_t transactionIndex) const
  {
    throwIfNotInitialized();
    throwIfStopped();

    if (m_transactions.size() <= transactionIndex)
    {
      throw std::system_error(make_error_code(cn::error::INDEX_OUT_OF_RANGE));
    }

    return m_transactions.get<RandomAccessIndex>()[transactionIndex];
  }

  Deposit WalletGreen::getDeposit(size_t depositIndex) const
  {
    throwIfNotInitialized();
    throwIfStopped();

    if (m_deposits.size() <= depositIndex)
    {
      throw std::system_error(make_error_code(cn::error::DEPOSIT_DOESNOT_EXIST));
    }

    Deposit deposit = m_deposits.get<RandomAccessIndex>()[depositIndex];

    uint32_t knownBlockHeight = m_node.getLastKnownBlockHeight();
    if (knownBlockHeight > deposit.unlockHeight)
    {
      deposit.locked = false;
    }

    return deposit;
  }

  size_t WalletGreen::getTransactionTransferCount(size_t transactionIndex) const
  {
    throwIfNotInitialized();
    throwIfStopped();

    auto bounds = getTransactionTransfersRange(transactionIndex);
    return static_cast<size_t>(std::distance(bounds.first, bounds.second));
  }

  WalletTransfer WalletGreen::getTransactionTransfer(size_t transactionIndex, size_t transferIndex) const
  {
    throwIfNotInitialized();
    throwIfStopped();

    auto bounds = getTransactionTransfersRange(transactionIndex);

    if (transferIndex >= static_cast<size_t>(std::distance(bounds.first, bounds.second)))
    {
      throw std::system_error(make_error_code(std::errc::invalid_argument));
    }

    return std::next(bounds.first, transferIndex)->second;
  }

  WalletGreen::TransfersRange WalletGreen::getTransactionTransfersRange(size_t transactionIndex) const
  {
    auto val = std::make_pair(transactionIndex, WalletTransfer());

    auto bounds = std::equal_range(m_transfers.begin(), m_transfers.end(), val, [](const TransactionTransferPair &a, const TransactionTransferPair &b) {
      return a.first < b.first;
    });

    return bounds;
  }

  size_t WalletGreen::transfer(const TransactionParameters &transactionParameters, crypto::SecretKey &transactionSK)
  {
    tools::ScopeExit releaseContext([this] {
      m_dispatcher.yield();
    });

    platform_system::EventLock lk(m_readyEvent);

    throwIfNotInitialized();
    throwIfTrackingMode();
    throwIfStopped();

    return doTransfer(transactionParameters, transactionSK);
  }

  void WalletGreen::prepareTransaction(
      std::vector<WalletOuts> &&wallets,
      const std::vector<WalletOrder> &orders,
      const std::vector<WalletMessage> &messages,
      uint64_t fee,
      uint64_t mixIn,
      const std::string &extra,
      uint64_t unlockTimestamp,
      const DonationSettings &donation,
      const cn::AccountPublicAddress &changeDestination,
      PreparedTransaction &preparedTransaction,
      crypto::SecretKey &transactionSK)
  {

    preparedTransaction.destinations = convertOrdersToTransfers(orders);
    preparedTransaction.neededMoney = countNeededMoney(preparedTransaction.destinations, fee);

    std::vector<OutputToTransfer> selectedTransfers;
    uint64_t foundMoney = selectTransfers(preparedTransaction.neededMoney, m_currency.defaultDustThreshold(), wallets, selectedTransfers);

    if (foundMoney < preparedTransaction.neededMoney)
    {
      throw std::system_error(make_error_code(error::WRONG_AMOUNT), "Not enough money");
    }

    std::vector<outs_for_amount> mixinResult;

    if (mixIn != 0)
    {
      requestMixinOuts(selectedTransfers, mixIn, mixinResult);
    }

    std::vector<InputInfo> keysInfo;
    prepareInputs(selectedTransfers, mixinResult, mixIn, keysInfo);

    uint64_t donationAmount = pushDonationTransferIfPossible(donation, foundMoney - preparedTransaction.neededMoney, m_currency.defaultDustThreshold(), preparedTransaction.destinations);
    preparedTransaction.changeAmount = foundMoney - preparedTransaction.neededMoney - donationAmount;

    std::vector<ReceiverAmounts> decomposedOutputs = splitDestinations(preparedTransaction.destinations, m_currency.defaultDustThreshold(), m_currency);
    if (preparedTransaction.changeAmount != 0)
    {
      WalletTransfer changeTransfer;
      changeTransfer.type = WalletTransferType::CHANGE;
      changeTransfer.address = m_currency.accountAddressAsString(changeDestination);
      changeTransfer.amount = static_cast<int64_t>(preparedTransaction.changeAmount);
      preparedTransaction.destinations.emplace_back(std::move(changeTransfer));

      auto splittedChange = splitAmount(preparedTransaction.changeAmount, changeDestination, m_currency.defaultDustThreshold());
      decomposedOutputs.emplace_back(std::move(splittedChange));
    }

    preparedTransaction.transaction = makeTransaction(decomposedOutputs, keysInfo, messages, extra, unlockTimestamp, transactionSK);
  }

  void WalletGreen::validateTransactionParameters(const TransactionParameters &transactionParameters) const
  {
    if (transactionParameters.destinations.empty())
    {
      throw std::system_error(make_error_code(error::ZERO_DESTINATION));
    }

    if (transactionParameters.donation.address.empty() != (transactionParameters.donation.threshold == 0))
    {
      throw std::system_error(make_error_code(error::WRONG_PARAMETERS));
    }

    validateSourceAddresses(transactionParameters.sourceAddresses);
    validateChangeDestination(transactionParameters.sourceAddresses, transactionParameters.changeDestination, false);
    validateOrders(transactionParameters.destinations);
  }

  size_t WalletGreen::doTransfer(const TransactionParameters &transactionParameters, crypto::SecretKey &transactionSK)
  {
    validateTransactionParameters(transactionParameters);
    cn::AccountPublicAddress changeDestination = getChangeDestination(transactionParameters.changeDestination, transactionParameters.sourceAddresses);

    std::vector<WalletOuts> wallets;
    if (!transactionParameters.sourceAddresses.empty())
    {
      wallets = pickWallets(transactionParameters.sourceAddresses);
    }
    else
    {
      wallets = pickWalletsWithMoney();
    }

    PreparedTransaction preparedTransaction;
    prepareTransaction(
        std::move(wallets),
        transactionParameters.destinations,
        transactionParameters.messages,
        transactionParameters.fee,
        transactionParameters.mixIn,
        transactionParameters.extra,
        transactionParameters.unlockTimestamp,
        transactionParameters.donation,
        changeDestination,
        preparedTransaction,
        transactionSK);

    return validateSaveAndSendTransaction(*preparedTransaction.transaction, preparedTransaction.destinations, false, true);
  }

  size_t WalletGreen::makeTransaction(const TransactionParameters &sendingTransaction)
  {
    size_t id = WALLET_INVALID_TRANSACTION_ID;
    tools::ScopeExit releaseContext([this] {
      m_dispatcher.yield();
    });

    platform_system::EventLock lk(m_readyEvent);

    throwIfNotInitialized();
    throwIfTrackingMode();
    throwIfStopped();

    validateTransactionParameters(sendingTransaction);
    cn::AccountPublicAddress changeDestination = getChangeDestination(sendingTransaction.changeDestination, sendingTransaction.sourceAddresses);
    m_logger(DEBUGGING) << "Change address " << m_currency.accountAddressAsString(changeDestination);

    std::vector<WalletOuts> wallets;
    if (!sendingTransaction.sourceAddresses.empty())
    {
      wallets = pickWallets(sendingTransaction.sourceAddresses);
    }
    else
    {
      wallets = pickWalletsWithMoney();
    }

    PreparedTransaction preparedTransaction;
    crypto::SecretKey txSecretKey;
    prepareTransaction(
        std::move(wallets),
        sendingTransaction.destinations,
        sendingTransaction.messages,
        sendingTransaction.fee,
        sendingTransaction.mixIn,
        sendingTransaction.extra,
        sendingTransaction.unlockTimestamp,
        sendingTransaction.donation,
        changeDestination,
        preparedTransaction,
        txSecretKey);

    id = validateSaveAndSendTransaction(*preparedTransaction.transaction, preparedTransaction.destinations, false, false);
    return id;
  }

  void WalletGreen::commitTransaction(size_t transactionId)
  {
    platform_system::EventLock lk(m_readyEvent);

    throwIfNotInitialized();
    throwIfStopped();
    throwIfTrackingMode();

    if (transactionId >= m_transactions.size())
    {
      m_logger(ERROR, BRIGHT_RED) << "Failed to commit transaction: invalid index " << transactionId << ". Number of transactions: " << m_transactions.size();
      throw std::system_error(make_error_code(cn::error::INDEX_OUT_OF_RANGE));
    }

    auto txIt = std::next(m_transactions.get<RandomAccessIndex>().begin(), transactionId);
    if (m_uncommitedTransactions.count(transactionId) == 0 || txIt->state != WalletTransactionState::CREATED)
    {
      throw std::system_error(make_error_code(error::TX_TRANSFER_IMPOSSIBLE));
    }

    platform_system::Event completion(m_dispatcher);
    std::error_code ec;

    m_node.relayTransaction(m_uncommitedTransactions[transactionId], [&ec, &completion, this](std::error_code error) {
      ec = error;
      this->m_dispatcher.remoteSpawn(std::bind(asyncRequestCompletion, std::ref(completion)));
    });
    completion.wait();

    if (!ec)
    {
      updateTransactionStateAndPushEvent(transactionId, WalletTransactionState::SUCCEEDED);
      m_uncommitedTransactions.erase(transactionId);
    }
    else
    {
      throw std::system_error(ec);
    }
  }

  void WalletGreen::rollbackUncommitedTransaction(size_t transactionId)
  {
    tools::ScopeExit releaseContext([this] {
      m_dispatcher.yield();
    });

    platform_system::EventLock lk(m_readyEvent);

    throwIfNotInitialized();
    throwIfStopped();
    throwIfTrackingMode();

    if (transactionId >= m_transactions.size())
    {
      throw std::system_error(make_error_code(cn::error::INDEX_OUT_OF_RANGE));
    }

    auto txIt = m_transactions.get<RandomAccessIndex>().begin();
    std::advance(txIt, transactionId);
    if (m_uncommitedTransactions.count(transactionId) == 0 || txIt->state != WalletTransactionState::CREATED)
    {
      throw std::system_error(make_error_code(error::TX_CANCEL_IMPOSSIBLE));
    }

    removeUnconfirmedTransaction(getObjectHash(m_uncommitedTransactions[transactionId]));
    m_uncommitedTransactions.erase(transactionId);
  }

  void WalletGreen::pushBackOutgoingTransfers(size_t txId, const std::vector<WalletTransfer> &destinations)
  {

    for (const auto &dest : destinations)
    {
      WalletTransfer d;
      d.type = dest.type;
      d.address = dest.address;
      d.amount = dest.amount;

      m_transfers.emplace_back(txId, std::move(d));
    }
  }

  size_t WalletGreen::insertOutgoingTransactionAndPushEvent(const Hash &transactionHash, uint64_t fee, const BinaryArray &extra, uint64_t unlockTimestamp)
  {
    WalletTransaction insertTx;
    insertTx.state = WalletTransactionState::CREATED;
    insertTx.creationTime = static_cast<uint64_t>(time(nullptr));
    insertTx.unlockTime = unlockTimestamp;
    insertTx.blockHeight = cn::WALLET_UNCONFIRMED_TRANSACTION_HEIGHT;
    insertTx.extra.assign(reinterpret_cast<const char *>(extra.data()), extra.size());
    insertTx.fee = fee;
    insertTx.hash = transactionHash;
    insertTx.totalAmount = 0; // 0 until transactionHandlingEnd() is called
    insertTx.timestamp = 0;   //0 until included in a block
    insertTx.isBase = false;

    size_t txId = m_transactions.get<RandomAccessIndex>().size();
    m_transactions.get<RandomAccessIndex>().push_back(std::move(insertTx));

    pushEvent(makeTransactionCreatedEvent(txId));

    return txId;
  }

  void WalletGreen::updateTransactionStateAndPushEvent(size_t transactionId, WalletTransactionState state)
  {
    auto it = std::next(m_transactions.get<RandomAccessIndex>().begin(), transactionId);

    if (it->state != state)
    {
      m_transactions.get<RandomAccessIndex>().modify(it, [state](WalletTransaction &tx) {
        tx.state = state;
      });

      pushEvent(makeTransactionUpdatedEvent(transactionId));
    }
  }

  bool WalletGreen::updateWalletDepositInfo(size_t depositId, const cn::Deposit &info)
  {
    auto &txIdIndex = m_deposits.get<RandomAccessIndex>();
    assert(depositId < txIdIndex.size());
    auto it = std::next(txIdIndex.begin(), depositId);

    bool updated = false;
    bool r = txIdIndex.modify(it, [&info, &updated](Deposit &deposit) {
      if (deposit.spendingTransactionId != info.spendingTransactionId)
      {
        deposit.spendingTransactionId = info.spendingTransactionId;
        updated = true;
      }
    });
    (void)r;
    assert(r);

    return updated;
  }

  bool WalletGreen::updateWalletTransactionInfo(size_t transactionId, const cn::TransactionInformation &info, int64_t totalAmount)
  {
    auto &txIdIndex = m_transactions.get<RandomAccessIndex>();
    assert(transactionId < txIdIndex.size());
    auto it = std::next(txIdIndex.begin(), transactionId);

    bool updated = false;
    bool r = txIdIndex.modify(it, [&info, totalAmount, &updated](WalletTransaction &transaction) {
      if (transaction.firstDepositId != info.firstDepositId)
      {
        transaction.firstDepositId = info.firstDepositId;
        updated = true;
        transaction.depositCount = 1;
      }

      if (transaction.blockHeight != info.blockHeight)
      {
        transaction.blockHeight = info.blockHeight;
        updated = true;
      }

      if (transaction.timestamp != info.timestamp)
      {
        transaction.timestamp = info.timestamp;
        updated = true;
      }

      bool isSucceeded = transaction.state == WalletTransactionState::SUCCEEDED;
      // If transaction was sent to daemon, it can not have CREATED and FAILED states, its state can be SUCCEEDED, CANCELLED or DELETED
      bool wasSent = transaction.state != WalletTransactionState::CREATED && transaction.state != WalletTransactionState::FAILED;
      bool isConfirmed = transaction.blockHeight != WALLET_UNCONFIRMED_TRANSACTION_HEIGHT;
      if (!isSucceeded && (wasSent || isConfirmed))
      {
        //transaction may be deleted first then added again
        transaction.state = WalletTransactionState::SUCCEEDED;
        updated = true;
      }

      if (transaction.totalAmount != totalAmount)
      {
        transaction.totalAmount = totalAmount;
        updated = true;
      }

      // Fix LegacyWallet error. Some old versions didn't fill extra field
      if (transaction.extra.empty() && !info.extra.empty())
      {
        transaction.extra = common::asString(info.extra);
        updated = true;
      }

      bool isBase = info.totalAmountIn == 0;
      if (transaction.isBase != isBase)
      {
        transaction.isBase = isBase;
        updated = true;
      }
    });
    (void)r;
    assert(r);

    return updated;
  }

  size_t WalletGreen::insertBlockchainTransaction(const TransactionInformation &info, int64_t txBalance)
  {
    auto &index = m_transactions.get<RandomAccessIndex>();

    WalletTransaction tx;
    tx.state = WalletTransactionState::SUCCEEDED;
    tx.timestamp = info.timestamp;
    tx.blockHeight = info.blockHeight;
    tx.hash = info.transactionHash;
    tx.depositCount = 0;
    tx.firstDepositId = WALLET_INVALID_DEPOSIT_ID;
    tx.isBase = info.totalAmountIn == 0;
    if (tx.isBase)
    {
      tx.fee = 0;
    }
    else
    {
      tx.fee = info.totalAmountIn < info.totalAmountOut ? cn::parameters::MINIMUM_FEE : info.totalAmountIn - info.totalAmountOut;
    }

    tx.unlockTime = info.unlockTime;
    tx.extra.assign(reinterpret_cast<const char *>(info.extra.data()), info.extra.size());
    tx.totalAmount = txBalance;
    tx.creationTime = info.timestamp;

    size_t txId = index.size();
    index.push_back(std::move(tx));

    return txId;
  }

  uint64_t WalletGreen::scanHeightToTimestamp(const uint32_t scanHeight) const
  {
    if (scanHeight == 0)
    {
      return 0;
    }

    /* Get the amount of seconds since the blockchain launched */
    double secondsSinceLaunch = scanHeight * cn::parameters::DIFFICULTY_TARGET;

    /* Add a bit of a buffer in case of difficulty weirdness, blocks coming
	   out too fast */
    secondsSinceLaunch = secondsSinceLaunch * 0.95;

    /* Get the genesis block timestamp and add the time since launch */
    uint64_t timestamp = m_currency.getGenesisTimestamp() + static_cast<uint64_t>(secondsSinceLaunch);
    /* Timestamp in the future */
    if (timestamp >= static_cast<uint64_t>(std::time(nullptr)))
    {
      return getCurrentTimestampAdjusted();
    }

    return timestamp;
  }

  uint64_t WalletGreen::getCurrentTimestampAdjusted() const
  {
    /* Get the current time as a unix timestamp */
    std::time_t time = std::time(nullptr);

    /* Take the amount of time a block can potentially be in the past/future */
    std::initializer_list<uint64_t> limits = {
        cn::parameters::CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT,
        cn::parameters::CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V1};

    /* Get the largest adjustment possible */
    uint64_t adjust = std::max(limits);

    /* Take the earliest timestamp that will include all possible blocks */
    return time - adjust;
  }

  void WalletGreen::reset(const uint64_t scanHeight)
  {
    throwIfNotInitialized();
    throwIfStopped();

    /* Stop so things can't be added to the container as we're looping */
    stop();

    /* Grab the wallet encrypted prefix */
    auto *prefix = reinterpret_cast<ContainerStoragePrefix *>(m_containerStorage.prefix());
    m_logger(INFO, BRIGHT_WHITE) << "reset with height " << scanHeight;
    uint64_t newTimestamp = scanHeightToTimestamp((uint32_t)scanHeight);

    m_logger(INFO, BRIGHT_WHITE) << "new timestamp " << newTimestamp;

    /* Reencrypt with the new creation timestamp so we rescan from here when we relaunch */
    prefix->encryptedViewKeys = encryptKeyPair(m_viewPublicKey, m_viewSecretKey, newTimestamp);

    /* As a reference so we can update it */
    for (auto &encryptedSpendKeys : m_containerStorage)
    {
      crypto::PublicKey publicKey;
      crypto::SecretKey secretKey;
      uint64_t oldTimestamp;

      /* Decrypt the key pair we're pointing to */
      decryptKeyPair(encryptedSpendKeys, publicKey, secretKey, oldTimestamp);

      /* Re-encrypt with the new timestamp */
      encryptedSpendKeys = encryptKeyPair(publicKey, secretKey, newTimestamp);
    }

    /* Start again so we can save */
    start();

    /* Save just the keys + timestamp to file */
    save(cn::WalletSaveLevel::SAVE_KEYS_ONLY);

    /* Stop and shutdown */
    stop();

    /* Shutdown the wallet */
    shutdown();

    start();

    /* Reopen from truncated storage */
    load(m_path, m_password);
  }

  bool WalletGreen::updateTransactionTransfers(size_t transactionId, const std::vector<ContainerAmounts> &containerAmountsList,
                                               int64_t allInputsAmount, int64_t allOutputsAmount)
  {

    assert(allInputsAmount <= 0);
    assert(allOutputsAmount >= 0);

    bool updated = false;

    auto transfersRange = getTransactionTransfersRange(transactionId);
    // Iterators can be invalidated, so the first transfer is addressed by its index
    size_t firstTransferIdx = std::distance(m_transfers.cbegin(), transfersRange.first);

    TransfersMap initialTransfers = getKnownTransfersMap(transactionId, firstTransferIdx);

    std::unordered_set<std::string> myInputAddresses;
    std::unordered_set<std::string> myOutputAddresses;
    int64_t myInputsAmount = 0;
    int64_t myOutputsAmount = 0;
    for (auto containerAmount : containerAmountsList)
    {
      AccountPublicAddress address{getWalletRecord(containerAmount.container).spendPublicKey, m_viewPublicKey};
      std::string addressString = m_currency.accountAddressAsString(address);

      updated |= updateAddressTransfers(transactionId, firstTransferIdx, addressString, initialTransfers[addressString].input, containerAmount.amounts.input);
      updated |= updateAddressTransfers(transactionId, firstTransferIdx, addressString, initialTransfers[addressString].output, containerAmount.amounts.output);

      myInputsAmount += containerAmount.amounts.input;
      myOutputsAmount += containerAmount.amounts.output;

      if (containerAmount.amounts.input != 0)
      {
        myInputAddresses.emplace(addressString);
      }

      if (containerAmount.amounts.output != 0)
      {
        myOutputAddresses.emplace(addressString);
      }
    }

    assert(myInputsAmount >= allInputsAmount);
    assert(myOutputsAmount <= allOutputsAmount);

    int64_t knownInputsAmount = 0;
    int64_t knownOutputsAmount = 0;
    auto updatedTransfers = getKnownTransfersMap(transactionId, firstTransferIdx);
    for (const auto &pair : updatedTransfers)
    {
      knownInputsAmount += pair.second.input;
      knownOutputsAmount += pair.second.output;
    }

    assert(myInputsAmount >= knownInputsAmount);
    assert(myOutputsAmount <= knownOutputsAmount);

    updated |= updateUnknownTransfers(transactionId, firstTransferIdx, myInputAddresses, knownInputsAmount, myInputsAmount, allInputsAmount, false);
    updated |= updateUnknownTransfers(transactionId, firstTransferIdx, myOutputAddresses, knownOutputsAmount, myOutputsAmount, allOutputsAmount, true);

    return updated;
  }

  WalletGreen::TransfersMap WalletGreen::getKnownTransfersMap(size_t transactionId, size_t firstTransferIdx) const
  {
    TransfersMap result;

    for (auto it = std::next(m_transfers.begin(), firstTransferIdx); it != m_transfers.end() && it->first == transactionId; ++it)
    {
      const auto &address = it->second.address;

      if (!address.empty())
      {
        if (it->second.amount < 0)
        {
          result[address].input += it->second.amount;
        }
        else
        {
          assert(it->second.amount > 0);
          result[address].output += it->second.amount;
        }
      }
    }

    return result;
  }

  bool WalletGreen::updateAddressTransfers(size_t transactionId, size_t firstTransferIdx, const std::string &address, int64_t knownAmount, int64_t targetAmount)
  {
    assert((knownAmount > 0 && targetAmount > 0) || (knownAmount < 0 && targetAmount < 0) || knownAmount == 0 || targetAmount == 0);

    bool updated = false;

    if (knownAmount != targetAmount)
    {
      if (knownAmount == 0)
      {
        appendTransfer(transactionId, firstTransferIdx, address, targetAmount);
        updated = true;
      }
      else if (targetAmount == 0)
      {
        assert(knownAmount != 0);
        updated |= eraseTransfersByAddress(transactionId, firstTransferIdx, address, knownAmount > 0);
      }
      else
      {
        updated |= adjustTransfer(transactionId, firstTransferIdx, address, targetAmount);
      }
    }

    return updated;
  }

  bool WalletGreen::updateUnknownTransfers(size_t transactionId, size_t firstTransferIdx, const std::unordered_set<std::string> &myAddresses,
                                           int64_t knownAmount, int64_t myAmount, int64_t totalAmount, bool isOutput)
  {

    bool updated = false;

    if (std::abs(knownAmount) > std::abs(totalAmount))
    {
      updated |= eraseForeignTransfers(transactionId, firstTransferIdx, myAddresses, isOutput);
      if (totalAmount == myAmount)
      {
        updated |= eraseTransfersByAddress(transactionId, firstTransferIdx, std::string(), isOutput);
      }
      else
      {
        assert(std::abs(totalAmount) > std::abs(myAmount));
        updated |= adjustTransfer(transactionId, firstTransferIdx, std::string(), totalAmount - myAmount);
      }
    }
    else if (knownAmount == totalAmount)
    {
      updated |= eraseTransfersByAddress(transactionId, firstTransferIdx, std::string(), isOutput);
    }
    else
    {
      assert(std::abs(totalAmount) > std::abs(knownAmount));
      updated |= adjustTransfer(transactionId, firstTransferIdx, std::string(), totalAmount - knownAmount);
    }

    return updated;
  }

  void WalletGreen::appendTransfer(size_t transactionId, size_t firstTransferIdx, const std::string &address, int64_t amount)
  {
    auto it = std::next(m_transfers.begin(), firstTransferIdx);
    auto insertIt = std::upper_bound(it, m_transfers.end(), transactionId, [](size_t transactionId, const TransactionTransferPair &pair) {
      return transactionId < pair.first;
    });

    WalletTransfer transfer{WalletTransferType::USUAL, address, amount};
    m_transfers.emplace(insertIt, std::piecewise_construct, std::forward_as_tuple(transactionId), std::forward_as_tuple(transfer));
  }

  bool WalletGreen::adjustTransfer(size_t transactionId, size_t firstTransferIdx, const std::string &address, int64_t amount)
  {
    assert(amount != 0);

    bool updated = false;
    bool updateOutputTransfers = amount > 0;
    bool firstAddressTransferFound = false;
    auto it = std::next(m_transfers.begin(), firstTransferIdx);
    while (it != m_transfers.end() && it->first == transactionId)
    {
      assert(it->second.amount != 0);
      bool transferIsOutput = it->second.amount > 0;
      if (transferIsOutput == updateOutputTransfers && it->second.address == address)
      {
        if (firstAddressTransferFound)
        {
          it = m_transfers.erase(it);
          updated = true;
        }
        else
        {
          if (it->second.amount != amount)
          {
            it->second.amount = amount;
            updated = true;
          }

          firstAddressTransferFound = true;
          ++it;
        }
      }
      else
      {
        ++it;
      }
    }

    if (!firstAddressTransferFound)
    {
      WalletTransfer transfer{WalletTransferType::USUAL, address, amount};
      m_transfers.emplace(it, std::piecewise_construct, std::forward_as_tuple(transactionId), std::forward_as_tuple(transfer));
      updated = true;
    }

    return updated;
  }

  bool WalletGreen::eraseTransfers(size_t transactionId, size_t firstTransferIdx, std::function<bool(bool, const std::string &)> &&predicate)
  {
    bool erased = false;
    auto it = std::next(m_transfers.begin(), firstTransferIdx);
    while (it != m_transfers.end() && it->first == transactionId)
    {
      bool transferIsOutput = it->second.amount > 0;
      if (predicate(transferIsOutput, it->second.address))
      {
        it = m_transfers.erase(it);
        erased = true;
      }
      else
      {
        ++it;
      }
    }

    return erased;
  }

  bool WalletGreen::eraseTransfersByAddress(size_t transactionId, size_t firstTransferIdx, const std::string &address, bool eraseOutputTransfers)
  {
    return eraseTransfers(transactionId, firstTransferIdx, [&address, eraseOutputTransfers](bool isOutput, const std::string &transferAddress) {
      return eraseOutputTransfers == isOutput && address == transferAddress;
    });
  }

  bool WalletGreen::eraseForeignTransfers(size_t transactionId, size_t firstTransferIdx, const std::unordered_set<std::string> &knownAddresses,
                                          bool eraseOutputTransfers)
  {

    return eraseTransfers(transactionId, firstTransferIdx, [&knownAddresses, eraseOutputTransfers](bool isOutput, const std::string &transferAddress) {
      return eraseOutputTransfers == isOutput && knownAddresses.count(transferAddress) == 0;
    });
  }

  std::unique_ptr<cn::ITransaction> WalletGreen::makeTransaction(const std::vector<ReceiverAmounts> &decomposedOutputs,
                                                                         std::vector<InputInfo> &keysInfo, const std::vector<WalletMessage> &messages, const std::string &extra, uint64_t unlockTimestamp, crypto::SecretKey &transactionSK)
  {

    std::unique_ptr<ITransaction> tx = createTransaction();

    using AmountToAddress = std::pair<const AccountPublicAddress *, uint64_t>;
    std::vector<AmountToAddress> amountsToAddresses;
    for (const auto &output : decomposedOutputs)
    {
      for (auto amount : output.amounts)
      {
        amountsToAddresses.emplace_back(AmountToAddress{&output.receiver, amount});
      }
    }

    std::shuffle(amountsToAddresses.begin(), amountsToAddresses.end(), std::default_random_engine{crypto::rand<std::default_random_engine::result_type>()});
    std::sort(amountsToAddresses.begin(), amountsToAddresses.end(), [](const AmountToAddress &left, const AmountToAddress &right) {
      return left.second < right.second;
    });

    tx->setUnlockTime(unlockTimestamp);

    for (auto &input : keysInfo)
    {
      tx->addInput(makeAccountKeys(*input.walletRecord), input.keyInfo, input.ephKeys);
    }

    tx->setDeterministicTransactionSecretKey(m_viewSecretKey);
    tx->getTransactionSecretKey(transactionSK);
    crypto::PublicKey publicKey = tx->getTransactionPublicKey();
    cn::KeyPair kp = {publicKey, transactionSK};
    for (size_t i = 0; i < messages.size(); ++i)
    {
      cn::AccountPublicAddress addressBin;
      if (!m_currency.parseAccountAddressString(messages[i].address, addressBin))
        continue;
      cn::tx_extra_message tag;
      if (!tag.encrypt(i, messages[i].message, &addressBin, kp))
        continue;
      BinaryArray ba;
      if (cn::append_message_to_extra(ba, tag))
      {
        tx->appendExtra(ba);
      }
    }

    for (const auto &amountToAddress : amountsToAddresses)
    {
      tx->addOutput(amountToAddress.second, *amountToAddress.first);
    }

    tx->appendExtra(common::asBinaryArray(extra));

    size_t i = 0;
    for (const auto &input : keysInfo)
    {
      tx->signInputKey(i, input.keyInfo, input.ephKeys);
      i++;
    }

    return tx;
  }

  void WalletGreen::sendTransaction(const cn::Transaction &cryptoNoteTransaction)
  {
    std::error_code ec;
    throwIfStopped();
    auto relayTransactionCompleted = std::promise<std::error_code>();
    auto relayTransactionWaitFuture = relayTransactionCompleted.get_future();
    m_node.relayTransaction(cryptoNoteTransaction, [&ec, &relayTransactionCompleted](std::error_code)
                            {
                              auto detachedPromise = std::move(relayTransactionCompleted);
                              detachedPromise.set_value(ec);
                            });
    ec = relayTransactionWaitFuture.get();

    if (ec)
    {
      throw std::system_error(ec);
    }
  }

  size_t WalletGreen::validateSaveAndSendTransaction(
      const ITransactionReader &transaction,
      const std::vector<WalletTransfer> &destinations,
      bool isFusion,
      bool send)
  {
    BinaryArray transactionData = transaction.getTransactionData();

    if ((transactionData.size() > m_upperTransactionSizeLimit) && (isFusion == false))
    {
      m_logger(ERROR, BRIGHT_RED) << "Transaction is too big";
      throw std::system_error(make_error_code(error::TRANSACTION_SIZE_TOO_BIG));
    }

    if ((transactionData.size() > m_currency.fusionTxMaxSize()) && (isFusion == true))
    {
      m_logger(ERROR, BRIGHT_RED) << "Fusion transaction is too big. Transaction hash";
      throw std::system_error(make_error_code(error::TRANSACTION_SIZE_TOO_BIG));
    }

    cn::Transaction cryptoNoteTransaction;
    if (!fromBinaryArray(cryptoNoteTransaction, transactionData))
    {
      throw std::system_error(make_error_code(error::INTERNAL_WALLET_ERROR), "Failed to deserialize created transaction");
    }

    uint64_t fee = transaction.getInputTotalAmount() < transaction.getOutputTotalAmount() ? cn::parameters::MINIMUM_FEE : transaction.getInputTotalAmount() - transaction.getOutputTotalAmount();
    size_t transactionId = insertOutgoingTransactionAndPushEvent(transaction.getTransactionHash(), fee, transaction.getExtra(), transaction.getUnlockTime());
    tools::ScopeExit rollbackTransactionInsertion([this, transactionId] {
      updateTransactionStateAndPushEvent(transactionId, WalletTransactionState::FAILED);
    });

    m_fusionTxsCache.emplace(transactionId, isFusion);
    pushBackOutgoingTransfers(transactionId, destinations);

    addUnconfirmedTransaction(transaction);
    tools::ScopeExit rollbackAddingUnconfirmedTransaction([this, &transaction] {
      try
      {
        removeUnconfirmedTransaction(transaction.getTransactionHash());
      }
      catch (...)
      {
        m_logger(WARNING, BRIGHT_RED) << "Rollback has failed. The TX will be stored as unconfirmed and will be deleted after the wallet is relaunched during TX pool sync.";
        // Ignore any exceptions. If rollback fails then the transaction is stored as unconfirmed and will be deleted after wallet relaunch
        // during transaction pool synchronization
      }
    });

    if (send)
    {
      sendTransaction(cryptoNoteTransaction);
      updateTransactionStateAndPushEvent(transactionId, WalletTransactionState::SUCCEEDED);
    }
    else
    {
      assert(m_uncommitedTransactions.count(transactionId) == 0);
      m_uncommitedTransactions.emplace(transactionId, std::move(cryptoNoteTransaction));
    }

    rollbackAddingUnconfirmedTransaction.cancel();
    rollbackTransactionInsertion.cancel();

    return transactionId;
  }

  AccountKeys WalletGreen::makeAccountKeys(const WalletRecord &wallet) const
  {
    AccountKeys keys;
    keys.address.spendPublicKey = wallet.spendPublicKey;
    keys.address.viewPublicKey = m_viewPublicKey;
    keys.spendSecretKey = wallet.spendSecretKey;
    keys.viewSecretKey = m_viewSecretKey;

    return keys;
  }

  void WalletGreen::requestMixinOuts(
      const std::vector<OutputToTransfer> &selectedTransfers,
      uint64_t mixIn,
      std::vector<outs_for_amount> &mixinResult)
  {

    std::vector<uint64_t> amounts;
    for (const auto &out : selectedTransfers)
    {
      amounts.push_back(out.out.amount);
    }

    std::error_code mixinError;

    throwIfStopped();

    auto getRandomOutsByAmountsCompleted = std::promise<std::error_code>();
    auto getRandomOutsByAmountsWaitFuture = getRandomOutsByAmountsCompleted.get_future();

    m_node.getRandomOutsByAmounts(std::move(amounts), mixIn, mixinResult, [&getRandomOutsByAmountsCompleted](std::error_code ec) {
     auto detachedPromise = std::move(getRandomOutsByAmountsCompleted);
      detachedPromise.set_value(ec);
    });
    mixinError = getRandomOutsByAmountsWaitFuture.get();

    checkIfEnoughMixins(mixinResult, mixIn);

    if (mixinError)
    {
      throw std::system_error(mixinError);
    }
  }

  uint64_t WalletGreen::selectTransfers(
      uint64_t neededMoney,
      uint64_t dustThreshold,
      const std::vector<WalletOuts> &wallets,
      std::vector<OutputToTransfer> &selectedTransfers) const
  {
    uint64_t foundMoney = 0;

    using OutputData = std::pair<WalletRecord *, TransactionOutputInformation>;
    std::vector<OutputData> walletOuts;
    std::unordered_map<uint64_t, std::vector<OutputData>> buckets;

    for (const auto &wallet : wallets)
    {
      for (const auto &out : wallet.outs)
      {
        auto numberOfDigits = static_cast<int>(floor(log10(out.amount)) + 1);

        if (out.amount > dustThreshold)
        {
          buckets[numberOfDigits].emplace_back(
              std::piecewise_construct,
              std::forward_as_tuple(wallet.wallet),
              std::forward_as_tuple(out));
        }
      }
    }

    while (foundMoney < neededMoney && !buckets.empty())
    {
      /* Take one element from each bucket, smallest first. */
      for (auto bucket = buckets.begin(); bucket != buckets.end();)
      {
        /* Bucket has been exhausted, remove from list */
        if (bucket->second.empty())
        {
          bucket = buckets.erase(bucket);
        }
        else
        {
          /** Add the amount to the selected transfers so long as 
           * foundMoney is still less than neededMoney. This prevents 
           * larger outputs than we need when we already have enough funds */
          if (foundMoney < neededMoney)
          {
            auto out = bucket->second.back();
            selectedTransfers.emplace_back(OutputToTransfer{std::move(out.second), std::move(out.first)});
            foundMoney += out.second.amount;
          }

          /* Remove amount we just added */
          bucket->second.pop_back();
          bucket++;
        }
      }
    }
    return foundMoney;
  };

  std::vector<WalletGreen::WalletOuts> WalletGreen::pickWalletsWithMoney() const
  {
    auto &walletsIndex = m_walletsContainer.get<RandomAccessIndex>();

    std::vector<WalletOuts> walletOuts;
    for (const auto &wallet : walletsIndex)
    {
      if (wallet.actualBalance == 0)
      {
        continue;
      }

      const ITransfersContainer *container = wallet.container;

      WalletOuts outs;
      container->getOutputs(outs.outs, ITransfersContainer::IncludeKeyUnlocked);
      outs.wallet = const_cast<WalletRecord *>(&wallet);

      walletOuts.push_back(std::move(outs));
    }

    return walletOuts;
  }

  WalletGreen::WalletOuts WalletGreen::pickWallet(const std::string &address) const
  {
    const auto &wallet = getWalletRecord(address);

    const ITransfersContainer *container = wallet.container;
    WalletOuts outs;
    container->getOutputs(outs.outs, ITransfersContainer::IncludeKeyUnlocked);
    outs.wallet = const_cast<WalletRecord *>(&wallet);

    return outs;
  }

  std::vector<WalletGreen::WalletOuts> WalletGreen::pickWallets(const std::vector<std::string> &addresses) const
  {
    std::vector<WalletOuts> wallets;
    wallets.reserve(addresses.size());

    for (const auto &address : addresses)
    {
      WalletOuts wallet = pickWallet(address);
      if (!wallet.outs.empty())
      {
        wallets.emplace_back(std::move(wallet));
      }
    }

    return wallets;
  }

  std::vector<cn::WalletGreen::ReceiverAmounts> WalletGreen::splitDestinations(const std::vector<cn::WalletTransfer> &destinations,
                                                                                       uint64_t dustThreshold,
                                                                                       const cn::Currency &currency) const
  {

    std::vector<ReceiverAmounts> decomposedOutputs;
    for (const auto &destination : destinations)
    {
      AccountPublicAddress address;
      parseAddressString(destination.address, currency, address);
      decomposedOutputs.push_back(splitAmount(destination.amount, address, dustThreshold));
    }

    return decomposedOutputs;
  }

  cn::WalletGreen::ReceiverAmounts WalletGreen::splitAmount(
      uint64_t amount,
      const AccountPublicAddress &destination,
      uint64_t dustThreshold) const
  {

    ReceiverAmounts receiverAmounts;

    receiverAmounts.receiver = destination;
    decomposeAmount(amount, dustThreshold, receiverAmounts.amounts);
    return receiverAmounts;
  }

  void WalletGreen::prepareInputs(
      const std::vector<OutputToTransfer> &selectedTransfers,
      std::vector<outs_for_amount> &mixinResult,
      uint64_t mixIn,
      std::vector<InputInfo> &keysInfo) const
  {

    using out_entry = cn::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry;

    size_t i = 0;
    for (const auto &input : selectedTransfers)
    {
      transaction_types::InputKeyInfo keyInfo;
      keyInfo.amount = input.out.amount;

      if (mixinResult.size())
      {
        std::sort(mixinResult[i].outs.begin(), mixinResult[i].outs.end(),
                  [](const out_entry &a, const out_entry &b) { return a.global_amount_index < b.global_amount_index; });
        for (const auto &fakeOut : mixinResult[i].outs)
        {
          if (input.out.globalOutputIndex == fakeOut.global_amount_index)
          {
            continue;
          }

          transaction_types::GlobalOutput globalOutput;
          globalOutput.outputIndex = static_cast<uint32_t>(fakeOut.global_amount_index);
          globalOutput.targetKey = fakeOut.out_key;
          keyInfo.outputs.push_back(std::move(globalOutput));
          if (keyInfo.outputs.size() >= mixIn)
          {
            break;
          }
        }
      }

      //paste real transaction to the random index
      auto insertIn = std::find_if(keyInfo.outputs.begin(), keyInfo.outputs.end(), [&](const transaction_types::GlobalOutput &a) {
        return a.outputIndex >= input.out.globalOutputIndex;
      });

      transaction_types::GlobalOutput realOutput;
      realOutput.outputIndex = input.out.globalOutputIndex;
      realOutput.targetKey = input.out.outputKey;

      auto insertedIn = keyInfo.outputs.insert(insertIn, realOutput);

      keyInfo.realOutput.transactionPublicKey = reinterpret_cast<const PublicKey &>(input.out.transactionPublicKey);
      keyInfo.realOutput.transactionIndex = static_cast<size_t>(insertedIn - keyInfo.outputs.begin());
      keyInfo.realOutput.outputInTransaction = input.out.outputInTransaction;

      //Important! outputs in selectedTransfers and in keysInfo must have the same order!
      InputInfo inputInfo;
      inputInfo.keyInfo = std::move(keyInfo);
      inputInfo.walletRecord = input.wallet;
      keysInfo.push_back(std::move(inputInfo));
      ++i;
    }
  }

  WalletTransactionWithTransfers WalletGreen::getTransaction(const crypto::Hash &transactionHash) const
  {
    throwIfNotInitialized();
    throwIfStopped();

    auto &hashIndex = m_transactions.get<TransactionIndex>();
    auto it = hashIndex.find(transactionHash);
    if (it == hashIndex.end())
    {
      throw std::system_error(make_error_code(error::OBJECT_NOT_FOUND), "Transaction not found");
    }

    WalletTransactionWithTransfers walletTransaction;
    walletTransaction.transaction = *it;
    walletTransaction.transfers = getTransactionTransfers(*it);

    return walletTransaction;
  }

  std::vector<TransactionsInBlockInfo> WalletGreen::getTransactions(const crypto::Hash &blockHash, size_t count) const
  {
    throwIfNotInitialized();
    throwIfStopped();

    auto &hashIndex = m_blockchain.get<BlockHashIndex>();
    auto it = hashIndex.find(blockHash);
    if (it == hashIndex.end())
    {
      return std::vector<TransactionsInBlockInfo>();
    }

    auto heightIt = m_blockchain.project<BlockHeightIndex>(it);

    auto blockIndex = static_cast<uint32_t>(std::distance(m_blockchain.get<BlockHeightIndex>().begin(), heightIt));
    return getTransactionsInBlocks(blockIndex, count);
  }

  std::vector<DepositsInBlockInfo> WalletGreen::getDeposits(const crypto::Hash &blockHash, size_t count) const
  {
    throwIfNotInitialized();
    throwIfStopped();

    auto &hashIndex = m_blockchain.get<BlockHashIndex>();
    auto it = hashIndex.find(blockHash);
    if (it == hashIndex.end())
    {
      return std::vector<DepositsInBlockInfo>();
    }

    auto heightIt = m_blockchain.project<BlockHeightIndex>(it);

    auto blockIndex = static_cast<uint32_t>(std::distance(m_blockchain.get<BlockHeightIndex>().begin(), heightIt));
    return getDepositsInBlocks(blockIndex, count);
  }

  std::vector<TransactionsInBlockInfo> WalletGreen::getTransactions(uint32_t blockIndex, size_t count) const
  {
    throwIfNotInitialized();
    throwIfStopped();

    return getTransactionsInBlocks(blockIndex, count);
  }

  std::vector<DepositsInBlockInfo> WalletGreen::getDeposits(uint32_t blockIndex, size_t count) const
  {
    throwIfNotInitialized();
    throwIfStopped();

    return getDepositsInBlocks(blockIndex, count);
  }

  std::vector<crypto::Hash> WalletGreen::getBlockHashes(uint32_t blockIndex, size_t count) const
  {
    throwIfNotInitialized();
    throwIfStopped();

    auto &index = m_blockchain.get<BlockHeightIndex>();

    if (blockIndex >= index.size())
    {
      return std::vector<crypto::Hash>();
    }

    auto start = std::next(index.begin(), blockIndex);
    auto end = std::next(index.begin(), std::min(index.size(), blockIndex + count));
    return std::vector<crypto::Hash>(start, end);
  }

  uint32_t WalletGreen::getBlockCount() const
  {
    throwIfNotInitialized();
    throwIfStopped();

    auto blockCount = static_cast<uint32_t>(m_blockchain.size());
    assert(blockCount != 0);

    return blockCount;
  }

  std::vector<WalletTransactionWithTransfers> WalletGreen::getUnconfirmedTransactions() const
  {
    throwIfNotInitialized();
    throwIfStopped();

    std::vector<WalletTransactionWithTransfers> result;
    auto lowerBound = m_transactions.get<BlockHeightIndex>().lower_bound(WALLET_UNCONFIRMED_TRANSACTION_HEIGHT);
    for (auto it = lowerBound; it != m_transactions.get<BlockHeightIndex>().end(); ++it)
    {
      if (it->state != WalletTransactionState::SUCCEEDED)
      {
        continue;
      }

      WalletTransactionWithTransfers transaction;
      transaction.transaction = *it;
      transaction.transfers = getTransactionTransfers(*it);

      result.push_back(transaction);
    }

    return result;
  }

  std::vector<size_t> WalletGreen::getDelayedTransactionIds() const
  {
    throwIfNotInitialized();
    throwIfStopped();
    throwIfTrackingMode();

    std::vector<size_t> result;
    result.reserve(m_uncommitedTransactions.size());

    for (const auto &kv : m_uncommitedTransactions)
    {
      result.push_back(kv.first);
    }

    return result;
  }

  void WalletGreen::start()
  {
    m_stopped = false;
  }

  void WalletGreen::stop()
  {
    m_stopped = true;
    m_eventOccurred.set();
  }

  WalletEvent WalletGreen::getEvent()
  {
    throwIfNotInitialized();
    throwIfStopped();

    while (m_events.empty())
    {
      m_eventOccurred.wait();
      m_eventOccurred.clear();
      throwIfStopped();
    }

    WalletEvent event = std::move(m_events.front());
    m_events.pop();

    return event;
  }

  void WalletGreen::throwIfNotInitialized() const
  {
    if (m_state != WalletState::INITIALIZED)
    {
      throw std::system_error(make_error_code(cn::error::NOT_INITIALIZED));
    }
  }

  void WalletGreen::onError(ITransfersSubscription *object, uint32_t height, std::error_code ec)
  {
    // Do nothing
  }

  void WalletGreen::synchronizationProgressUpdated(uint32_t processedBlockCount, uint32_t totalBlockCount)
  {
    m_dispatcher.remoteSpawn([processedBlockCount, totalBlockCount, this]() { onSynchronizationProgressUpdated(processedBlockCount, totalBlockCount); });
  }

  void WalletGreen::synchronizationCompleted(std::error_code result)
  {
    m_observerManager.notify(&IWalletObserver::synchronizationCompleted, result);
    m_dispatcher.remoteSpawn([this]() { onSynchronizationCompleted(); });
  }

  void WalletGreen::onSynchronizationProgressUpdated(uint32_t processedBlockCount, uint32_t totalBlockCount)
  {
    assert(processedBlockCount > 0);

    platform_system::EventLock lk(m_readyEvent);

    if (m_state == WalletState::NOT_INITIALIZED)
    {
      return;
    }

    pushEvent(makeSyncProgressUpdatedEvent(processedBlockCount, totalBlockCount));

    uint32_t currentHeight = processedBlockCount - 1;
    unlockBalances(currentHeight);
  }

  void WalletGreen::onSynchronizationCompleted()
  {
    platform_system::EventLock lk(m_readyEvent);

    if (m_state == WalletState::NOT_INITIALIZED)
    {
      return;
    }

    pushEvent(makeSyncCompletedEvent());
  }

  void WalletGreen::onBlocksAdded(const crypto::PublicKey &viewPublicKey, const std::vector<crypto::Hash> &blockHashes)
  {
    m_dispatcher.remoteSpawn([this, blockHashes]() { blocksAdded(blockHashes); });
  }

  void WalletGreen::blocksAdded(const std::vector<crypto::Hash> &blockHashes)
  {
    platform_system::EventLock lk(m_readyEvent);

    if (m_state == WalletState::NOT_INITIALIZED)
    {
      return;
    }
    m_blockchain.insert(m_blockchain.end(), blockHashes.begin(), blockHashes.end());
  }

  void WalletGreen::onBlockchainDetach(const crypto::PublicKey &viewPublicKey, uint32_t blockIndex)
  {
    m_dispatcher.remoteSpawn([this, blockIndex]() { blocksRollback(blockIndex); });
  }

  void WalletGreen::blocksRollback(uint32_t blockIndex)
  {
    platform_system::EventLock lk(m_readyEvent);

    if (m_state == WalletState::NOT_INITIALIZED)
    {
      return;
    }

    auto &blockHeightIndex = m_blockchain.get<BlockHeightIndex>();
    blockHeightIndex.erase(std::next(blockHeightIndex.begin(), blockIndex), blockHeightIndex.end());
  }

  void WalletGreen::onTransactionDeleteBegin(const crypto::PublicKey &viewPublicKey, const crypto::Hash &transactionHash)
  {
    m_dispatcher.remoteSpawn([this, &transactionHash]() { transactionDeleteBegin(transactionHash); });
  }

  // TODO remove
  void WalletGreen::transactionDeleteBegin(const crypto::Hash& /*transactionHash*/)
  {
  }

  void WalletGreen::onTransactionDeleteEnd(const crypto::PublicKey &viewPublicKey, const crypto::Hash &transactionHash)
  {
    m_dispatcher.remoteSpawn([this, &transactionHash]() { transactionDeleteEnd(transactionHash); });
  }

  // TODO remove
  void WalletGreen::transactionDeleteEnd(const crypto::Hash &transactionHash)
  {
  }

  void WalletGreen::unlockBalances(uint32_t height)
  {
    auto &index = m_unlockTransactionsJob.get<BlockHeightIndex>();
    auto upper = index.upper_bound(height);

    if (index.begin() != upper)
    {
      for (auto it = index.begin(); it != upper; ++it)
      {
        updateBalance(it->container);
      }

      index.erase(index.begin(), upper);
      pushEvent(makeMoneyUnlockedEvent());
    }
  }

  void WalletGreen::onTransactionUpdated(ITransfersSubscription * /*object*/, const crypto::Hash & /*transactionHash*/)
  {
    // Deprecated, ignore it. New event handler is onTransactionUpdated(const crypto::PublicKey&, const crypto::Hash&, const std::vector<ITransfersContainer*>&)
  }

  void WalletGreen::onTransactionUpdated(
      const crypto::PublicKey &,
      const crypto::Hash &transactionHash,
      const std::vector<ITransfersContainer *> &containers)
  {
    assert(!containers.empty());

    TransactionInformation info;
    std::vector<ContainerAmounts> containerAmountsList;
    containerAmountsList.reserve(containers.size());
    for (auto container : containers)
    {
      uint64_t inputsAmount;
      // Don't move this code to the following remote spawn, because it guarantees that the container has the
      // transaction
      uint64_t outputsAmount;
      bool found = container->getTransactionInformation(transactionHash, info, &inputsAmount, &outputsAmount);
      (void)found;
      assert(found);

      ContainerAmounts containerAmounts;
      containerAmounts.container = container;
      containerAmounts.amounts.input = -static_cast<int64_t>(inputsAmount);
      containerAmounts.amounts.output = static_cast<int64_t>(outputsAmount);
      containerAmountsList.emplace_back(std::move(containerAmounts));
    }

    m_dispatcher.remoteSpawn(
        [this, info, containerAmountsList] { this->transactionUpdated(info, containerAmountsList); });
  }

  /* Insert a new deposit into the deposit index */
  DepositId WalletGreen::insertNewDeposit(
      const TransactionOutputInformation &depositOutput,
      TransactionId creatingTransactionId,
      const Currency &currency,
      uint32_t height)
  {
    assert(depositOutput.type == transaction_types::OutputType::Multisignature);
    assert(depositOutput.term != 0);

    Deposit deposit;
    deposit.amount = depositOutput.amount;
    deposit.creatingTransactionId = creatingTransactionId;
    deposit.term = depositOutput.term;
    deposit.spendingTransactionId = WALLET_INVALID_TRANSACTION_ID;
    deposit.interest = currency.calculateInterest(deposit.amount, deposit.term, height);
    deposit.height = height;
    deposit.unlockHeight = height + depositOutput.term;
    deposit.locked = true;

    return insertDeposit(deposit, depositOutput.outputInTransaction, depositOutput.transactionHash);
  }

  DepositId WalletGreen::insertDeposit(
      const Deposit &deposit,
      size_t depositIndexInTransaction,
      const Hash &transactionHash)
  {

    Deposit info = deposit;

    info.outputInTransaction = static_cast<uint32_t>(depositIndexInTransaction);
    info.transactionHash = transactionHash;

    auto &hashIndex = m_transactions.get<TransactionIndex>();
    auto it = hashIndex.find(transactionHash);
    if (it == hashIndex.end())
    {
      throw std::system_error(make_error_code(error::OBJECT_NOT_FOUND), "Transaction not found");
    }

    WalletTransactionWithTransfers walletTransaction;
    walletTransaction.transaction = *it;
    walletTransaction.transfers = getTransactionTransfers(*it);

    DepositId id = m_deposits.size();
    m_deposits.push_back(std::move(info));

    m_logger(DEBUGGING, BRIGHT_GREEN) << "New deposit created, id "
                                      << id << ", locking "
                                      << m_currency.formatAmount(deposit.amount) << " ,for a term of "
                                      << deposit.term << " blocks, at block "
                                      << deposit.height;

    return id;
  }

  std::vector<TransactionOutputInformation> WalletGreen::getUnspentOutputs()
  {
    std::vector<TransactionOutputInformation> unspentOutputs;
    const auto &walletsIndex = m_walletsContainer.get<RandomAccessIndex>();

    std::vector<WalletOuts> walletOuts;
    for (const auto &wallet : walletsIndex)
    {
      const ITransfersContainer *container = wallet.container;

      std::vector<TransactionOutputInformation> outputs;
      container->getOutputs(outputs, ITransfersContainer::IncludeKeyUnlocked);
      unspentOutputs.insert(unspentOutputs.end(), outputs.begin(), outputs.end());
    }
    return unspentOutputs;
  }

  size_t WalletGreen::getUnspentOutputsCount()
  {

    return getUnspentOutputs().size();
  }

  std::string WalletGreen::getReserveProof(const std::string &address, const uint64_t &reserve, const std::string &message)
  {
    const WalletRecord& wallet = getWalletRecord(address);
    const AccountKeys& keys = makeAccountKeys(wallet);

    if (keys.spendSecretKey == NULL_SECRET_KEY)
    {
      throw std::runtime_error("Reserve proof can only be generated by a full wallet");
    }
    uint64_t actualBalance = getActualBalance();
    if (actualBalance == 0)
    {
      throw std::runtime_error("Zero balance");
    }

    if (actualBalance < reserve)
    {
      throw std::runtime_error("Not enough balance for the requested minimum reserve amount");
    }

    // determine which outputs to include in the proof
    std::vector<TransactionOutputInformation> selected_transfers;
    wallet.container->getOutputs(selected_transfers, ITransfersContainer::IncludeKeyUnlocked);

    // minimize the number of outputs included in the proof, by only picking the N largest outputs that can cover the requested min reserve amount
    auto compareTransactionOutputInformationByAmount = [](const TransactionOutputInformation &a, const TransactionOutputInformation &b)
    { return a.amount < b.amount; };
    std::sort(selected_transfers.begin(), selected_transfers.end(), compareTransactionOutputInformationByAmount);
    while (selected_transfers.size() >= 2 && selected_transfers[1].amount >= reserve)
    {
      selected_transfers.erase(selected_transfers.begin());
    }
    size_t sz = 0;
    uint64_t total = 0;
    while (total < reserve)
    {
      total += selected_transfers[sz].amount;
      ++sz;
    }
    selected_transfers.resize(sz);

    // compute signature prefix hash
    std::string prefix_data = message;
    prefix_data.append((const char *)&keys.address, sizeof(cn::AccountPublicAddress));

    std::vector<crypto::KeyImage> kimages;
    cn::KeyPair ephemeral;

    for (const auto &td : selected_transfers)
    {
      // derive ephemeral secret key
      crypto::KeyImage ki;
      const bool r = cn::generate_key_image_helper(keys, td.transactionPublicKey, td.outputInTransaction, ephemeral, ki);
      if (!r)
      {
        throw std::runtime_error("Failed to generate key image");
      }
      // now we can insert key image
      prefix_data.append((const char *)&ki, sizeof(crypto::PublicKey));
      kimages.push_back(ki);
    }

    crypto::Hash prefix_hash;
    crypto::cn_fast_hash(prefix_data.data(), prefix_data.size(), prefix_hash);

    // generate proof entries
    std::vector<reserve_proof_entry> proofs(selected_transfers.size());

    for (size_t i = 0; i < selected_transfers.size(); ++i)
    {
      const TransactionOutputInformation &td = selected_transfers[i];
      reserve_proof_entry &proof = proofs[i];
      proof.key_image = kimages[i];
      proof.txid = td.transactionHash;
      proof.index_in_tx = td.outputInTransaction;

      const auto& txPubKey = td.transactionPublicKey;

      for (int i = 0; i < 2; ++i)
      {
        crypto::KeyImage sk = crypto::scalarmultKey(*reinterpret_cast<const crypto::KeyImage *>(&txPubKey), *reinterpret_cast<const crypto::KeyImage *>(&keys.viewSecretKey));
        proof.shared_secret = *reinterpret_cast<const crypto::PublicKey *>(&sk);

        crypto::KeyDerivation derivation;
        if (!crypto::generate_key_derivation(proof.shared_secret, keys.viewSecretKey, derivation))
        {
          throw std::runtime_error("Failed to generate key derivation");
        }
      }

      // generate signature for shared secret
      crypto::generate_tx_proof(prefix_hash, keys.address.viewPublicKey, txPubKey, proof.shared_secret, keys.viewSecretKey, proof.shared_secret_sig);

      // derive ephemeral secret key
      crypto::KeyImage ki;
      cn::KeyPair ephemeral;

      const bool r = cn::generate_key_image_helper(keys, td.transactionPublicKey, td.outputInTransaction, ephemeral, ki);
      if (!r)
      {
        throw std::runtime_error("Failed to generate key image");
      }

      if (ephemeral.publicKey != td.outputKey)
      {
        throw std::runtime_error("Derived public key doesn't agree with the stored one");
      }

      // generate signature for key image
      const std::vector<const crypto::PublicKey *> &pubs = {&ephemeral.publicKey};

      crypto::generate_ring_signature(prefix_hash, proof.key_image, &pubs[0], 1, ephemeral.secretKey, 0, &proof.key_image_sig);
    }
    // generate signature for the spend key that received those outputs
    crypto::Signature signature;
    crypto::generate_signature(prefix_hash, keys.address.spendPublicKey, keys.spendSecretKey, signature);

    // serialize & encode
    reserve_proof p;
    p.proofs.assign(proofs.begin(), proofs.end());
    memcpy(&p.signature, &signature, sizeof(signature));

    BinaryArray ba = toBinaryArray(p);
    std::string ret = common::toHex(ba);

    ret = "ReserveProofV1" + tools::base_58::encode(ret);

    return ret;
  }

  bool WalletGreen::getTxProof(const crypto::Hash &transactionHash, const cn::AccountPublicAddress &address, const crypto::SecretKey &tx_key, std::string &signature)
  {
    const crypto::KeyImage p = *reinterpret_cast<const crypto::KeyImage *>(&address.viewPublicKey);
    const crypto::KeyImage k = *reinterpret_cast<const crypto::KeyImage *>(&tx_key);
    crypto::KeyImage pk = crypto::scalarmultKey(p, k);
    crypto::PublicKey R;
    crypto::PublicKey rA = reinterpret_cast<const PublicKey &>(pk);
    crypto::secret_key_to_public_key(tx_key, R);
    crypto::Signature sig;
    try
    {
      crypto::generate_tx_proof(transactionHash, R, address.viewPublicKey, rA, tx_key, sig);
    }
    catch (std::runtime_error&)
    {
      return false;
    }

    signature = std::string("ProofV1") +
                tools::base_58::encode(std::string((const char *)&rA, sizeof(crypto::PublicKey))) +
                tools::base_58::encode(std::string((const char *)&sig, sizeof(crypto::Signature)));

    return true;
  }

  /* Process transactions, this covers both new transactions AND confirmed transactions */
  void WalletGreen::transactionUpdated(
      TransactionInformation transactionInfo,
      const std::vector<ContainerAmounts> &containerAmountsList)
  {
    platform_system::EventLock lk(m_readyEvent);

    if (m_state == WalletState::NOT_INITIALIZED)
    {
      return;
    }

    size_t firstDepositId = WALLET_INVALID_DEPOSIT_ID;
    size_t depositCount = 0;

    bool updated = false;
    bool isNew = false;

    int64_t totalAmount = std::accumulate(containerAmountsList.begin(), containerAmountsList.end(), static_cast<int64_t>(0),
                                          [](int64_t sum, const ContainerAmounts &containerAmounts) { return sum + containerAmounts.amounts.input + containerAmounts.amounts.output; });

    size_t transactionId;
    auto &hashIndex = m_transactions.get<TransactionIndex>();
    auto it = hashIndex.find(transactionInfo.transactionHash);
    if (it != hashIndex.end())
    {
      transactionId = std::distance(m_transactions.get<RandomAccessIndex>().begin(), m_transactions.project<RandomAccessIndex>(it));
      updated |= updateWalletTransactionInfo(transactionId, transactionInfo, totalAmount);
    }
    else
    {
      isNew = true;
      transactionId = insertBlockchainTransaction(transactionInfo, totalAmount);
      m_fusionTxsCache.emplace(transactionId, isFusionTransaction(*it));
    }

    for (const auto& containerAmounts : containerAmountsList)
    {
      auto newDepositOuts = containerAmounts.container->getTransactionOutputs(transactionInfo.transactionHash, ITransfersContainer::IncludeTypeDeposit | ITransfersContainer::IncludeStateAll);
      auto spentDepositOutputs = containerAmounts.container->getTransactionInputs(transactionInfo.transactionHash, ITransfersContainer::IncludeTypeDeposit);

      std::vector<DepositId> updatedDepositIds;

      /* Check for new deposits in this transaction, and create them */
      for (const auto& depositOutput: newDepositOuts)
      {
        /* We only add confirmed deposit entries, so this condition prevents the same deposit
        in the deposit index during creation and during confirmation */
        if (transactionInfo.blockHeight == WALLET_UNCONFIRMED_TRANSACTION_HEIGHT)
        {
          continue;
        }
        auto id = insertNewDeposit(depositOutput, transactionId, m_currency, transactionInfo.blockHeight);
        updatedDepositIds.push_back(id);
      }

      /* Now check for any deposit withdrawals in the transactions */
      for (const auto& depositOutput: spentDepositOutputs)
      {
        auto depositId = getDepositId(depositOutput.transactionHash);
        assert(depositId != WALLET_INVALID_DEPOSIT_ID);
        if (depositId == WALLET_INVALID_DEPOSIT_ID)
        {
          throw std::invalid_argument("processSpentDeposits error: requested deposit doesn't exist");
        }

        auto info = m_deposits[depositId];
        info.spendingTransactionId = transactionId;
        updated |= updateWalletDepositInfo(depositId, info);
      }

      /* If there are new deposits, update the transaction information with the 
         firstDepositId and the depositCount */
      if (!updatedDepositIds.empty())
      {
        firstDepositId = updatedDepositIds[0];
        depositCount = updatedDepositIds.size();
        transactionInfo.depositCount = depositCount;
        transactionInfo.firstDepositId = firstDepositId;
        updated |= updateWalletTransactionInfo(transactionId, transactionInfo, totalAmount);
      }
    }

    if (transactionInfo.blockHeight != cn::WALLET_UNCONFIRMED_TRANSACTION_HEIGHT)
    {
      // In some cases a transaction can be included to a block but not removed from m_uncommitedTransactions. Fix it
      m_uncommitedTransactions.erase(transactionId);
    }

    // Update cached balance
    for (auto containerAmounts : containerAmountsList)
    {
      updateBalance(containerAmounts.container);

      if (transactionInfo.blockHeight != cn::WALLET_UNCONFIRMED_TRANSACTION_HEIGHT)
      {
        uint32_t unlockHeight = std::max(transactionInfo.blockHeight + m_transactionSoftLockTime, static_cast<uint32_t>(transactionInfo.unlockTime));
        insertUnlockTransactionJob(transactionInfo.transactionHash, unlockHeight, containerAmounts.container);
      }
    }

    updated |= updateTransactionTransfers(transactionId, containerAmountsList, -static_cast<int64_t>(transactionInfo.totalAmountIn),
                                          static_cast<int64_t>(transactionInfo.totalAmountOut));

    if (isNew)
    {
      pushEvent(makeTransactionCreatedEvent(transactionId));
    }
    else if (updated)
    {
      pushEvent(makeTransactionUpdatedEvent(transactionId));
    }
  }

  void WalletGreen::pushEvent(const WalletEvent &event)
  {
    m_events.push(event);
    m_eventOccurred.set();
  }

  size_t WalletGreen::getTransactionId(const Hash &transactionHash) const
  {
    auto it = m_transactions.get<TransactionIndex>().find(transactionHash);

    if (it == m_transactions.get<TransactionIndex>().end())
    {
      throw std::system_error(make_error_code(std::errc::invalid_argument));
    }

    auto rndIt = m_transactions.project<RandomAccessIndex>(it);
    auto txId = std::distance(m_transactions.get<RandomAccessIndex>().begin(), rndIt);

    return txId;
  }

  size_t WalletGreen::getDepositId(const Hash &transactionHash) const
  {
    auto it = m_deposits.get<TransactionIndex>().find(transactionHash);

    if (it == m_deposits.get<TransactionIndex>().end())
    {
      return WALLET_INVALID_DEPOSIT_ID;
    }

    auto rndIt = m_deposits.project<RandomAccessIndex>(it);
    auto depositId = std::distance(m_deposits.get<RandomAccessIndex>().begin(), rndIt);

    return depositId;
  }

  void WalletGreen::onTransactionDeleted(ITransfersSubscription *object, const Hash &transactionHash)
  {
    m_dispatcher.remoteSpawn([object, transactionHash, this]() { this->transactionDeleted(object, transactionHash); });
  }

  void WalletGreen::transactionDeleted(ITransfersSubscription *object, const Hash &transactionHash)
  {
    platform_system::EventLock lk(m_readyEvent);

    if (m_state == WalletState::NOT_INITIALIZED)
    {
      return;
    }

    auto it = m_transactions.get<TransactionIndex>().find(transactionHash);
    if (it == m_transactions.get<TransactionIndex>().end())
    {
      return;
    }

    cn::ITransfersContainer *container = &object->getContainer();
    updateBalance(container);
    deleteUnlockTransactionJob(transactionHash);

    bool updated = false;
    m_transactions.get<TransactionIndex>().modify(it, [&updated](cn::WalletTransaction &tx) {
      if (tx.state == WalletTransactionState::CREATED || tx.state == WalletTransactionState::SUCCEEDED)
      {
        tx.state = WalletTransactionState::CANCELLED;
        updated = true;
      }

      if (tx.blockHeight != WALLET_UNCONFIRMED_TRANSACTION_HEIGHT)
      {
        tx.blockHeight = WALLET_UNCONFIRMED_TRANSACTION_HEIGHT;
        updated = true;
      }
    });

    if (updated)
    {
      auto transactionId = getTransactionId(transactionHash);
      pushEvent(makeTransactionUpdatedEvent(transactionId));
    }
  }

  void WalletGreen::insertUnlockTransactionJob(const Hash &transactionHash, uint32_t blockHeight, cn::ITransfersContainer *container)
  {
    auto &index = m_unlockTransactionsJob.get<BlockHeightIndex>();
    index.insert({blockHeight, container, transactionHash});
  }

  void WalletGreen::deleteUnlockTransactionJob(const Hash &transactionHash)
  {
    auto &index = m_unlockTransactionsJob.get<TransactionHashIndex>();
    index.erase(transactionHash);
  }

  void WalletGreen::startBlockchainSynchronizer()
  {
    if (!m_walletsContainer.empty() && !m_blockchainSynchronizerStarted)
    {
      m_blockchainSynchronizer.start();
      m_blockchainSynchronizerStarted = true;
    }
  }

  void WalletGreen::stopBlockchainSynchronizer()
  {
    if (m_blockchainSynchronizerStarted)
    {
      m_blockchainSynchronizer.stop();
      m_blockchainSynchronizerStarted = false;
    }
  }

  void WalletGreen::addUnconfirmedTransaction(const ITransactionReader &transaction)
  {
    auto addUnconfirmedTransactionCompleted = std::promise<std::error_code>();
    auto addUnconfirmedTransactionWaitFuture = addUnconfirmedTransactionCompleted.get_future();

    addUnconfirmedTransactionWaitFuture = m_blockchainSynchronizer.addUnconfirmedTransaction(transaction);

    std::error_code ec = addUnconfirmedTransactionWaitFuture.get();
    if (ec)
    {
      throw std::system_error(ec, "Failed to add unconfirmed transaction");
    }
  }

  void WalletGreen::removeUnconfirmedTransaction(const crypto::Hash &transactionHash)
  {
    platform_system::RemoteContext<void> context(m_dispatcher, [this, &transactionHash] {
      m_blockchainSynchronizer.removeUnconfirmedTransaction(transactionHash).get();
    });

    context.get();
  }

  void WalletGreen::updateBalance(cn::ITransfersContainer *container)
  {
    auto it = m_walletsContainer.get<TransfersContainerIndex>().find(container);

    if (it == m_walletsContainer.get<TransfersContainerIndex>().end())
    {
      return;
    }

    bool updated = false;

    /* First get the available and pending balances from the container */
    uint64_t actual = container->balance(ITransfersContainer::IncludeKeyUnlocked);
    uint64_t pending = container->balance(ITransfersContainer::IncludeKeyNotUnlocked);

    /* Now update the overall balance (getBalance without parameters) */
    if (it->actualBalance < actual)
    {
      m_actualBalance += actual - it->actualBalance;
      updated = true;
    }
    else if (it->actualBalance > actual)
    {
      m_actualBalance -= it->actualBalance - actual;
      updated = true;
    }

    if (it->pendingBalance < pending)
    {
      m_pendingBalance += pending - it->pendingBalance;
      updated = true;
    }
    else if (it->pendingBalance > pending)
    {
      m_pendingBalance -= it->pendingBalance - pending;
      updated = true;
    }

    /* Update locked deposit balance, this will cover deposits, as well 
       as investments since they are all deposits with different parameters */
    std::vector<TransactionOutputInformation> transfers2;
    container->getOutputs(transfers2, ITransfersContainer::IncludeTypeDeposit | ITransfersContainer::IncludeStateLocked | ITransfersContainer::IncludeStateSoftLocked);

    std::vector<uint32_t> heights2;
    for (const auto &transfer2 : transfers2)
    {
      crypto::Hash hash2 = transfer2.transactionHash;
      TransactionInformation info2;
      if (container->getTransactionInformation(hash2, info2, nullptr, nullptr))
      {
        heights2.push_back(info2.blockHeight);
      }
    }
    uint64_t locked = calculateDepositsAmount(transfers2, m_currency, heights2);

    /* This updates the unlocked deposit balance, these are the deposits that have matured
       and can be withdrawn */
    std::vector<TransactionOutputInformation> transfers;
    container->getOutputs(transfers, ITransfersContainer::IncludeTypeDeposit | ITransfersContainer::IncludeStateUnlocked);

    std::vector<uint32_t> heights;
    for (const auto &transfer : transfers)
    {
      crypto::Hash hash = transfer.transactionHash;
      TransactionInformation info;
      if (container->getTransactionInformation(hash, info, nullptr, nullptr))
      {
        heights.push_back(info.blockHeight);
      }
    }
    uint64_t unlocked = calculateDepositsAmount(transfers, m_currency, heights);

    /* Now do the same thing for overall deposit balances */
    if (it->lockedDepositBalance < locked)
    {
      m_lockedDepositBalance += locked - it->lockedDepositBalance;
      updated = true;
    }
    else if (it->lockedDepositBalance > locked)
    {
      m_lockedDepositBalance -= it->lockedDepositBalance - locked;
      updated = true;
    }

    if (it->unlockedDepositBalance < unlocked)
    {
      m_unlockedDepositBalance += unlocked - it->unlockedDepositBalance;
      updated = true;
    }
    else if (it->unlockedDepositBalance > unlocked)
    {
      m_unlockedDepositBalance -= it->unlockedDepositBalance - unlocked;
      updated = true;
    }

    /* Write any changes to the wallet balances to the container */
    if (updated)
    {
      m_walletsContainer.get<TransfersContainerIndex>().modify(it, [actual, pending, locked, unlocked](WalletRecord &wallet) {
        wallet.actualBalance = actual;
        wallet.pendingBalance = pending;
        wallet.lockedDepositBalance = locked;
        wallet.unlockedDepositBalance = unlocked;
      });

      /* Keep the logging to debugging */
      m_logger(DEBUGGING, BRIGHT_WHITE) << "Wallet balance updated, address "
                                        << m_currency.accountAddressAsString({it->spendPublicKey, m_viewPublicKey})
                                        << ", actual " << m_currency.formatAmount(it->actualBalance) << ", pending "
                                        << m_currency.formatAmount(it->pendingBalance);
      m_logger(DEBUGGING, BRIGHT_WHITE) << "Container balance updated, actual "
                                        << m_currency.formatAmount(m_actualBalance) << ", pending "
                                        << m_currency.formatAmount(m_pendingBalance) << ", locked deposits "
                                        << m_currency.formatAmount(m_lockedDepositBalance) << ",unlocked deposits "
                                        << m_currency.formatAmount(m_unlockedDepositBalance);
      m_observerManager.notify(&IWalletObserver::actualBalanceUpdated, actual);
      m_observerManager.notify(&IWalletObserver::pendingBalanceUpdated, pending);
      m_observerManager.notify(&IWalletObserver::actualDepositBalanceUpdated, locked);
      m_observerManager.notify(&IWalletObserver::pendingDepositBalanceUpdated, unlocked);
    }
  }

  const WalletRecord &WalletGreen::getWalletRecord(const PublicKey &key) const
  {
    auto it = m_walletsContainer.get<KeysIndex>().find(key);
    if (it == m_walletsContainer.get<KeysIndex>().end())
    {
      throw std::system_error(make_error_code(error::WALLET_NOT_FOUND));
    }

    return *it;
  }

  const WalletRecord &WalletGreen::getWalletRecord(const std::string &address) const
  {
    cn::AccountPublicAddress pubAddr = parseAddress(address);
    return getWalletRecord(pubAddr.spendPublicKey);
  }

  const WalletRecord &WalletGreen::getWalletRecord(cn::ITransfersContainer *container) const
  {
    auto it = m_walletsContainer.get<TransfersContainerIndex>().find(container);
    if (it == m_walletsContainer.get<TransfersContainerIndex>().end())
    {
      throw std::system_error(make_error_code(error::WALLET_NOT_FOUND));
    }

    return *it;
  }

  cn::AccountPublicAddress WalletGreen::parseAddress(const std::string &address) const
  {
    cn::AccountPublicAddress pubAddr;

    if (!m_currency.parseAccountAddressString(address, pubAddr))
    {
      throw std::system_error(make_error_code(error::BAD_ADDRESS));
    }

    return pubAddr;
  }

  void WalletGreen::throwIfStopped() const
  {
    if (m_stopped)
    {
      throw std::system_error(make_error_code(error::OPERATION_CANCELLED));
    }
  }

  void WalletGreen::throwIfTrackingMode() const
  {
    if (getTrackingMode() == WalletTrackingMode::TRACKING)
    {
      throw std::system_error(make_error_code(error::TRACKING_MODE));
    }
  }

  WalletGreen::WalletTrackingMode WalletGreen::getTrackingMode() const
  {
    if (m_walletsContainer.get<RandomAccessIndex>().empty())
    {
      return WalletTrackingMode::NO_ADDRESSES;
    }

    return m_walletsContainer.get<RandomAccessIndex>().begin()->spendSecretKey == NULL_SECRET_KEY ? WalletTrackingMode::TRACKING : WalletTrackingMode::NOT_TRACKING;
  }

  size_t WalletGreen::createFusionTransaction(
      uint64_t threshold, uint64_t mixin,
      const std::vector<std::string> &sourceAddresses,
      const std::string &destinationAddress)
  {

    size_t id = WALLET_INVALID_TRANSACTION_ID;
    tools::ScopeExit releaseContext([this] {
      m_dispatcher.yield();
    });

    platform_system::EventLock lk(m_readyEvent);

    throwIfNotInitialized();
    throwIfTrackingMode();
    throwIfStopped();

    validateSourceAddresses(sourceAddresses);
    validateChangeDestination(sourceAddresses, destinationAddress, true);

    const size_t MAX_FUSION_OUTPUT_COUNT = 8;

    uint64_t fusionTreshold = m_currency.defaultDustThreshold();

    if (threshold <= fusionTreshold)
    {
      throw std::system_error(make_error_code(error::THRESHOLD_TOO_LOW));
    }

    if (m_walletsContainer.get<RandomAccessIndex>().size() == 0)
    {
      throw std::system_error(make_error_code(error::MINIMUM_ONE_ADDRESS));
    }

    size_t estimatedFusionInputsCount = m_currency.getApproximateMaximumInputCount(m_currency.fusionTxMaxSize(), MAX_FUSION_OUTPUT_COUNT, mixin);
    if (estimatedFusionInputsCount < m_currency.fusionTxMinInputCount())
    {
      throw std::system_error(make_error_code(error::MIXIN_COUNT_TOO_BIG));
    }

    auto fusionInputs = pickRandomFusionInputs(sourceAddresses, threshold, m_currency.fusionTxMinInputCount(), estimatedFusionInputsCount);
    if (fusionInputs.size() < m_currency.fusionTxMinInputCount())
    {
      //nothing to optimize
      throw std::system_error(make_error_code(error::NOTHING_TO_OPTIMIZE));
      return WALLET_INVALID_TRANSACTION_ID;
    }

    std::vector<outs_for_amount> mixinResult;
    if (mixin != 0)
    {
      requestMixinOuts(fusionInputs, mixin, mixinResult);
    }

    std::vector<InputInfo> keysInfo;
    prepareInputs(fusionInputs, mixinResult, mixin, keysInfo);

    AccountPublicAddress destination = getChangeDestination(destinationAddress, sourceAddresses);

    std::unique_ptr<ITransaction> fusionTransaction;
    size_t transactionSize;
    int round = 0;
    do
    {
      if (round != 0)
      {
        fusionInputs.pop_back();
        keysInfo.pop_back();
      }

      uint64_t inputsAmount = std::accumulate(fusionInputs.begin(), fusionInputs.end(), static_cast<uint64_t>(0), [](uint64_t amount, const OutputToTransfer &input) {
        return amount + input.out.amount;
      });

      ReceiverAmounts decomposedOutputs = decomposeFusionOutputs(destination, inputsAmount);
      assert(decomposedOutputs.amounts.size() <= MAX_FUSION_OUTPUT_COUNT);

      crypto::SecretKey txkey;
      std::vector<WalletMessage> messages;
      fusionTransaction = makeTransaction(std::vector<ReceiverAmounts>{decomposedOutputs}, keysInfo, messages, "", 0, txkey);
      transactionSize = getTransactionSize(*fusionTransaction);

      ++round;
    } while (transactionSize > m_currency.fusionTxMaxSize() && fusionInputs.size() >= m_currency.fusionTxMinInputCount());

    if (fusionInputs.size() < m_currency.fusionTxMinInputCount())
    {
      throw std::system_error(make_error_code(error::MINIMUM_INPUT_COUNT));
    }
    if (fusionTransaction->getOutputCount() == 0)
    {
      throw std::system_error(make_error_code(error::WRONG_AMOUNT));
    }
    id = validateSaveAndSendTransaction(*fusionTransaction, {}, true, true);
    return id;
  }

  WalletGreen::ReceiverAmounts WalletGreen::decomposeFusionOutputs(const AccountPublicAddress &address, uint64_t inputsAmount) const
  {
    WalletGreen::ReceiverAmounts outputs;
    outputs.receiver = address;
    if (inputsAmount > m_currency.minimumFeeV2())
    {
      decomposeAmount(inputsAmount - m_currency.minimumFeeV2(), 0, outputs.amounts);
      std::sort(outputs.amounts.begin(), outputs.amounts.end());
    }

    return outputs;
  }

  bool WalletGreen::isFusionTransaction(size_t transactionId) const
  {
    throwIfNotInitialized();
    throwIfStopped();

    if (m_transactions.size() <= transactionId)
    {
      throw std::system_error(make_error_code(cn::error::INDEX_OUT_OF_RANGE));
    }

    auto isFusionIter = m_fusionTxsCache.find(transactionId);
    if (isFusionIter != m_fusionTxsCache.end())
    {
      return isFusionIter->second;
    }

    bool result = isFusionTransaction(m_transactions.get<RandomAccessIndex>()[transactionId]);
    m_fusionTxsCache.emplace(transactionId, result);
    return result;
  }

  bool WalletGreen::isFusionTransaction(const WalletTransaction &walletTx) const
  {
    if (walletTx.fee != 0)
    {
      return false;
    }

    uint64_t inputsSum = 0;
    uint64_t outputsSum = 0;
    std::vector<uint64_t> outputsAmounts;
    std::vector<uint64_t> inputsAmounts;
    TransactionInformation txInfo;
    bool gotTx = false;
    const auto &walletsIndex = m_walletsContainer.get<RandomAccessIndex>();
    for (const WalletRecord &wallet : walletsIndex)
    {
      for (const TransactionOutputInformation &output : wallet.container->getTransactionOutputs(walletTx.hash, ITransfersContainer::IncludeTypeKey | ITransfersContainer::IncludeStateAll))
      {
        if (outputsAmounts.size() <= output.outputInTransaction)
        {
          outputsAmounts.resize(output.outputInTransaction + 1, 0);
        }

        assert(output.amount != 0);
        assert(outputsAmounts[output.outputInTransaction] == 0);
        outputsAmounts[output.outputInTransaction] = output.amount;
        outputsSum += output.amount;
      }

      for (const TransactionOutputInformation &input : wallet.container->getTransactionInputs(walletTx.hash, ITransfersContainer::IncludeTypeKey))
      {
        inputsSum += input.amount;
        inputsAmounts.push_back(input.amount);
      }

      if (!gotTx)
      {
        gotTx = wallet.container->getTransactionInformation(walletTx.hash, txInfo);
      }
    }

    if (!gotTx)
    {
      return false;
    }

    if (outputsSum != inputsSum || outputsSum != txInfo.totalAmountOut || inputsSum != txInfo.totalAmountIn)
    {
      return false;
    }
    else
    {
      return m_currency.isFusionTransaction(inputsAmounts, outputsAmounts, 0); //size = 0 here because can't get real size of tx in wallet.
    }
  }

  size_t WalletGreen::createOptimizationTransaction(const std::string &address)
  {
    if (getUnspentOutputsCount() < 100)
    {
      throw std::system_error(make_error_code(error::NOTHING_TO_OPTIMIZE));
      return WALLET_INVALID_TRANSACTION_ID;
    }

    uint64_t balance = getActualBalance(address);
    uint64_t threshold = 100;
    bool fusionReady = false;
    while (threshold <= balance && !fusionReady)
    {
      EstimateResult estimation = estimate(threshold, {address});
      if (estimation.fusionReadyCount > 50)
      {
        fusionReady = true;
        break;
      }
      threshold *= 10;
    }
    if (fusionReady)
    {
      return createFusionTransaction(threshold, cn::parameters::MINIMUM_MIXIN, {address}, address);
    }
    throw std::system_error(make_error_code(error::NOTHING_TO_OPTIMIZE));
    return WALLET_INVALID_TRANSACTION_ID;
  }

  void WalletGreen::validateChangeDestination(const std::vector<std::string> &sourceAddresses, const std::string &changeDestination, bool isFusion) const
  {
    std::string message;
    if (changeDestination.empty())
    {
      if (sourceAddresses.size() > 1 || (sourceAddresses.empty() && m_walletsContainer.size() > 1))
      {
        message = std::string(isFusion ? "Destination" : "Change destination") + " address is necessary";
        m_logger(ERROR, BRIGHT_RED) << message << ". Source addresses size=" << sourceAddresses.size() << ", wallets count=" << m_walletsContainer.size();
        throw std::system_error(make_error_code(isFusion ? error::DESTINATION_ADDRESS_REQUIRED : error::CHANGE_ADDRESS_REQUIRED), message);
      }
    }
    else
    {
      if (!cn::validateAddress(changeDestination, m_currency))
      {
        message = std::string("Bad ") + (isFusion ? "destination" : "change destination") + " address: " + changeDestination;
        m_logger(ERROR, BRIGHT_RED) << message;
        throw std::system_error(make_error_code(cn::error::BAD_ADDRESS), message);
      }

      if (!isMyAddress(changeDestination))
      {
        message = std::string(isFusion ? "Destination" : "Change destination") + " address is not found in current container: " + changeDestination;
        m_logger(ERROR, BRIGHT_RED) << message;
        throw std::system_error(make_error_code(isFusion ? error::DESTINATION_ADDRESS_NOT_FOUND : error::CHANGE_ADDRESS_NOT_FOUND), message);
      }
    }
  }

  void WalletGreen::validateSourceAddresses(const std::vector<std::string> &sourceAddresses) const
  {
    validateAddresses(sourceAddresses);

    auto badAddr = std::find_if(sourceAddresses.begin(), sourceAddresses.end(), [this](const std::string &addr) {
      return !isMyAddress(addr);
    });

    if (badAddr != sourceAddresses.end())
    {
      throw std::system_error(make_error_code(error::BAD_ADDRESS), "Source address must belong to current container: " + *badAddr);
    }
  }

  IFusionManager::EstimateResult WalletGreen::estimate(uint64_t threshold, const std::vector<std::string> &sourceAddresses) const
  {
    platform_system::EventLock lk(m_readyEvent);

    throwIfNotInitialized();
    throwIfStopped();

    validateSourceAddresses(sourceAddresses);

    IFusionManager::EstimateResult result{0, 0};
    auto walletOuts = sourceAddresses.empty() ? pickWalletsWithMoney() : pickWallets(sourceAddresses);
    std::array<size_t, std::numeric_limits<uint64_t>::digits10 + 1> bucketSizes;
    bucketSizes.fill(0);
    for (const auto &wallet : walletOuts)
    {
      for (const auto &out : wallet.outs)
      {
        uint8_t powerOfTen = 0;
        if (m_currency.isAmountApplicableInFusionTransactionInput(out.amount, threshold, powerOfTen, m_node.getLastKnownBlockHeight()))
        {
          assert(powerOfTen < std::numeric_limits<uint64_t>::digits10 + 1);
          bucketSizes[powerOfTen]++;
        }
      }

      result.totalOutputCount += wallet.outs.size();
    }

    for (auto bucketSize : bucketSizes)
    {
      if (bucketSize >= m_currency.fusionTxMinInputCount())
      {
        result.fusionReadyCount += bucketSize;
      }
    }

    return result;
  }

  std::vector<WalletGreen::OutputToTransfer> WalletGreen::pickRandomFusionInputs(const std::vector<std::string> &addresses,
                                                                                 uint64_t threshold, size_t minInputCount, size_t maxInputCount) const
  {

    std::vector<WalletGreen::OutputToTransfer> allFusionReadyOuts;
    auto walletOuts = addresses.empty() ? pickWalletsWithMoney() : pickWallets(addresses);
    std::array<size_t, std::numeric_limits<uint64_t>::digits10 + 1> bucketSizes;
    bucketSizes.fill(0);
    for (auto &walletOut : walletOuts)
    {
      for (auto &out : walletOut.outs)
      {
        uint8_t powerOfTen = 0;
        if (m_currency.isAmountApplicableInFusionTransactionInput(out.amount, threshold, powerOfTen, m_node.getLastKnownBlockHeight()))
        {
          allFusionReadyOuts.push_back({std::move(out), walletOut.wallet});
          assert(powerOfTen < std::numeric_limits<uint64_t>::digits10 + 1);
          bucketSizes[powerOfTen]++;
        }
      }
    }

    //now, pick the bucket
    std::vector<uint8_t> bucketNumbers(bucketSizes.size());
    std::iota(bucketNumbers.begin(), bucketNumbers.end(), 0);
    std::shuffle(bucketNumbers.begin(), bucketNumbers.end(), std::default_random_engine{crypto::rand<std::default_random_engine::result_type>()});
    size_t bucketNumberIndex = 0;
    for (; bucketNumberIndex < bucketNumbers.size(); ++bucketNumberIndex)
    {
      if (bucketSizes[bucketNumbers[bucketNumberIndex]] >= minInputCount)
      {
        break;
      }
    }

    if (bucketNumberIndex == bucketNumbers.size())
    {
      return {};
    }

    size_t selectedBucket = bucketNumbers[bucketNumberIndex];
    assert(selectedBucket < std::numeric_limits<uint64_t>::digits10 + 1);
    assert(bucketSizes[selectedBucket] >= minInputCount);
    uint64_t lowerBound = 1;
    for (size_t i = 0; i < selectedBucket; ++i)
    {
      lowerBound *= 10;
    }

    uint64_t upperBound = selectedBucket == std::numeric_limits<uint64_t>::digits10 ? UINT64_MAX : lowerBound * 10;
    std::vector<WalletGreen::OutputToTransfer> selectedOuts;
    selectedOuts.reserve(bucketSizes[selectedBucket]);
    for (auto& output: allFusionReadyOuts)
    {
      if (output.out.amount >= lowerBound && output.out.amount < upperBound)
      {
        selectedOuts.push_back(std::move(output));
      }
    }

    assert(selectedOuts.size() >= minInputCount);

    auto outputsSortingFunction = [](const OutputToTransfer &l, const OutputToTransfer &r) { return l.out.amount < r.out.amount; };
    if (selectedOuts.size() <= maxInputCount)
    {
      std::sort(selectedOuts.begin(), selectedOuts.end(), outputsSortingFunction);
      return selectedOuts;
    }

    ShuffleGenerator<size_t, crypto::random_engine<size_t>> generator(selectedOuts.size());
    std::vector<WalletGreen::OutputToTransfer> trimmedSelectedOuts;
    trimmedSelectedOuts.reserve(maxInputCount);
    for (size_t i = 0; i < maxInputCount; ++i)
    {
      trimmedSelectedOuts.push_back(std::move(selectedOuts[generator()]));
    }

    std::sort(trimmedSelectedOuts.begin(), trimmedSelectedOuts.end(), outputsSortingFunction);
    return trimmedSelectedOuts;
  }

  std::vector<DepositsInBlockInfo> WalletGreen::getDepositsInBlocks(uint32_t blockIndex, size_t count) const
  {
    if (count == 0)
    {
      throw std::system_error(make_error_code(error::WRONG_PARAMETERS), "blocks count must be greater than zero");
    }

    std::vector<DepositsInBlockInfo> result;

    if (blockIndex >= m_blockchain.size())
    {
      return result;
    }

    auto &blockHeightIndex = m_deposits.get<BlockHeightIndex>();
    auto stopIndex = static_cast<uint32_t>(std::min(m_blockchain.size(), blockIndex + count));

    for (uint32_t height = blockIndex; height < stopIndex; ++height)
    {
      DepositsInBlockInfo info;
      info.blockHash = m_blockchain[height];

      auto lowerBound = blockHeightIndex.lower_bound(height);
      auto upperBound = blockHeightIndex.upper_bound(height);
      for (auto it = lowerBound; it != upperBound; ++it)
      {
        Deposit deposit;
        deposit = *it;
        info.deposits.emplace_back(std::move(deposit));
      }
      result.emplace_back(std::move(info));
    }

    return result;
  }

  std::vector<TransactionsInBlockInfo> WalletGreen::getTransactionsInBlocks(uint32_t blockIndex, size_t count) const
  {
    if (count == 0)
    {
      throw std::system_error(make_error_code(error::WRONG_PARAMETERS), "blocks count must be greater than zero");
    }

    std::vector<TransactionsInBlockInfo> result;

    if (blockIndex >= m_blockchain.size())
    {
      return result;
    }

    auto &blockHeightIndex = m_transactions.get<BlockHeightIndex>();
    auto stopIndex = static_cast<uint32_t>(std::min(m_blockchain.size(), blockIndex + count));

    for (uint32_t height = blockIndex; height < stopIndex; ++height)
    {
      TransactionsInBlockInfo info;
      info.blockHash = m_blockchain[height];

      auto lowerBound = blockHeightIndex.lower_bound(height);
      auto upperBound = blockHeightIndex.upper_bound(height);
      for (auto it = lowerBound; it != upperBound; ++it)
      {
        if (it->state != WalletTransactionState::SUCCEEDED)
        {
          continue;
        }

        WalletTransactionWithTransfers transaction;
        transaction.transaction = *it;
        transaction.transfers = getTransactionTransfers(*it);

        info.transactions.emplace_back(std::move(transaction));
      }

      result.emplace_back(std::move(info));
    }

    return result;
  }

  crypto::Hash WalletGreen::getBlockHashByIndex(uint32_t blockIndex) const
  {
    assert(blockIndex < m_blockchain.size());
    return m_blockchain.get<BlockHeightIndex>()[blockIndex];
  }

  std::vector<WalletTransfer> WalletGreen::getTransactionTransfers(const WalletTransaction &transaction) const
  {
    auto &transactionIdIndex = m_transactions.get<RandomAccessIndex>();

    auto it = transactionIdIndex.iterator_to(transaction);
    assert(it != transactionIdIndex.end());

    size_t transactionId = std::distance(transactionIdIndex.begin(), it);
    size_t transfersCount = getTransactionTransferCount(transactionId);

    std::vector<WalletTransfer> result;
    result.reserve(transfersCount);

    for (size_t transferId = 0; transferId < transfersCount; ++transferId)
    {
      result.push_back(getTransactionTransfer(transactionId, transferId));
    }

    return result;
  }

  void WalletGreen::filterOutTransactions(WalletTransactions &transactions, WalletTransfers &transfers, std::function<bool(const WalletTransaction &)> &&pred) const
  {
    size_t cancelledTransactions = 0;

    transactions.reserve(m_transactions.size());
    transfers.reserve(m_transfers.size());

    auto &index = m_transactions.get<RandomAccessIndex>();
    size_t transferIdx = 0;
    for (size_t i = 0; i < m_transactions.size(); ++i)
    {
      const WalletTransaction &transaction = index[i];

      if (pred(transaction))
      {
        ++cancelledTransactions;

        while (transferIdx < m_transfers.size() && m_transfers[transferIdx].first == i)
        {
          ++transferIdx;
        }
      }
      else
      {
        transactions.emplace_back(transaction);

        while (transferIdx < m_transfers.size() && m_transfers[transferIdx].first == i)
        {
          transfers.emplace_back(i - cancelledTransactions, m_transfers[transferIdx].second);
          ++transferIdx;
        }
      }
    }
  }

  ///pre: changeDestinationAddress belongs to current container
  ///pre: source address belongs to current container
  cn::AccountPublicAddress WalletGreen::getChangeDestination(const std::string &changeDestinationAddress, const std::vector<std::string> &sourceAddresses) const
  {
    if (!changeDestinationAddress.empty())
    {
      return parseAccountAddressString(changeDestinationAddress, m_currency);
    }

    if (m_walletsContainer.size() == 1)
    {
      return AccountPublicAddress{m_walletsContainer.get<RandomAccessIndex>()[0].spendPublicKey, m_viewPublicKey};
    }

    assert(sourceAddresses.size() == 1 && isMyAddress(sourceAddresses[0]));
    return parseAccountAddressString(sourceAddresses[0], m_currency);
  }

  bool WalletGreen::isMyAddress(const std::string &addressString) const
  {
    cn::AccountPublicAddress address = parseAccountAddressString(addressString, m_currency);
    return m_viewPublicKey == address.viewPublicKey && m_walletsContainer.get<KeysIndex>().count(address.spendPublicKey) != 0;
  }

  void WalletGreen::deleteContainerFromUnlockTransactionJobs(const ITransfersContainer *container)
  {
    for (auto it = m_unlockTransactionsJob.begin(); it != m_unlockTransactionsJob.end();)
    {
      if (it->container == container)
      {
        it = m_unlockTransactionsJob.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }

  std::vector<size_t> WalletGreen::deleteTransfersForAddress(const std::string &address, std::vector<size_t> &deletedTransactions)
  {
    assert(!address.empty());

    int64_t deletedInputs = 0;
    int64_t deletedOutputs = 0;

    int64_t unknownInputs = 0;

    bool transfersLeft = false;
    size_t firstTransactionTransfer = 0;

    std::vector<size_t> updatedTransactions;

    for (size_t i = 0; i < m_transfers.size(); ++i)
    {
      WalletTransfer &transfer = m_transfers[i].second;

      if (transfer.address == address)
      {
        if (transfer.amount >= 0)
        {
          deletedOutputs += transfer.amount;
        }
        else
        {
          deletedInputs += transfer.amount;
          transfer.address = "";
        }
      }
      else if (transfer.address.empty())
      {
        if (transfer.amount < 0)
        {
          unknownInputs += transfer.amount;
        }
      }
      else if (isMyAddress(transfer.address))
      {
        transfersLeft = true;
      }

      size_t transactionId = m_transfers[i].first;
      if ((i == m_transfers.size() - 1) || (transactionId != m_transfers[i + 1].first))
      {
        //the last transfer for current transaction

        size_t transfersBeforeMerge = m_transfers.size();
        if (deletedInputs != 0)
        {
          adjustTransfer(transactionId, firstTransactionTransfer, "", deletedInputs + unknownInputs);
        }

        assert(transfersBeforeMerge >= m_transfers.size());
        i -= transfersBeforeMerge - m_transfers.size();

        auto &randomIndex = m_transactions.get<RandomAccessIndex>();

        randomIndex.modify(std::next(randomIndex.begin(), transactionId), [transfersLeft, deletedInputs, deletedOutputs](WalletTransaction &transaction) {
          transaction.totalAmount -= deletedInputs + deletedOutputs;

          if (!transfersLeft)
          {
            transaction.state = WalletTransactionState::DELETED;
          }
        });

        if (!transfersLeft)
        {
          deletedTransactions.push_back(transactionId);
        }

        if (deletedInputs != 0 || deletedOutputs != 0)
        {
          updatedTransactions.push_back(transactionId);
        }

        //reset values for next transaction
        deletedInputs = 0;
        deletedOutputs = 0;
        unknownInputs = 0;
        transfersLeft = false;
        firstTransactionTransfer = i + 1;
      }
    }

    return updatedTransactions;
  }

  size_t WalletGreen::getTxSize(const TransactionParameters &sendingTransaction)
  {
    platform_system::EventLock lk(m_readyEvent);

    throwIfNotInitialized();
    throwIfTrackingMode();
    throwIfStopped();

    cn::AccountPublicAddress changeDestination = getChangeDestination(sendingTransaction.changeDestination, sendingTransaction.sourceAddresses);

    std::vector<WalletOuts> wallets;
    if (!sendingTransaction.sourceAddresses.empty())
    {
      wallets = pickWallets(sendingTransaction.sourceAddresses);
    }
    else
    {
      wallets = pickWalletsWithMoney();
    }

    PreparedTransaction preparedTransaction;
    crypto::SecretKey txSecretKey;
    prepareTransaction(
        std::move(wallets),
        sendingTransaction.destinations,
        sendingTransaction.messages,
        sendingTransaction.fee,
        sendingTransaction.mixIn,
        sendingTransaction.extra,
        sendingTransaction.unlockTimestamp,
        sendingTransaction.donation,
        changeDestination,
        preparedTransaction,
        txSecretKey);

    BinaryArray transactionData = preparedTransaction.transaction->getTransactionData();
    return transactionData.size();
  }

  void WalletGreen::deleteFromUncommitedTransactions(const std::vector<size_t> &deletedTransactions)
  {
    for (auto transactionId : deletedTransactions)
    {
      m_uncommitedTransactions.erase(transactionId);
    }
  }

  void WalletGreen::clearCacheAndShutdown()
  {
    if (m_walletsContainer.size() != 0)
    {
      m_synchronizer.unsubscribeConsumerNotifications(m_viewPublicKey, this);
    }

    stopBlockchainSynchronizer();
    m_blockchainSynchronizer.removeObserver(this);

    clearCaches(true, true);

    m_walletsContainer.clear();

    shutdown();
  }

  bool canInsertTransactionToIndex(const WalletTransaction &transaction)
  {
    return transaction.state == WalletTransactionState::SUCCEEDED && transaction.blockHeight != WALLET_UNCONFIRMED_TRANSACTION_HEIGHT &&
           transaction.totalAmount > 0 && !transaction.extra.empty();
  }

  void WalletGreen::pushToPaymentsIndex(const crypto::Hash &paymentId, size_t txId)
  {
    m_paymentIds[paymentId].push_back(txId);
  }

  void WalletGreen::buildPaymentIds()
  {
    size_t begin = 0;
    size_t end = getTransactionCount();
    std::vector<uint8_t> extra;
    for (auto txId = begin; txId != end; ++txId)
    {
      const WalletTransaction& tx = getTransaction(txId);
      PaymentId paymentId;
      extra.insert(extra.begin(), tx.extra.begin(), tx.extra.end());
      if (canInsertTransactionToIndex(tx) && getPaymentIdFromTxExtra(extra, paymentId))
      {
        pushToPaymentsIndex(paymentId, txId);
      }
      extra.clear();
    }
  }

  std::vector<PaymentIdTransactions> WalletGreen::getTransactionsByPaymentIds(const std::vector<crypto::Hash> &paymentIds)
  {
    buildPaymentIds();
    std::vector<PaymentIdTransactions> payments(paymentIds.size());
    auto payment = payments.begin();
    for (auto &key : paymentIds)
    {
      payment->paymentId = key;
      auto it = m_paymentIds.find(key);
      if (it != m_paymentIds.end())
      {
        std::transform(it->second.begin(), it->second.end(), std::back_inserter(payment->transactions),
                       [this](size_t txId)
                       {
                         return getTransaction(txId);
                       });
      }

      ++payment;
    }
    return payments;
  }

  cn::WalletEvent WalletGreen::makeTransactionUpdatedEvent(size_t id)
  {
    cn::WalletEvent event;
    event.type = cn::WalletEventType::TRANSACTION_UPDATED;
    event.transactionUpdated.transactionIndex = id;
    m_observerManager.notify(&IWalletObserver::transactionUpdated, id);
    return event;
  }

  cn::WalletEvent WalletGreen::makeTransactionCreatedEvent(size_t id)
  {
    cn::WalletEvent event;
    event.type = cn::WalletEventType::TRANSACTION_CREATED;
    event.transactionCreated.transactionIndex = id;
    m_observerManager.notify(&IWalletObserver::sendTransactionCompleted, id, std::error_code());
    return event;
  }

  cn::WalletEvent WalletGreen::makeMoneyUnlockedEvent()
  {
    cn::WalletEvent event;
    event.type = cn::WalletEventType::BALANCE_UNLOCKED;

    return event;
  }

  cn::WalletEvent WalletGreen::makeSyncProgressUpdatedEvent(uint32_t current, uint32_t total)
  {
    cn::WalletEvent event;
    event.type = cn::WalletEventType::SYNC_PROGRESS_UPDATED;
    event.synchronizationProgressUpdated.processedBlockCount = current;
    event.synchronizationProgressUpdated.totalBlockCount = total;
    m_observerManager.notify(&IWalletObserver::synchronizationProgressUpdated, current, total);
    return event;
  }

  cn::WalletEvent WalletGreen::makeSyncCompletedEvent()
  {
    cn::WalletEvent event;
    event.type = cn::WalletEventType::SYNC_COMPLETED;
    return event;
  }

} //namespace cn

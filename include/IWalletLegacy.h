// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <array>
#include <istream>
#include <limits>
#include <ostream>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>
#include <list>
#include <boost/optional.hpp>
#include "CryptoNote.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"
#include "ITransfersContainer.h"
#include "IWallet.h"

namespace CryptoNote {

typedef size_t TransactionId;
typedef size_t TransferId;
typedef size_t DepositId;

struct WalletLegacyTransfer {
  std::string address;
  int64_t amount;
};

const TransactionId WALLET_LEGACY_INVALID_TRANSACTION_ID    = std::numeric_limits<TransactionId>::max();
const TransferId WALLET_LEGACY_INVALID_TRANSFER_ID          = std::numeric_limits<TransferId>::max();
const DepositId WALLET_LEGACY_INVALID_DEPOSIT_ID            = std::numeric_limits<DepositId>::max();
const uint32_t WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT = std::numeric_limits<uint32_t>::max();

enum class WalletLegacyTransactionState : uint8_t {
  Active,    // --> {Deleted}
  Deleted,   // --> {Active}

  Sending,   // --> {Active, Cancelled, Failed}
  Cancelled, // --> {}
  Failed     // --> {}
};

struct TransactionMessage {
  std::string message;
  std::string address;
};

struct WalletLegacyTransaction {
  TransferId       firstTransferId;
  size_t           transferCount;
  DepositId        firstDepositId;
  size_t           depositCount;
  int64_t          totalAmount;
  uint64_t         fee;
  uint64_t         sentTime;
  uint64_t         unlockTime;
  Crypto::Hash     hash;
  boost::optional<Crypto::SecretKey> secretKey = CryptoNote::NULL_SECRET_KEY;

  bool             isCoinbase;
  uint32_t         blockHeight;
  uint64_t         timestamp;
  std::string      extra;
  WalletLegacyTransactionState state;
  std::vector<std::string> messages;
};

using PaymentId = Crypto::Hash;
struct Payments {
  PaymentId paymentId;
  std::vector<WalletLegacyTransaction> transactions;
};

static_assert(std::is_move_constructible<Payments>::value, "Payments is not move constructible");

class IWalletLegacyObserver {
public:
  virtual ~IWalletLegacyObserver() {}

  virtual void initCompleted(std::error_code result) {}
  virtual void saveCompleted(std::error_code result) {}
  virtual void synchronizationProgressUpdated(uint32_t current, uint32_t total) {}
  virtual void synchronizationCompleted(std::error_code result) {}
  virtual void actualBalanceUpdated(uint64_t actualBalance) {}
  virtual void pendingBalanceUpdated(uint64_t pendingBalance) {}
  virtual void actualDepositBalanceUpdated(uint64_t actualDepositBalance) {}
  virtual void pendingDepositBalanceUpdated(uint64_t pendingDepositBalance) {}
  virtual void actualInvestmentBalanceUpdated(uint64_t actualInvestmentBalance) {}
  virtual void pendingInvestmentBalanceUpdated(uint64_t pendingInvestmentBalance) {}  
  virtual void externalTransactionCreated(TransactionId transactionId) {}
  virtual void sendTransactionCompleted(TransactionId transactionId, std::error_code result) {}
  virtual void transactionUpdated(TransactionId transactionId) {}
  virtual void depositsUpdated(const std::vector<DepositId>& depositIds) {}
};

class IWalletLegacy {
public:
  virtual ~IWalletLegacy() {} ;
  virtual void addObserver(IWalletLegacyObserver* observer) = 0;
  virtual void removeObserver(IWalletLegacyObserver* observer) = 0;

  virtual void initAndGenerate(const std::string& password) = 0;
  virtual void initAndLoad(std::istream& source, const std::string& password) = 0;
  virtual void initWithKeys(const AccountKeys& accountKeys, const std::string& password) = 0;
  virtual void shutdown() = 0;
  virtual void reset() = 0;
  virtual bool checkWalletPassword(std::istream& source, const std::string& password) = 0;

  virtual void save(std::ostream& destination, bool saveDetailed = true, bool saveCache = true) = 0;

  virtual std::error_code changePassword(const std::string& oldPassword, const std::string& newPassword) = 0;

  virtual std::string getAddress() = 0;

  virtual uint64_t actualBalance() = 0;
  virtual uint64_t dustBalance() = 0;

  virtual uint64_t pendingBalance() = 0;
  virtual uint64_t actualDepositBalance() = 0;
  virtual uint64_t actualInvestmentBalance() = 0;  
  virtual uint64_t pendingDepositBalance() = 0;
  virtual uint64_t pendingInvestmentBalance() = 0;  

  virtual size_t getTransactionCount() = 0;
  virtual size_t getTransferCount() = 0;
  virtual size_t getDepositCount() = 0;
  virtual size_t getNumUnlockedOutputs() = 0;
  virtual std::vector<TransactionOutputInformation> getUnspentOutputs() = 0;

  virtual TransactionId findTransactionByTransferId(TransferId transferId) = 0;
  virtual void getAccountKeys(AccountKeys& keys) = 0;
  virtual bool getTransaction(TransactionId transactionId, WalletLegacyTransaction& transaction) = 0;
  virtual bool getTransfer(TransferId transferId, WalletLegacyTransfer& transfer) = 0;
  virtual bool getDeposit(DepositId depositId, Deposit& deposit) = 0;
  virtual std::vector<Payments> getTransactionsByPaymentIds(const std::vector<PaymentId>& paymentIds) const = 0;
  virtual bool getTxProof(Crypto::Hash& txid, CryptoNote::AccountPublicAddress& address, Crypto::SecretKey& tx_key, std::string& sig_str) = 0;
  virtual std::string getReserveProof(const uint64_t &reserve, const std::string &message) = 0;
  virtual Crypto::SecretKey getTxKey(Crypto::Hash& txid) = 0;
  virtual bool get_tx_key(Crypto::Hash& txid, Crypto::SecretKey& txSecretKey) = 0;
  virtual TransactionId sendTransaction(Crypto::SecretKey& transactionSK, const WalletLegacyTransfer& transfer, uint64_t fee, const std::string& extra = "", uint64_t mixIn = 0, uint64_t unlockTimestamp = 0, const std::vector<TransactionMessage>& messages = std::vector<TransactionMessage>(), uint64_t ttl = 0) = 0;
  virtual TransactionId sendTransaction(Crypto::SecretKey& transactionSK, std::vector<WalletLegacyTransfer>& transfers, uint64_t fee, const std::string& extra = "", uint64_t mixIn = 0, uint64_t unlockTimestamp = 0, const std::vector<TransactionMessage>& messages = std::vector<TransactionMessage>(), uint64_t ttl = 0) = 0;
  virtual size_t estimateFusion(const uint64_t& threshold) = 0;
  virtual std::list<TransactionOutputInformation> selectFusionTransfersToSend(uint64_t threshold, size_t minInputCount, size_t maxInputCount) = 0;
  virtual TransactionId sendFusionTransaction(const std::list<TransactionOutputInformation>& fusionInputs, uint64_t fee, const std::string& extra = "", uint64_t mixIn = 0, uint64_t unlockTimestamp = 0) = 0;
  virtual TransactionId deposit(uint32_t term, uint64_t amount, uint64_t fee, uint64_t mixIn = 0) = 0;
  virtual TransactionId withdrawDeposits(const std::vector<DepositId>& depositIds, uint64_t fee) = 0;
  virtual std::error_code cancelTransaction(size_t transferId) = 0;

};

}

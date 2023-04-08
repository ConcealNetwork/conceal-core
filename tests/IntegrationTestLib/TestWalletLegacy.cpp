// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "TestWalletLegacy.h"
#include <thread>

namespace Tests
{
namespace common
{

using namespace cn;
using namespace crypto;

const std::string TEST_PASSWORD = "password";

TestWalletLegacy::TestWalletLegacy(platform_system::Dispatcher &dispatcher, const Currency &currency, INode &node) : m_dispatcher(dispatcher),
                                                                                                            m_synchronizationCompleted(dispatcher),
                                                                                                            m_someTransactionUpdated(dispatcher),
                                                                                                            m_currency(currency),
                                                                                                            m_node(node),
                                                                                                            m_wallet(new cn::WalletLegacy(currency, node, m_logger, true)),
                                                                                                            m_currentHeight(0)
{
  m_wallet->addObserver(this);
}

TestWalletLegacy::~TestWalletLegacy()
{
  m_wallet->removeObserver(this);
  // Make sure all remote spawns are executed
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  m_dispatcher.yield();
}

std::error_code TestWalletLegacy::init()
{
  cn::AccountBase walletAccount;
  walletAccount.generate();

  m_wallet->initWithKeys(walletAccount.getAccountKeys(), TEST_PASSWORD);
  m_synchronizationCompleted.wait();
  return m_lastSynchronizationResult;
}

namespace
{
struct TransactionSendingWaiter : public IWalletLegacyObserver
{
  platform_system::Dispatcher &m_dispatcher;
  platform_system::Event m_event;
  bool m_waiting = false;
  TransactionId m_expectedTxId;
  std::error_code m_result;

  TransactionSendingWaiter(platform_system::Dispatcher &dispatcher) : m_dispatcher(dispatcher), m_event(dispatcher)
  {
  }

  void wait(TransactionId expectedTxId)
  {
    m_waiting = true;
    m_expectedTxId = expectedTxId;
    m_event.wait();
    m_waiting = false;
  }

  virtual void sendTransactionCompleted(TransactionId transactionId, std::error_code result) override
  {
    m_dispatcher.remoteSpawn([this, transactionId, result]() {
      if (m_waiting && m_expectedTxId == transactionId)
      {
        m_result = result;
        m_event.set();
      }
    });
  }
};
} // namespace

std::error_code TestWalletLegacy::sendTransaction(const std::string &address, uint64_t amount, Hash &txHash)
{
  TransactionSendingWaiter transactionSendingWaiter(m_dispatcher);
  m_wallet->addObserver(&transactionSendingWaiter);

  WalletLegacyTransfer transfer{address, static_cast<int64_t>(amount)};
  std::vector<cn::TransactionMessage> messages;
  std::string extraString;
  uint64_t fee = cn::parameters::MINIMUM_FEE_V2;
  uint64_t mixIn = 0;
  uint64_t unlockTimestamp = 0;
  uint64_t ttl = 0;
  crypto::SecretKey transactionSK;
  auto txId = m_wallet->sendTransaction(transactionSK, transfer, fee, extraString, mixIn, unlockTimestamp, messages, ttl);

  transactionSendingWaiter.wait(txId);
  m_wallet->removeObserver(&transactionSendingWaiter);
  // TODO workaround: make sure ObserverManager doesn't have local pointers to transactionSendingWaiter, so it can be destroyed
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  // Run all spawned handlers from TransactionSendingWaiter::sendTransactionCompleted
  m_dispatcher.yield();

  WalletLegacyTransaction txInfo;
  if (!m_wallet->getTransaction(txId, txInfo))
  {
    return std::make_error_code(std::errc::identifier_removed);
  }

  txHash = txInfo.hash;
  return transactionSendingWaiter.m_result;
}

void TestWalletLegacy::waitForSynchronizationToHeight(uint32_t height)
{
  while (m_synchronizedHeight < height)
  {
    m_synchronizationCompleted.wait();
  }
}

IWalletLegacy *TestWalletLegacy::wallet()
{
  return m_wallet.get();
}

AccountPublicAddress TestWalletLegacy::address() const
{
  std::string addressString = m_wallet->getAddress();
  AccountPublicAddress address;
  bool ok = m_currency.parseAccountAddressString(addressString, address);
  assert(ok);
  return address;
}

void TestWalletLegacy::synchronizationCompleted(std::error_code result)
{
  m_dispatcher.remoteSpawn([this, result]() {
    m_lastSynchronizationResult = result;
    m_synchronizedHeight = m_currentHeight;
    m_synchronizationCompleted.set();
    m_synchronizationCompleted.clear();
  });
}

void TestWalletLegacy::synchronizationProgressUpdated(uint32_t current, uint32_t total)
{
  m_dispatcher.remoteSpawn([this, current]() {
    m_currentHeight = current;
  });
}

} // namespace common
} // namespace Tests

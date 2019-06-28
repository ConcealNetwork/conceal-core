// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "WalletRpcServer.h"

#include <fstream>
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
#include "Common/Util.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "Common/CommandLine.h"
#include "Common/StringTools.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/Account.h"
#include "crypto/hash.h"
#include "WalletLegacy/WalletHelper.h"
// #include "wallet_errors.h"

#include "Rpc/JsonRpc.h"

using namespace Logging;
using namespace CryptoNote;

namespace Tools {

const command_line::arg_descriptor<uint16_t> wallet_rpc_server::arg_rpc_bind_port = { "rpc-bind-port", "Starts wallet as rpc server for wallet operations, sets bind port for server", 0, true };
const command_line::arg_descriptor<std::string> wallet_rpc_server::arg_rpc_bind_ip = { "rpc-bind-ip", "Specify ip to bind rpc server", "127.0.0.1" };
const command_line::arg_descriptor<std::string> wallet_rpc_server::arg_rpc_user = { "rpc-user", "Username to use the rpc server. If authorization is not required, leave it empty", "" };
const command_line::arg_descriptor<std::string> wallet_rpc_server::arg_rpc_password = { "rpc-password", "Password to use the rpc server. If authorization is not required, leave it empty", "" };

void wallet_rpc_server::init_options(boost::program_options::options_description& desc) {
  command_line::add_arg(desc, arg_rpc_bind_ip);
  command_line::add_arg(desc, arg_rpc_bind_port);
  command_line::add_arg(desc, arg_rpc_user);
  command_line::add_arg(desc, arg_rpc_password);
}
//------------------------------------------------------------------------------------------------------------------------------
wallet_rpc_server::wallet_rpc_server(
  System::Dispatcher& dispatcher,
  Logging::ILogger& log,
  CryptoNote::IWalletLegacy&w,
  CryptoNote::INode& n,
  CryptoNote::Currency& currency,
  const std::string& walletFile)
  :
  HttpServer(dispatcher, log),
  logger(log, "WalletRpc"),
  m_dispatcher(dispatcher),
  m_stopComplete(dispatcher),
  m_wallet(w),
  m_node(n),
  m_currency(currency),
  m_walletFilename(walletFile) {
}
//------------------------------------------------------------------------------------------------------------------------------
bool wallet_rpc_server::run() {
  start(m_bind_ip, m_port, m_rpcUser, m_rpcPassword);
  m_stopComplete.wait();
  return true;
}

void wallet_rpc_server::send_stop_signal() {
  m_dispatcher.remoteSpawn([this] {
    std::cout << "wallet_rpc_server::send_stop_signal()" << std::endl;
    stop();
    m_stopComplete.set();
  });
}

//------------------------------------------------------------------------------------------------------------------------------
bool wallet_rpc_server::handle_command_line(const boost::program_options::variables_map& vm) {
  m_bind_ip = command_line::get_arg(vm, arg_rpc_bind_ip);
  m_port = command_line::get_arg(vm, arg_rpc_bind_port);
  m_rpcUser = command_line::get_arg(vm, arg_rpc_user);
  m_rpcPassword = command_line::get_arg(vm, arg_rpc_password);
  return true;
}
//------------------------------------------------------------------------------------------------------------------------------
bool wallet_rpc_server::init(const boost::program_options::variables_map& vm) {
  if (!handle_command_line(vm)) {
    logger(ERROR) << "Failed to process command line in wallet_rpc_server";
    return false;
  }

  return true;
}

void wallet_rpc_server::processRequest(const CryptoNote::HttpRequest& request, CryptoNote::HttpResponse& response) {

  using namespace CryptoNote::JsonRpc;

  JsonRpcRequest jsonRequest;
  JsonRpcResponse jsonResponse;

  try {
    jsonRequest.parseRequest(request.getBody());
    jsonResponse.setId(jsonRequest.getId());

    static std::unordered_map<std::string, JsonMemberMethod> s_methods = {
      { "create_integrated", makeMemberMethod(&wallet_rpc_server::on_create_integrated) },  
      { "getbalance", makeMemberMethod(&wallet_rpc_server::on_getbalance) },
      { "transfer", makeMemberMethod(&wallet_rpc_server::on_transfer) },
      { "store", makeMemberMethod(&wallet_rpc_server::on_store) },
      { "get_messages", makeMemberMethod(&wallet_rpc_server::on_get_messages) },
      { "get_payments", makeMemberMethod(&wallet_rpc_server::on_get_payments) },
      { "get_transfers", makeMemberMethod(&wallet_rpc_server::on_get_transfers) },
      { "get_height", makeMemberMethod(&wallet_rpc_server::on_get_height) },
      { "get_outputs", makeMemberMethod(&wallet_rpc_server::on_get_outputs) },
      { "get_tx_proof"     , makeMemberMethod(&wallet_rpc_server::on_get_tx_proof)      },
      { "get_reserve_proof", makeMemberMethod(&wallet_rpc_server::on_get_reserve_proof) },      
      { "optimize", makeMemberMethod(&wallet_rpc_server::on_optimize) },
      { "estimate_fusion"  , makeMemberMethod(&wallet_rpc_server::on_estimate_fusion) },
      { "send_fusion"      , makeMemberMethod(&wallet_rpc_server::on_send_fusion) },
      { "reset", makeMemberMethod(&wallet_rpc_server::on_reset) }
    };

    auto it = s_methods.find(jsonRequest.getMethod());
    if (it == s_methods.end()) {
      throw JsonRpcError(errMethodNotFound);
    }

    it->second(this, jsonRequest, jsonResponse);

  } catch (const JsonRpcError& err) {
    jsonResponse.setError(err);
  } catch (const std::exception& e) {
    jsonResponse.setError(JsonRpcError(WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR, e.what()));
  }

  response.setBody(jsonResponse.getBody());
}

//------------------------------------------------------------------------------------------------------------------------------
bool wallet_rpc_server::on_getbalance(const wallet_rpc::COMMAND_RPC_GET_BALANCE::request& req, wallet_rpc::COMMAND_RPC_GET_BALANCE::response& res) {
  res.locked_amount = m_wallet.pendingBalance();
  res.available_balance = m_wallet.actualBalance();
  res.balance = res.locked_amount + res.available_balance;
  res.unlocked_balance = res.available_balance;
  return true;
}
//------------------------------------------------------------------------------------------------------------------------------
bool wallet_rpc_server::on_transfer(const wallet_rpc::COMMAND_RPC_TRANSFER::request& req, wallet_rpc::COMMAND_RPC_TRANSFER::response& res) {
  std::vector<CryptoNote::WalletLegacyTransfer> transfers;
  std::vector<CryptoNote::TransactionMessage> messages;
  for (auto it = req.destinations.begin(); it != req.destinations.end(); it++) {
    CryptoNote::WalletLegacyTransfer transfer;
    transfer.address = it->address;
    transfer.amount = it->amount;
    transfers.push_back(transfer);

    if (!it->message.empty()) {
      messages.emplace_back(CryptoNote::TransactionMessage{ it->message, it->address });
    }
  }

  std::vector<uint8_t> extra;
  if (!req.payment_id.empty()) {
    std::string payment_id_str = req.payment_id;

    Crypto::Hash payment_id;
    if (!CryptoNote::parsePaymentId(payment_id_str, payment_id)) {
      throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_WRONG_PAYMENT_ID,
        "Payment id has invalid format: \"" + payment_id_str + "\", expected 64-character string");
    }

    BinaryArray extra_nonce;
    CryptoNote::setPaymentIdToTransactionExtraNonce(extra_nonce, payment_id);
    if (!CryptoNote::addExtraNonceToTransactionExtra(extra, extra_nonce)) {
      throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_WRONG_PAYMENT_ID,
        "Something went wrong with payment_id. Please check its format: \"" + payment_id_str + "\", expected 64-character string");
    }
  }

  for (auto& rpc_message : req.messages) {
     messages.emplace_back(CryptoNote::TransactionMessage{ rpc_message.message, rpc_message.address });
  }

  uint64_t ttl = 0;
  if (req.ttl != 0) {
    ttl = static_cast<uint64_t>(time(nullptr)) + req.ttl;
  }

  uint64_t actualFee = 100;
  if (req.fee >= 100) {
    actualFee = req.fee;
  }

  std::string extraString;
  std::copy(extra.begin(), extra.end(), std::back_inserter(extraString));
  try {
    CryptoNote::WalletHelper::SendCompleteResultObserver sent;
    WalletHelper::IWalletRemoveObserverGuard removeGuard(m_wallet, sent);

    Crypto::SecretKey transactionSK;
    CryptoNote::TransactionId tx = m_wallet.sendTransaction(transactionSK, transfers, actualFee, extraString, req.mixin, req.unlock_time, messages, ttl);
    if (tx == WALLET_LEGACY_INVALID_TRANSACTION_ID) {
      throw std::runtime_error("Couldn't send transaction");
    }

    std::error_code sendError = sent.wait(tx);
    removeGuard.removeObserver();

    if (sendError) {
      throw std::system_error(sendError);
    }

    CryptoNote::WalletLegacyTransaction txInfo;
    m_wallet.getTransaction(tx, txInfo);
    res.tx_hash = Common::podToHex(txInfo.hash);
    res.tx_secret_key = Common::podToHex(transactionSK);

  } catch (const std::exception& e) {
    throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_GENERIC_TRANSFER_ERROR, e.what());
  }
  return true;
}
//------------------------------------------------------------------------------------------------------------------------------

bool wallet_rpc_server::on_get_tx_proof(const wallet_rpc::COMMAND_RPC_GET_TX_PROOF::request& req,
	wallet_rpc::COMMAND_RPC_GET_TX_PROOF::response& res) {
	Crypto::Hash txid;
	if (!parse_hash256(req.tx_hash, txid)) {
		throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR, std::string("Failed to parse tx_hash"));
	}
	CryptoNote::AccountPublicAddress dest_address;
	if (!m_currency.parseAccountAddressString(req.dest_address, dest_address)) {
		throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_WRONG_ADDRESS, std::string("Failed to parse address"));
	}

	Crypto::SecretKey tx_key, tx_key2;
	bool r = m_wallet.get_tx_key(txid, tx_key);

	if (!req.tx_key.empty()) {
		Crypto::Hash tx_key_hash;
		size_t size;
		if (!Common::fromHex(req.tx_key, &tx_key_hash, sizeof(tx_key_hash), size) || size != sizeof(tx_key_hash)) {
			throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR, std::string("Failed to parse tx_key"));
		}
		tx_key2 = *(struct Crypto::SecretKey *) &tx_key_hash;

		if (r) {
			if (tx_key != tx_key2) {
				throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR, 
					std::string("Tx secret key was found for the given txid, but you've also provided another tx secret key which doesn't match the found one."));
			}
		}
		tx_key = tx_key2;
	}
	else {
		if (!r) {
			throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR,
				std::string("Tx secret key wasn't found in the wallet file. Provide it as the optional <tx_key> parameter if you have it elsewhere."));
		}
	}
	
	std::string sig_str;
	if (m_wallet.getTxProof(txid, dest_address, tx_key, sig_str)) {
		res.signature = sig_str;
	}
	else {
		throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR, std::string("Failed to get transaction proof"));
	}

	return true;
}

bool wallet_rpc_server::on_get_reserve_proof(const wallet_rpc::COMMAND_RPC_GET_BALANCE_PROOF::request& req,
	wallet_rpc::COMMAND_RPC_GET_BALANCE_PROOF::response& res) {

	try {
		res.signature = m_wallet.getReserveProof(req.amount != 0 ? req.amount : m_wallet.actualBalance(), !req.message.empty() ? req.message : "");
	}
	catch (const std::exception &e) {
		throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR, e.what());
	}

	return true;
}

bool wallet_rpc_server::on_optimize(const wallet_rpc::COMMAND_RPC_OPTIMIZE::request& req, wallet_rpc::COMMAND_RPC_OPTIMIZE::response& res) {
  std::vector<CryptoNote::WalletLegacyTransfer> transfers;
  std::vector<CryptoNote::TransactionMessage> messages;
  std::string extraString;
  uint64_t fee = CryptoNote::parameters::MINIMUM_FEE_V1;
  uint64_t mixIn = 0;
  uint64_t unlockTimestamp = 0;
  uint64_t ttl = 0;

  try {
    CryptoNote::WalletHelper::SendCompleteResultObserver sent;
    WalletHelper::IWalletRemoveObserverGuard removeGuard(m_wallet, sent);

    Crypto::SecretKey transactionSK;
    CryptoNote::TransactionId tx = m_wallet.sendTransaction(transactionSK, transfers, fee, extraString, mixIn, unlockTimestamp, messages, ttl);
    if (tx == WALLET_LEGACY_INVALID_TRANSACTION_ID) {
      throw std::runtime_error("Couldn't send transaction");
    }

    std::error_code sendError = sent.wait(tx);
    removeGuard.removeObserver();

    if (sendError) {
      throw std::system_error(sendError);
    }

    CryptoNote::WalletLegacyTransaction txInfo;
    m_wallet.getTransaction(tx, txInfo);
    res.tx_hash = Common::podToHex(txInfo.hash);
    res.tx_secret_key = Common::podToHex(transactionSK);

  } catch (const std::exception& e) {
    throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_GENERIC_TRANSFER_ERROR, e.what());
  }
  return true;
}
//------------------------------------------------------------------------------------------------------------------------------
bool wallet_rpc_server::on_estimate_fusion(const wallet_rpc::COMMAND_RPC_ESTIMATE_FUSION::request& req, wallet_rpc::COMMAND_RPC_ESTIMATE_FUSION::response& res)
{
  if (req.threshold <= m_currency.defaultDustThreshold()) {
    throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR, std::string("Fusion transaction threshold is too small. Threshold: " + 
      m_currency.formatAmount(req.threshold)) + ", minimum threshold " + m_currency.formatAmount(m_currency.defaultDustThreshold() + 1));
  }
  try {
    res.fusion_ready_count = m_wallet.estimateFusion(req.threshold);
  }
  catch (std::exception &e) {
    throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR, std::string("Failed to estimate fusion ready count: ") + e.what());
  }
  return true;
}
//------------------------------------------------------------------------------------------------------------------------------
bool wallet_rpc_server::on_send_fusion(const wallet_rpc::COMMAND_RPC_SEND_FUSION::request& req, wallet_rpc::COMMAND_RPC_SEND_FUSION::response& res)
{
  const size_t MAX_FUSION_OUTPUT_COUNT = 4;

  if (req.threshold <= m_currency.defaultDustThreshold()) {
    throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR, std::string("Fusion transaction threshold is too small. Threshold: " +
      m_currency.formatAmount(req.threshold)) + ", minimum threshold " + m_currency.formatAmount(m_currency.defaultDustThreshold() + 1));
  }

  size_t estimatedFusionInputsCount = m_currency.getApproximateMaximumInputCount(m_currency.fusionTxMaxSize(), MAX_FUSION_OUTPUT_COUNT, req.mixin);
  if (estimatedFusionInputsCount < m_currency.fusionTxMinInputCount()) {
    throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR,
      std::string("Fusion transaction mixin is too big " + std::to_string(req.mixin)));
  }

  try {
    std::list<TransactionOutputInformation> fusionInputs = m_wallet.selectFusionTransfersToSend(req.threshold, m_currency.fusionTxMinInputCount(), estimatedFusionInputsCount);
    if (fusionInputs.size() < m_currency.fusionTxMinInputCount()) {
      //nothing to optimize
      throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR,
        std::string("Fusion transaction not created: nothing to optimize for threshold " + std::to_string(req.threshold)));
    }

    std::string extraString;
    CryptoNote::WalletHelper::SendCompleteResultObserver sent;
    WalletHelper::IWalletRemoveObserverGuard removeGuard(m_wallet, sent);

    CryptoNote::TransactionId tx = m_wallet.sendFusionTransaction(fusionInputs, 50, extraString, req.mixin, req.unlock_time);
    if (tx == WALLET_LEGACY_INVALID_TRANSACTION_ID)
      throw std::runtime_error("Couldn't send fusion transaction");

    std::error_code sendError = sent.wait(tx);
    removeGuard.removeObserver();

    if (sendError)
       throw std::system_error(sendError);

    CryptoNote::WalletLegacyTransaction txInfo;
    m_wallet.getTransaction(tx, txInfo);
    res.tx_hash = Common::podToHex(txInfo.hash);
  }
  catch (const std::exception& e)
  {
    throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_GENERIC_TRANSFER_ERROR, e.what());
  }
  return true;
}
//------------------------------------------------------------------------------------------------------------------------------
bool wallet_rpc_server::on_store(const wallet_rpc::COMMAND_RPC_STORE::request& req, wallet_rpc::COMMAND_RPC_STORE::response& res) {
  try {
    WalletHelper::storeWallet(m_wallet, m_walletFilename);
  } catch (std::exception& e) {
    throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR, std::string("Couldn't save wallet: ") + e.what());
  }

  return true;
}
//------------------------------------------------------------------------------------------------------------------------------
bool wallet_rpc_server::on_get_messages(const wallet_rpc::COMMAND_RPC_GET_MESSAGES::request& req, wallet_rpc::COMMAND_RPC_GET_MESSAGES::response& res) {
  res.total_tx_count = m_wallet.getTransactionCount();

  for (uint64_t i = req.first_tx_id; i < res.total_tx_count && res.tx_messages.size() < req.tx_limit; ++i) {
    WalletLegacyTransaction tx;
    if (!m_wallet.getTransaction(static_cast<TransactionId>(i), tx)) {
      throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR, "Failed to get transaction");
    }

    if (!tx.messages.empty()) {
      wallet_rpc::transaction_messages tx_messages;
      tx_messages.tx_hash = Common::podToHex(tx.hash);
      tx_messages.tx_id = i;
      tx_messages.block_height = tx.blockHeight;
      tx_messages.timestamp = tx.timestamp;
      std::copy(tx.messages.begin(), tx.messages.end(), std::back_inserter(tx_messages.messages));

      // replace newlines in messages with \n to avoid JSON error
      for (auto it = tx_messages.messages.begin(); it != tx_messages.messages.end(); it++) {
        for (std::string::size_type n = 0; ( ( n = (*it).find("\n", n ) ) != std::string::npos ); ) {
          (*it).replace(n, 1, "\\n");
          n += 2;
        }
      }

      res.tx_messages.emplace_back(std::move(tx_messages));
    }
  }

  return true;
}
//------------------------------------------------------------------------------------------------------------------------------
bool wallet_rpc_server::on_get_payments(const wallet_rpc::COMMAND_RPC_GET_PAYMENTS::request& req, wallet_rpc::COMMAND_RPC_GET_PAYMENTS::response& res) {
  PaymentId expectedPaymentId;
  CryptoNote::BinaryArray payment_id_blob;

  if (!Common::fromHex(req.payment_id, payment_id_blob)) {
    throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_WRONG_PAYMENT_ID, "Payment ID has invald format");
  }

  if (sizeof(expectedPaymentId) != payment_id_blob.size()) {
    throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_WRONG_PAYMENT_ID, "Payment ID has invalid size");
  }

  std::copy(std::begin(payment_id_blob), std::end(payment_id_blob), reinterpret_cast<char*>(&expectedPaymentId)); // no UB, char can alias any type
  auto payments = m_wallet.getTransactionsByPaymentIds({expectedPaymentId});
  assert(payments.size() == 1);
  for (auto& transaction : payments[0].transactions) {
    wallet_rpc::payment_details rpc_payment;
    rpc_payment.tx_hash = Common::podToHex(transaction.hash);
    rpc_payment.amount = transaction.totalAmount;
    rpc_payment.block_height = transaction.blockHeight;
    rpc_payment.unlock_time = transaction.unlockTime;
    res.payments.push_back(rpc_payment);
  }

  return true;
}

/* ----------------------------------------------------------------------------------------------------------- */

/* CREATE INTEGRATED */
/* takes an address and payment ID and returns an integrated address */

bool wallet_rpc_server::on_create_integrated(const wallet_rpc::COMMAND_RPC_CREATE_INTEGRATED::request& req, wallet_rpc::COMMAND_RPC_CREATE_INTEGRATED::response& res) 
{

  if (!req.payment_id.empty() && !req.address.empty()) 
  {

    std::string payment_id_str = req.payment_id;
    std::string address_str = req.address;

    uint64_t prefix;
    CryptoNote::AccountPublicAddress addr;

    /* get the spend and view public keys from the address */
    const bool valid = CryptoNote::parseAccountAddressString(prefix, 
                                                            addr,
                                                            address_str);

    CryptoNote::BinaryArray ba;
    CryptoNote::toBinaryArray(addr, ba);
    std::string keys = Common::asString(ba);

    /* create the integrated address the same way you make a public address */
    std::string integratedAddress = Tools::Base58::encode_addr (
        CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
        payment_id_str + keys
    );

    res.integrated_address = integratedAddress;
  }
  return true;
}

/* --------------------------------------------------------------------------------- */

bool wallet_rpc_server::on_get_transfers(const wallet_rpc::COMMAND_RPC_GET_TRANSFERS::request& req, wallet_rpc::COMMAND_RPC_GET_TRANSFERS::response& res) {
  res.transfers.clear();
  size_t transactionsCount = m_wallet.getTransactionCount();
  for (size_t trantransactionNumber = 0; trantransactionNumber < transactionsCount; ++trantransactionNumber) {
    WalletLegacyTransaction txInfo;
    m_wallet.getTransaction(trantransactionNumber, txInfo);
    if (txInfo.state != WalletLegacyTransactionState::Active || txInfo.blockHeight == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT) {
      continue;
    }

    std::string address = "";
    if (txInfo.totalAmount < 0) {
      if (txInfo.transferCount > 0) {
        WalletLegacyTransfer tr;
        m_wallet.getTransfer(txInfo.firstTransferId, tr);
        address = tr.address;
      }
    }

    wallet_rpc::Transfer transfer;
    transfer.time = txInfo.timestamp;
    transfer.output = txInfo.totalAmount < 0;
    transfer.transactionHash = Common::podToHex(txInfo.hash);
    transfer.amount = std::abs(txInfo.totalAmount);
    transfer.fee = txInfo.fee;
    transfer.address = address;
    transfer.blockIndex = txInfo.blockHeight;
    transfer.unlockTime = txInfo.unlockTime;
    transfer.paymentId = "";

    std::vector<uint8_t> extraVec;
    extraVec.reserve(txInfo.extra.size());
    std::for_each(txInfo.extra.begin(), txInfo.extra.end(), [&extraVec](const char el) { extraVec.push_back(el); });

    Crypto::Hash paymentId;
    transfer.paymentId = (getPaymentIdFromTxExtra(extraVec, paymentId) && paymentId != NULL_HASH ? Common::podToHex(paymentId) : "");

    res.transfers.push_back(transfer);
  }

  return true;
}

bool wallet_rpc_server::on_get_height(const wallet_rpc::COMMAND_RPC_GET_HEIGHT::request& req, wallet_rpc::COMMAND_RPC_GET_HEIGHT::response& res) {
  res.height = m_node.getLastLocalBlockHeight();
  return true;
}

bool wallet_rpc_server::on_get_outputs(const wallet_rpc::COMMAND_RPC_GET_OUTPUTS::request& req, wallet_rpc::COMMAND_RPC_GET_OUTPUTS::response& res) {
  res.num_unlocked_outputs = m_wallet.getNumUnlockedOutputs();
  return true;
}

bool wallet_rpc_server::on_reset(const wallet_rpc::COMMAND_RPC_RESET::request& req, wallet_rpc::COMMAND_RPC_RESET::response& res) {
  m_wallet.reset();
  return true;
}

}

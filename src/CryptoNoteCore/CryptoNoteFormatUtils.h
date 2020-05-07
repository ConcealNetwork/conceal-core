// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <boost/utility/value_init.hpp>

#include "CryptoNoteBasic.h"
#include "CryptoNoteSerialization.h"

#include "Serialization/BinaryOutputStreamSerializer.h"
#include "Serialization/BinaryInputStreamSerializer.h"

namespace Logging {
class ILogger;
}

namespace CryptoNote {

bool parseAndValidateTransactionFromBinaryArray(const BinaryArray& transactionBinaryArray, Transaction& transaction, Crypto::Hash& transactionHash, Crypto::Hash& transactionPrefixHash);

struct TransactionSourceEntry {
  typedef std::pair<uint32_t, Crypto::PublicKey> OutputEntry;

  std::vector<OutputEntry> outputs;           //index + key
  uint64_t realOutput;                          //index in outputs vector of real output_entry
  Crypto::PublicKey realTransactionPublicKey; //incoming real tx public key
  uint64_t realOutputIndexInTransaction;        //index in transaction outputs vector
  uint64_t amount;                            //money
};

struct TransactionDestinationEntry {
  uint64_t amount;                    //money
  AccountPublicAddress addr;          //destination address

  TransactionDestinationEntry() : amount(0), addr(boost::value_initialized<AccountPublicAddress>()) {}
  TransactionDestinationEntry(uint64_t amount, const AccountPublicAddress &addr) : amount(amount), addr(addr) {}
};

struct tx_message_entry
{
  std::string message;
  bool encrypt;
  AccountPublicAddress addr;
};

bool constructTransaction(
  const AccountKeys& senderAccountKeys,
  const std::vector<TransactionSourceEntry>& sources,
  const std::vector<TransactionDestinationEntry>& destinations,
  const std::vector<tx_message_entry>& messages,
  uint64_t ttl, std::vector<uint8_t> extra, Transaction& transaction, uint64_t unlock_time, Logging::ILogger& log, Crypto::SecretKey& transactionSK);

inline bool constructTransaction(
  const AccountKeys& sender_account_keys,
  const std::vector<TransactionSourceEntry>& sources,
  const std::vector<TransactionDestinationEntry>& destinations,
  std::vector<uint8_t> extra, Transaction& tx, uint64_t unlock_time, Logging::ILogger& log, Crypto::SecretKey& transactionSK) {

  return constructTransaction(sender_account_keys, sources, destinations, std::vector<tx_message_entry>(), 0, extra, tx, unlock_time, log, transactionSK);
}

bool is_out_to_acc(const AccountKeys& acc, const KeyOutput& out_key, const Crypto::PublicKey& tx_pub_key, uint64_t keyIndex);
bool is_out_to_acc(const AccountKeys& acc, const KeyOutput& out_key, const Crypto::KeyDerivation& derivation, uint64_t keyIndex);
bool lookup_acc_outs(const AccountKeys& acc, const Transaction& tx, const Crypto::PublicKey& tx_pub_key, std::vector<uint64_t>& outs, uint64_t& money_transfered);
bool lookup_acc_outs(const AccountKeys& acc, const Transaction& tx, std::vector<uint64_t>& outs, uint64_t& money_transfered);
bool generate_key_image_helper(const AccountKeys& ack, const Crypto::PublicKey& tx_public_key, uint64_t real_output_index, KeyPair& in_ephemeral, Crypto::KeyImage& ki);
std::string short_hash_str(const Crypto::Hash& h);

bool get_block_hashing_blob(const Block& b, BinaryArray& blob);
bool get_aux_block_header_hash(const Block& b, Crypto::Hash& res);
bool get_block_hash(const Block& b, Crypto::Hash& res);
Crypto::Hash get_block_hash(const Block& b);
bool get_block_longhash(Crypto::cn_context &context, const Block& b, Crypto::Hash& res);
bool get_inputs_money_amount(const Transaction& tx, uint64_t& money);
uint64_t get_outs_money_amount(const Transaction& tx);
bool check_inputs_types_supported(const TransactionPrefix& tx);
bool check_outs_valid(const TransactionPrefix& tx, std::string* error = 0);
bool checkMultisignatureInputsDiff(const TransactionPrefix& tx);

bool check_money_overflow(const TransactionPrefix& tx);
bool check_outs_overflow(const TransactionPrefix& tx);
bool check_inputs_overflow(const TransactionPrefix& tx);
uint32_t get_block_height(const Block& b);
std::vector<uint32_t> relative_output_offsets_to_absolute(const std::vector<uint32_t>& off);
std::vector<uint32_t> absolute_output_offsets_to_relative(const std::vector<uint32_t>& off);


// 62387455827 -> 455827 + 7000000 + 80000000 + 300000000 + 2000000000 + 60000000000, where 455827 <= dust_threshold
template<typename chunk_handler_t, typename dust_handler_t>
void decompose_amount_into_digits(uint64_t amount, uint64_t dust_threshold, const chunk_handler_t& chunk_handler, const dust_handler_t& dust_handler) {
  if (0 == amount) {
    return;
  }

  bool is_dust_handled = false;
  uint64_t dust = 0;
  uint64_t order = 1;
  while (0 != amount) {
    uint64_t chunk = (amount % 10) * order;
    amount /= 10;
    order *= 10;

    if (dust + chunk <= dust_threshold) {
      dust += chunk;
    } else {
      if (!is_dust_handled && 0 != dust) {
        dust_handler(dust);
        is_dust_handled = true;
      }
      if (0 != chunk) {
        chunk_handler(chunk);
      }
    }
  }

  if (!is_dust_handled && 0 != dust) {
    dust_handler(dust);
  }
}

void get_tx_tree_hash(const std::vector<Crypto::Hash>& tx_hashes, Crypto::Hash& h);
Crypto::Hash get_tx_tree_hash(const std::vector<Crypto::Hash>& tx_hashes);
Crypto::Hash get_tx_tree_hash(const Block& b);
bool is_valid_decomposed_amount(uint64_t amount);


}

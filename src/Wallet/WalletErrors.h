// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <system_error>

namespace CryptoNote {
namespace error {

// custom error conditions enum type:
enum WalletErrorCodes {
  NOT_INITIALIZED = 1,
  ALREADY_INITIALIZED,
  WRONG_STATE,
  WRONG_PASSWORD,
  INTERNAL_WALLET_ERROR,
  MIXIN_COUNT_TOO_BIG,
  NOTHING_TO_OPTIMIZE,
  MINIMUM_INPUT_COUNT,
  MINIMUM_ONE_ADDRESS,
  THRESHOLD_TOO_LOW,
  BAD_ADDRESS,
  BAD_INTEGRATED_ADDRESS,
  TRANSACTION_SIZE_TOO_BIG,
  WRONG_AMOUNT,
  BAD_PREFIX,
  SUM_OVERFLOW,
  ZERO_DESTINATION,
  TX_CANCEL_IMPOSSIBLE,
  TX_CANCELLED,
  OPERATION_CANCELLED,
  TX_TRANSFER_IMPOSSIBLE,
  WRONG_VERSION,
  FEE_TOO_SMALL,
  KEY_GENERATION_ERROR,
  INDEX_OUT_OF_RANGE,
  ADDRESS_ALREADY_EXISTS,
  TRACKING_MODE,
  WRONG_PARAMETERS,
  OBJECT_NOT_FOUND,
  WALLET_NOT_FOUND,
  CHANGE_ADDRESS_REQUIRED,
  CHANGE_ADDRESS_NOT_FOUND,
  DEPOSIT_TERM_TOO_SMALL,
  DEPOSIT_TERM_TOO_BIG,
  DEPOSIT_AMOUNT_TOO_SMALL,
  DEPOSIT_DOESNOT_EXIST,
  DEPOSIT_LOCKED,
  DESTINATION_ADDRESS_REQUIRED,
  DESTINATION_ADDRESS_NOT_FOUND,
  DAEMON_NOT_SYNCED
};

// custom category:
class WalletErrorCategory : public std::error_category {
public:
  static WalletErrorCategory INSTANCE;

  virtual const char* name() const throw() override {
    return "WalletErrorCategory";
  }

  virtual std::error_condition default_error_condition(int ev) const throw() override {
    return std::error_condition(ev, *this);
  }

  virtual std::string message(int ev) const override {
    switch (ev) {
    case NOT_INITIALIZED:          return "Object was not initialized";
    case WRONG_PASSWORD:           return "The password is wrong";
    case ALREADY_INITIALIZED:      return "The object is already initialized";
    case INTERNAL_WALLET_ERROR:    return "Internal error occurred";
    case MIXIN_COUNT_TOO_BIG:      return "MixIn count is too big";
    case NOTHING_TO_OPTIMIZE:      return "There are no inputs to optimize";
    case THRESHOLD_TOO_LOW:        return "Threshold must be greater than 10";    
    case MINIMUM_ONE_ADDRESS:      return "You should have at least one address";
    case MINIMUM_INPUT_COUNT:      return "Not enough inputs to optimizne, minimum 12";    
    case BAD_ADDRESS:              return "Invalid address";
    case BAD_INTEGRATED_ADDRESS:   return "Integrated address should be 186 characters";    
    case TRANSACTION_SIZE_TOO_BIG: return "Transaction size is too big, please optimize your wallet.";
    case WRONG_AMOUNT:             return "Insufficient funds";
    case BAD_PREFIX:               return "Address has incorrect prefix";    
    case SUM_OVERFLOW:             return "Sum overflow";
    case ZERO_DESTINATION:         return "The destination is empty";
    case TX_CANCEL_IMPOSSIBLE:     return "Impossible to cancel transaction";
    case WRONG_STATE:              return "The wallet is in wrong state (maybe loading or saving), try again later";
    case OPERATION_CANCELLED:      return "The operation you've requested has been cancelled";
    case TX_TRANSFER_IMPOSSIBLE:   return "Transaction transfer impossible";
    case WRONG_VERSION:            return "Wrong version";
    case FEE_TOO_SMALL:            return "Transaction fee is too small";
    case KEY_GENERATION_ERROR:     return "Cannot generate new key";
    case INDEX_OUT_OF_RANGE:       return "Not found";
    case ADDRESS_ALREADY_EXISTS:   return "Address already exists";
    case TRACKING_MODE:            return "The wallet is in tracking mode";
    case WRONG_PARAMETERS:         return "Wrong parameters passed";
    case OBJECT_NOT_FOUND:         return "Object not found";
    case WALLET_NOT_FOUND:         return "Requested wallet not found";
    case CHANGE_ADDRESS_REQUIRED:  return "Change address required";
    case CHANGE_ADDRESS_NOT_FOUND: return "Change address not found";
    case DEPOSIT_TERM_TOO_SMALL:   return "Deposit term is too small";
    case DEPOSIT_TERM_TOO_BIG:     return "Deposit term is too big";
    case DEPOSIT_AMOUNT_TOO_SMALL: return "Deposit amount is too small";
    case DEPOSIT_DOESNOT_EXIST:    return "Deposit not found";
    case DESTINATION_ADDRESS_REQUIRED:  return  "Destination address required";
    case DESTINATION_ADDRESS_NOT_FOUND: return "Destination address not found";
    case DEPOSIT_LOCKED:           return "Deposit is locked";
    case DAEMON_NOT_SYNCED:         return "Daemon is not synchronized";
        default:
      return "Unknown error";
    }
  }

private:
  WalletErrorCategory() {
  }
};

}
}

inline std::error_code make_error_code(CryptoNote::error::WalletErrorCodes e) {
  return std::error_code(static_cast<int>(e), CryptoNote::error::WalletErrorCategory::INSTANCE);
}

namespace std {

template <>
struct is_error_code_enum<CryptoNote::error::WalletErrorCodes>: public true_type {};

}

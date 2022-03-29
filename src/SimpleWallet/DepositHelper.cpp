#include <sstream>
#include <string>

#include "DepositHelper.h"

#include "CryptoNoteConfig.h"

#include "Common/StringTools.h"

namespace cn
{
  uint32_t deposit_helper::deposit_term(cn::Deposit deposit)
  {
    return deposit.term;
  }

  std::string deposit_helper::deposit_amount(cn::Deposit deposit, const Currency& currency)
  {
    return currency.formatAmount(deposit.amount);
  }

  std::string deposit_helper::deposit_interest(cn::Deposit deposit, const Currency& currency)
  {
    return currency.formatAmount(deposit.interest);
  }

  std::string deposit_helper::deposit_status(cn::Deposit deposit)
  {
    std::string status_str = "";

    if (deposit.locked)
      status_str = "Locked";
    else if (deposit.spendingTransactionId == cn::WALLET_LEGACY_INVALID_TRANSACTION_ID)
      status_str = "Unlocked";
    else
      status_str = "Spent";

    return status_str;
  }

  size_t deposit_helper::deposit_creating_tx_id(cn::Deposit deposit)
  {
    return deposit.creatingTransactionId;
  }

  size_t deposit_helper::deposit_spending_tx_id(cn::Deposit deposit)
  {
    return deposit.spendingTransactionId;
  }

  uint64_t deposit_helper::deposit_unlock_height(cn::Deposit deposit)
  {
    uint64_t u_height = deposit.unlockHeight;

    if (u_height == 0)
      u_height = deposit.height + deposit.term;
    
    if (u_height == 0 || u_height % 21900)
      u_height = 0;

    return u_height;
  }

  uint64_t deposit_helper::deposit_height(cn::Deposit deposit)
  {
    return deposit.height;
  }

  std::string deposit_helper::get_deposit_info(cn::Deposit deposit, cn::DepositId did, const Currency& currency)
  {
    std::string unlock_str = "";

    bool bad_unlock = deposit.unlockHeight > cn::parameters::CRYPTONOTE_MAX_BLOCK_NUMBER;
    if (bad_unlock)
      unlock_str = "Please wait.";
    else
      unlock_str = std::to_string(deposit_unlock_height(deposit));

    bool bad_unlock2 = unlock_str == "0";
    if (bad_unlock2)
      unlock_str = "ERROR";

    std::stringstream full_info;

    full_info << std::left <<
      std::setw(8)  << std::to_string(did) << " | " <<
      std::setw(20) << deposit_amount(deposit, currency) << " | " <<
      std::setw(20) << deposit_interest(deposit, currency) << " | " <<
      std::setw(16) << unlock_str << " | " <<
      std::setw(10) << deposit_status(deposit);
    
    std::string as_str = full_info.str();

    return as_str;
  }

  std::string deposit_helper::get_full_deposit_info(cn::Deposit deposit, cn::DepositId did, const Currency& currency)
  {
    std::string unlock_str = "";

    bool bad_unlock = deposit_unlock_height(deposit) > cn::parameters::CRYPTONOTE_MAX_BLOCK_NUMBER;
    if (bad_unlock)
      unlock_str = "Please wait.";
    else
      unlock_str = std::to_string(deposit_unlock_height(deposit));

    bool bad_unlock2 = unlock_str == "0";
    if (bad_unlock2)
      unlock_str = "ERROR";

    uint64_t unlocked = deposit_height(deposit) + deposit_term(deposit);
    std::string unlocked_str = "";

    bool bad_unlock3 = unlocked_str == "0" || unlocked % 21900;
    if (bad_unlock3)
      unlocked_str = "ERROR";

    std::stringstream full_info;

    full_info <<
      "ID:            "  << std::to_string(did) << "\n" <<
      "Amount:        " << deposit_amount(deposit, currency) << "\n" <<
      "Interest:      " << deposit_interest(deposit, currency) << "\n" <<
      "Height:        " << deposit_height(deposit) << "\n" <<
      "Unlock Height: " << unlock_str << " | " << unlocked << "\n" <<
      "Status:        " << deposit_status(deposit) << "\n";

    std::string as_str = full_info.str();

    return as_str;
  }
}
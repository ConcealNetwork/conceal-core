#include <sstream>
#include <string>

#include "DepositHelper.h"

#include "CryptoNoteConfig.h"

#include "Common/StringTools.h"

namespace cn
{
  uint32_t deposit_helper::deposit_term(const cn::Deposit &deposit) const
  {
    return deposit.term;
  }

  std::string deposit_helper::deposit_amount(const cn::Deposit &deposit, const Currency &currency) const
  {
    return currency.formatAmount(deposit.amount);
  }

  std::string deposit_helper::deposit_interest(const cn::Deposit &deposit, const Currency &currency) const
  {
    return currency.formatAmount(deposit.interest);
  }

  std::string deposit_helper::deposit_status(const cn::Deposit &deposit) const
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

  size_t deposit_helper::deposit_creating_tx_id(const cn::Deposit &deposit) const
  {
    return deposit.creatingTransactionId;
  }

  size_t deposit_helper::deposit_spending_tx_id(const cn::Deposit &deposit) const
  {
    return deposit.spendingTransactionId;
  }

  std::string deposit_helper::deposit_unlock_height(const cn::Deposit &deposit, const cn::WalletLegacyTransaction &txInfo) const
  {
    std::string unlock_str = "";

    if (txInfo.blockHeight > cn::parameters::CRYPTONOTE_MAX_BLOCK_NUMBER)
    {
      unlock_str = "Please wait.";
    }
    else
    {
      unlock_str = std::to_string(txInfo.blockHeight + deposit_term(deposit));
    }

    bool bad_unlock2 = unlock_str == "0";
    if (bad_unlock2)
    {
      unlock_str = "ERROR";
    }

    return unlock_str;
  }

  std::string deposit_helper::deposit_height(const cn::WalletLegacyTransaction &txInfo) const
  {
    std::string height_str = "";
    uint64_t deposit_height = txInfo.blockHeight;

    bool bad_unlock = deposit_height > cn::parameters::CRYPTONOTE_MAX_BLOCK_NUMBER;
    if (bad_unlock)
    {
      height_str = "Please wait.";
    }
    else
    {
      height_str = std::to_string(deposit_height);
    }

    bool bad_unlock2 = height_str == "0";
    if (bad_unlock2)
    {
      height_str = "ERROR";
    }

    return height_str;
  }

  std::string deposit_helper::get_deposit_info(const cn::Deposit &deposit, cn::DepositId did, const Currency &currency, const cn::WalletLegacyTransaction &txInfo) const
  {
    std::stringstream full_info;

    full_info << std::left <<
      std::setw(8)  << common::makeCenteredString(8, std::to_string(did)) << " | " <<
      std::setw(20) << common::makeCenteredString(20, deposit_amount(deposit, currency)) << " | " <<
      std::setw(20) << common::makeCenteredString(20, deposit_interest(deposit, currency)) << " | " <<
      std::setw(16) << common::makeCenteredString(16, deposit_unlock_height(deposit, txInfo)) << " | " <<
      std::setw(10) << common::makeCenteredString(10, deposit_status(deposit));
    
    std::string as_str = full_info.str();

    return as_str;
  }

  std::string deposit_helper::get_full_deposit_info(const cn::Deposit &deposit, cn::DepositId did, const Currency &currency, const cn::WalletLegacyTransaction &txInfo) const
  {
    std::stringstream full_info;

    full_info << "ID:            " << std::to_string(did) << "\n"
              << "Amount:        " << deposit_amount(deposit, currency) << "\n"
              << "Interest:      " << deposit_interest(deposit, currency) << "\n"
              << "Height:        " << deposit_height(txInfo) << "\n"
              << "Unlock Height: " << deposit_unlock_height(deposit, txInfo) << "\n"
              << "Status:        " << deposit_status(deposit) << "\n";

    std::string as_str = full_info.str();

    return as_str;
  }
}

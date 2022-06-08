

#include <string>

#include "IWalletLegacy.h"

#include "CryptoNoteCore/Currency.h"

namespace cn
{
  class deposit_helper
  {
  public:
    /**
     * @param deposit - obtain information from deposit
     * @return - returns deposit term, should be multiple of 21900
    **/
    uint32_t deposit_term(const cn::Deposit &deposit) const;

    /**
     * @param deposit - obtain information from deposit
     * @param currency - format output amount
     * @return - returns deposit amount as string
    **/
    std::string deposit_amount(const cn::Deposit &deposit, const Currency &currency) const;

    /**
     * @param deposit  - obtain information from deposit
     * @param currency - format output amount
     * @return - returns deposit interest based of amount and term as string
    **/
    std::string deposit_interest(const cn::Deposit &deposit, const Currency &currency) const;

    /**
     * @param deposit - obtain information from deposit
     * @return - "Locked", "Unlocked" or "Spent" depending on state of deposit
    **/
    std::string deposit_status(const cn::Deposit &deposit) const;

    /**
     * @param deposit - obtain information from deposit
     * @return - returns deposit creatingTransactionId 
    **/
    size_t deposit_creating_tx_id(const cn::Deposit &deposit) const;

    /**
     * @param deposit - obtain information from deposit
     * @return - returns deposit spendingTransactionId
    **/
    size_t deposit_spending_tx_id(const cn::Deposit &deposit) const;

    /**
     * @param deposit - obtain information from deposit
     * @return - returns unlock height from height deposited + term
    **/
    std::string deposit_unlock_height(const cn::Deposit &deposit, const cn::WalletLegacyTransaction &txInfo) const;

    /**
     * @param txInfo - obtain height from transaction
     * @return - returns deposit height
    **/
    std::string deposit_height(const cn::WalletLegacyTransaction &txInfo) const;

    /**
     * @param deposit - obtain information from deposit
     * @param did - id to search deposit
     * @param currency - used to supply amount and interest
     * @param txInfo - obtain height from transaction
     * @return - returns deposit info string in single row style
    **/
    std::string get_deposit_info(const cn::Deposit &deposit, cn::DepositId did, const Currency &currency, const cn::WalletLegacyTransaction &txInfo) const;

    /**
     * @param deposit - obtain information from deposit
     * @param did - id to search deposit
     * @param currency - used to supply amount and interest
     * @param txInfo - obtain height from transaction
     * @return - returns full deposit info string
    **/
    std::string get_full_deposit_info(const cn::Deposit &deposit, cn::DepositId did, const Currency &currency, const cn::WalletLegacyTransaction &txInfo) const;
  };
}
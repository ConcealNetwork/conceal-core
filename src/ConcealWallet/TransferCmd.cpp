// Copyright (c) 2018-2022 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "TransferCmd.h"

#include "Common/Base58.h"
#include "Common/StringTools.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/TransactionExtra.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"

using namespace logging;

namespace cn
{
  transfer_cmd::transfer_cmd(const cn::Currency& currency, const std::string& remote_fee_address) :
    m_currency(currency),
    fee(currency.minimumFeeV2()),
    m_remote_address(remote_fee_address) {
  }

  bool transfer_cmd::parseTx(const LoggerRef& logger, const std::vector<std::string> &args)
  {
    ArgumentReader<std::vector<std::string>::const_iterator> ar(args.begin(), args.end());

    try 
    {
      /* Parse the remaining arguments */
      while (!ar.eof()) 
      {
        auto arg = ar.next();

        if (arg.size() && arg[0] == '-') 
        {
          const auto& value = ar.next();
          bool ttl_to_str = common::fromString(value, ttl) || ttl < 1 || ttl * 60 > m_currency.mempoolTxLiveTime();

          if (arg == "-p" && !createTxExtraWithPaymentId(value, extra))
          {
            logger(ERROR, BRIGHT_RED) << "payment ID has invalid format: \"" << value << "\", expected 64-character string";
            return false;
          }
          else if (arg == "-m")
          {
            messages.emplace_back(value);
          }
          else if (arg == "-ttl" && !ttl_to_str)
          {
            logger(ERROR, BRIGHT_RED) << "TTL has invalid format: \"" << value << "\", " <<
              "enter time from 1 to " << (m_currency.mempoolTxLiveTime() / 60) << " minutes";
            return false;
          }
        }
        else
        {
          /* Integrated address check */
          if (arg.length() == 186)
          {
            std::string paymentID;
            std::string spendPublicKey;
            std::string viewPublicKey;
            const uint64_t paymentIDLen = 64;

            /* Extract the payment id */
            std::string decoded;
            uint64_t prefix;
            if (tools::base_58::decode_addr(arg, prefix, decoded))
              paymentID = decoded.substr(0, paymentIDLen);

            /* Validate and add the payment ID to extra */
            if (!createTxExtraWithPaymentId(paymentID, extra))
            {
              logger(ERROR, BRIGHT_RED) << "Integrated payment ID has invalid format: \"" << paymentID << "\", expected 64-character string";
              return false;
            }

            /* create the address from the public keys */
            std::string keys = decoded.substr(paymentIDLen, std::string::npos);
            cn::AccountPublicAddress addr;
            cn::BinaryArray ba = common::asBinaryArray(keys);

            if (!cn::fromBinaryArray(addr, ba))
              return true;

            std::string address = cn::getAccountAddressAsStr(cn::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX, addr);   

            arg = address;
          }

          WalletOrder destination;
          WalletOrder feeDestination;          
          cn::TransactionDestinationEntry de;
          std::string aliasUrl;

          if (!m_currency.parseAccountAddressString(arg, de.addr))
            aliasUrl = arg;

          auto value = ar.next();
          bool ok = m_currency.parseAmount(value, de.amount);

          if (!ok || 0 == de.amount)
          {
            logger(ERROR, BRIGHT_RED) << "amount is wrong: " << arg << ' ' << value <<
              ", expected number from 0 to " << m_currency.formatAmount(cn::parameters::MONEY_SUPPLY);
            return false;
          }

          if (aliasUrl.empty())
          {
            destination.address = arg;
            destination.amount = de.amount;
            dsts.push_back(destination);
          }
          else
          {
            aliases[aliasUrl].emplace_back(WalletOrder{"", de.amount});
          }

          /* Remote node transactions fees are 10000 X */
          if (!m_remote_address.empty())
          {
            destination.address = m_remote_address;                   
            destination.amount = 10000;
            dsts.push_back(destination);
          }
        }
      }

      if (dsts.empty() && aliases.empty())
      {
        logger(ERROR, BRIGHT_RED) << "At least one destination address is required";
        return false;
      }
    }
    catch (const std::exception& e)
    {
      logger(ERROR, BRIGHT_RED) << e.what();
      return false;
    }

    return true;
  }
} 
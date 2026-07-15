// Copyright (c) 2018-2026 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license.

#include "ChainAudit/ChainAudit.h"

#include <sstream>

#include <boost/variant/get.hpp>

#include "Common/StringTools.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"

namespace cn
{
namespace chain_audit
{
  namespace
  {
    const uint64_t COINBASE_REWARD_OVERCLAIM_TOLERANCE = 10;

    Finding makeFinding(Severity severity, const std::string &code,
                        uint32_t height, uint32_t txIndex,
                        uint32_t outputIndex, const crypto::Hash &blockHash,
                        const crypto::Hash &txHash, const std::string &message)
    {
      Finding finding;
      finding.severity = severity;
      finding.code = code;
      finding.height = height;
      finding.txIndex = txIndex;
      finding.outputIndex = outputIndex;
      finding.blockHash = blockHash;
      finding.txHash = txHash;
      finding.message = message;
      return finding;
    }

    void setAmountDelta(Finding &finding, uint64_t actualAmount,
                        uint64_t expectedAmount, uint64_t deltaAmount,
                        uint64_t consensusTolerance,
                        bool consensusAccepted)
    {
      finding.hasAmountDelta = true;
      finding.actualAmount = actualAmount;
      finding.expectedAmount = expectedAmount;
      finding.deltaAmount = deltaAmount;
      finding.consensusTolerance = consensusTolerance;
      finding.consensusAccepted = consensusAccepted;
    }

    bool isAllowedCoinbaseOutputTarget(const Currency &currency,
                                       const TransactionOutputTarget &target)
    {
      if (target.type() == typeid(KeyOutput))
      {
        return true;
      }

      return false;
    }

    uint64_t outputAmount(const Transaction &tx)
    {
      uint64_t total = 0;
      for (const auto &output : tx.outputs)
      {
        total += output.amount;
      }
      return total;
    }

    uint64_t totalFees(const Currency &currency,
                       const std::vector<Transaction> &transactions,
                       uint32_t height)
    {
      uint64_t total = 0;
      for (const auto &tx : transactions)
      {
        uint64_t fee = 0;
        if (currency.getTransactionFee(tx, fee, height))
        {
          total += fee;
        }
      }
      return total;
    }

    uint64_t totalInterest(const Currency &currency,
                           const std::vector<Transaction> &transactions,
                           uint32_t height)
    {
      uint64_t total = 0;
      for (const auto &tx : transactions)
      {
        total += currency.calculateTotalTransactionInterest(tx, height);
      }
      return total;
    }
  }

  const char *severityName(Severity severity)
  {
    switch (severity)
    {
    case Severity::Info:
      return "INFO";
    case Severity::Warning:
      return "WARNING";
    case Severity::High:
      return "HIGH";
    case Severity::Critical:
      return "CRITICAL";
    }
    return "UNKNOWN";
  }

  std::string outputTypeName(const TransactionOutputTarget &target)
  {
    if (target.type() == typeid(KeyOutput))
      return "KeyOutput";
    if (target.type() == typeid(MultisignatureOutput))
      return "MultisignatureOutput";
    return "UnknownOutput";
  }

  std::string inputTypeName(const TransactionInput &input)
  {
    if (input.type() == typeid(BaseInput))
      return "BaseInput";
    if (input.type() == typeid(KeyInput))
      return "KeyInput";
    if (input.type() == typeid(MultisignatureInput))
      return "MultisignatureInput";
    return "UnknownInput";
  }

  bool hasClassicalInput(const Transaction &tx)
  {
    for (const auto &input : tx.inputs)
    {
      if (input.type() == typeid(KeyInput) ||
          input.type() == typeid(MultisignatureInput))
      {
        return true;
      }
    }
    return false;
  }

  bool hasMultisignatureDepositOutput(const Transaction &tx)
  {
    for (const auto &output : tx.outputs)
    {
      if (output.target.type() == typeid(MultisignatureOutput) &&
          boost::get<MultisignatureOutput>(output.target).term != 0)
      {
        return true;
      }
    }
    return false;
  }

  size_t cumulativeBlockSize(const Block &block, const std::vector<Transaction> &transactions)
  {
    size_t size = getObjectBinarySize(block.baseTransaction);
    for (const auto &tx : transactions)
    {
      size += getObjectBinarySize(tx);
    }
    return size;
  }

  void auditTransactionOutputs(const Currency &currency, const Transaction &tx,
                               uint32_t height, uint32_t txIndex,
                               const crypto::Hash &blockHash,
                               const crypto::Hash &txHash,
                               std::vector<Finding> &findings)
  {
    for (size_t i = 0; i < tx.outputs.size(); ++i)
    {
      const TransactionOutput &output = tx.outputs[i];

      if (output.target.type() == typeid(MultisignatureOutput))
      {
        const MultisignatureOutput &msig =
            boost::get<MultisignatureOutput>(output.target);
        if (msig.requiredSignatureCount == 0)
        {
          findings.push_back(makeFinding(
              Severity::High, "MSIG_ZERO_REQUIRED_SIGNATURES", height,
              txIndex, static_cast<uint32_t>(i), blockHash, txHash,
              "multisignature output requires zero signatures"));
        }

        if (!currency.validateOutput(output.amount, msig, height))
        {
          findings.push_back(makeFinding(
              msig.term == 0 ? Severity::Warning : Severity::High,
              "MSIG_OUTPUT_INVALID_TERM_OR_AMOUNT", height, txIndex,
              static_cast<uint32_t>(i), blockHash, txHash,
              "multisignature output fails deposit term/min-amount validation"));
        }
      }
    }
  }

  void auditBlock(const Currency &currency, const Block &block,
                  const std::vector<Transaction> &transactions,
                  const BlockAuditContext &context,
                  std::vector<Finding> &findings)
  {
    const crypto::Hash coinbaseHash = getObjectHash(block.baseTransaction);
    uint64_t minerReward = outputAmount(block.baseTransaction);

    for (size_t i = 0; i < block.baseTransaction.outputs.size(); ++i)
    {
      const TransactionOutput &output = block.baseTransaction.outputs[i];
      if (!isAllowedCoinbaseOutputTarget(currency, output.target))
      {
        std::ostringstream ss;
        ss << "coinbase output has non-standard target "
           << outputTypeName(output.target);
        findings.push_back(makeFinding(
            output.target.type() == typeid(MultisignatureOutput)
                ? Severity::Critical
                : Severity::High,
            "COINBASE_NON_STANDARD_OUTPUT", context.height, 0,
            static_cast<uint32_t>(i), context.blockHash, coinbaseHash,
            ss.str()));
      }
    }

    auditTransactionOutputs(currency, block.baseTransaction, context.height, 0,
                            context.blockHash, coinbaseHash, findings);

    const uint64_t fee = totalFees(currency, transactions, context.height);
    uint64_t expectedReward = 0;
    int64_t emissionChange = 0;
    if (currency.getBlockReward(context.medianBlockSize,
                                context.cumulativeBlockSize,
                                context.previousGeneratedCoins, fee,
                                context.height, expectedReward,
                                emissionChange))
    {
      if (minerReward > expectedReward)
      {
        const uint64_t delta = minerReward - expectedReward;
        const bool consensusAccepted =
            delta <= COINBASE_REWARD_OVERCLAIM_TOLERANCE;
        std::ostringstream ss;
        ss << "coinbase pays " << currency.formatAmount(minerReward)
           << ", expected " << currency.formatAmount(expectedReward)
           << ", delta_atomic=" << delta
           << ", consensus_tolerance_atomic="
           << COINBASE_REWARD_OVERCLAIM_TOLERANCE;

        if (!consensusAccepted || context.collectInfoFindings)
        {
          Finding finding = makeFinding(
              consensusAccepted ? Severity::Info : Severity::Critical,
              consensusAccepted ? "COINBASE_REWARD_TOLERATED_DUST"
                                : "COINBASE_REWARD_OVERCLAIM",
              context.height, 0, 0, context.blockHash, coinbaseHash, ss.str());
          setAmountDelta(finding, minerReward, expectedReward, delta,
                         COINBASE_REWARD_OVERCLAIM_TOLERANCE,
                         consensusAccepted);
          findings.push_back(finding);
        }
      }

      const uint64_t interest = totalInterest(currency, transactions, context.height);
      uint64_t expectedGenerated = context.previousGeneratedCoins + interest;
      if (emissionChange >= 0)
      {
        expectedGenerated += static_cast<uint64_t>(emissionChange);
      }
      else
      {
        expectedGenerated -= static_cast<uint64_t>(-emissionChange);
      }
      if (context.storedGeneratedCoins != expectedGenerated)
      {
        std::ostringstream ss;
        ss << "stored emitted coins " << currency.formatAmount(context.storedGeneratedCoins)
           << ", expected " << currency.formatAmount(expectedGenerated);
        findings.push_back(makeFinding(
            Severity::Critical, "EMITTED_COINS_DRIFT", context.height, 0, 0,
            context.blockHash, coinbaseHash, ss.str()));
      }
    }
    else
    {
      findings.push_back(makeFinding(
          Severity::High, "BLOCK_REWARD_RECALC_FAILED", context.height, 0, 0,
          context.blockHash, coinbaseHash,
          "scanner could not recalculate the expected block reward"));
    }

    for (size_t i = 0; i < transactions.size(); ++i)
    {
      const Transaction &tx = transactions[i];
      const crypto::Hash txHash = getObjectHash(tx);
      auditTransactionOutputs(currency, tx, context.height,
                              static_cast<uint32_t>(i + 1), context.blockHash,
                              txHash, findings);

      if (context.collectInfoFindings && context.inCheckpointZone &&
          hasClassicalInput(tx))
      {
        findings.push_back(makeFinding(
            Severity::Info, "CHECKPOINT_ZONE_CLASSICAL_SPEND", context.height,
            static_cast<uint32_t>(i + 1), 0, context.blockHash, txHash,
            "transaction has classical inputs inside checkpoint zone; reference checks run separately by the CLI scanner"));
      }
    }
  }
}
}

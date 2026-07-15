// Copyright (c) 2018-2026 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "CryptoNoteCore/Currency.h"
#include "crypto/hash.h"

namespace cn
{
namespace chain_audit
{
  enum class Severity
  {
    Info,
    Warning,
    High,
    Critical
  };

  struct Finding
  {
    Severity severity;
    std::string code;
    uint32_t height;
    uint32_t txIndex;
    uint32_t outputIndex;
    crypto::Hash blockHash;
    crypto::Hash txHash;
    std::string message;
    bool hasAmountDelta = false;
    uint64_t actualAmount = 0;
    uint64_t expectedAmount = 0;
    uint64_t deltaAmount = 0;
    uint64_t consensusTolerance = 0;
    bool consensusAccepted = false;
  };

  struct BlockAuditContext
  {
    uint32_t height;
    crypto::Hash blockHash;
    uint64_t previousGeneratedCoins;
    uint64_t storedGeneratedCoins;
    size_t medianBlockSize;
    size_t cumulativeBlockSize;
    bool inCheckpointZone;
    bool collectInfoFindings = true;
  };

  const char *severityName(Severity severity);
  std::string outputTypeName(const TransactionOutputTarget &target);
  std::string inputTypeName(const TransactionInput &input);

  bool hasClassicalInput(const Transaction &tx);
  bool hasMultisignatureDepositOutput(const Transaction &tx);
  size_t cumulativeBlockSize(const Block &block, const std::vector<Transaction> &transactions);

  void auditTransactionOutputs(const Currency &currency, const Transaction &tx,
                               uint32_t height, uint32_t txIndex,
                               const crypto::Hash &blockHash,
                               const crypto::Hash &txHash,
                               std::vector<Finding> &findings);

  void auditBlock(const Currency &currency, const Block &block,
                  const std::vector<Transaction> &transactions,
                  const BlockAuditContext &context,
                  std::vector<Finding> &findings);
}
}

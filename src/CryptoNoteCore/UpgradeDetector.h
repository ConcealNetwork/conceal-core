// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <algorithm>
#include <cstdint>
#include <ctime>

#include "Common/StringTools.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteConfig.h"
#include <Logging/LoggerRef.h>

namespace CryptoNote {
  class UpgradeDetectorBase {
  public:
    enum : uint32_t {
      UNDEF_HEIGHT = static_cast<uint32_t>(-1),
    };
  };

  static_assert(CryptoNote::UpgradeDetectorBase::UNDEF_HEIGHT == UINT32_C(0xFFFFFFFF), "UpgradeDetectorBase::UNDEF_HEIGHT has invalid value");

  template <typename BC>
  class BasicUpgradeDetector : public UpgradeDetectorBase {
  public:
    BasicUpgradeDetector(const Currency& currency, BC& blockchain, uint8_t targetVersion, Logging::ILogger& log) :
      m_currency(currency),
      m_blockchain(blockchain),
      m_targetVersion(targetVersion),
      m_votingCompleteHeight(UNDEF_HEIGHT),
      logger(log, "upgrade") {
    }

    bool init() {
      uint32_t upgradeHeight = m_currency.upgradeHeight(m_targetVersion);
      if (upgradeHeight == UNDEF_HEIGHT) {
        if (m_blockchain.empty()) {
          m_votingCompleteHeight = UNDEF_HEIGHT;

        } else if (m_targetVersion - 1 == m_blockchain.back().bl.majorVersion) {
          m_votingCompleteHeight = findVotingCompleteHeight(m_blockchain.size() - 1);

        } else if (m_targetVersion <= m_blockchain.back().bl.majorVersion) {
          auto it = std::lower_bound(m_blockchain.begin(), m_blockchain.end(), m_targetVersion,
            [](const typename BC::value_type& b, uint8_t v) { return b.bl.majorVersion < v; });
          if (it == m_blockchain.end() || it->bl.majorVersion != m_targetVersion) {
            logger(Logging::ERROR, Logging::BRIGHT_RED) << "Internal error: upgrade height isn't found";
            return false;
          }

          uint32_t upgradeHeight = it - m_blockchain.begin();
          m_votingCompleteHeight = findVotingCompleteHeight(upgradeHeight);
          if (m_votingCompleteHeight == UNDEF_HEIGHT) {
            logger(Logging::ERROR, Logging::BRIGHT_RED) << "Internal error: voting complete height isn't found, upgrade height = " << upgradeHeight;
            return false;
          }
        } else {
          m_votingCompleteHeight = UNDEF_HEIGHT;
        }
      } else if (!m_blockchain.empty()) {
        if (m_blockchain.size() <= upgradeHeight + 1) {
          if (m_blockchain.back().bl.majorVersion >= m_targetVersion) {
            logger(Logging::ERROR, Logging::BRIGHT_RED) << "Internal error: block at height " << (m_blockchain.size() - 1) <<
              " has invalid version " << static_cast<int>(m_blockchain.back().bl.majorVersion) <<
              ", expected " << static_cast<int>(m_targetVersion - 1) << " or less";
            return false;
          }
        } else {
          int blockVersionAtUpgradeHeight = m_blockchain[upgradeHeight].bl.majorVersion;
          if (blockVersionAtUpgradeHeight != m_targetVersion - 1) {
          }

          int blockVersionAfterUpgradeHeight = m_blockchain[upgradeHeight + 1].bl.majorVersion;
          if (blockVersionAfterUpgradeHeight != m_targetVersion) {
            logger(Logging::ERROR, Logging::BRIGHT_RED) << "Internal error: block at height " << (upgradeHeight + 1) <<
              " has invalid version " << blockVersionAfterUpgradeHeight <<
              ", expected " << static_cast<int>(m_targetVersion);
            return false;
          }
        }
      }

      return true;
    }

    uint8_t targetVersion() const {
      return m_targetVersion;
    }
    uint32_t votingCompleteHeight() const {
      return m_votingCompleteHeight;
    }

    uint32_t upgradeHeight() const {
      if (m_currency.upgradeHeight(m_targetVersion) == UNDEF_HEIGHT) {
        return m_votingCompleteHeight == UNDEF_HEIGHT ? UNDEF_HEIGHT : m_currency.calculateUpgradeHeight(m_votingCompleteHeight);
      } else {
        return m_currency.upgradeHeight(m_targetVersion);
      }
    }

    void blockPushed() {
      assert(!m_blockchain.empty());

      if (m_currency.upgradeHeight(m_targetVersion) != UNDEF_HEIGHT) {
        if (m_blockchain.size() <= m_currency.upgradeHeight(m_targetVersion) + 1) {
          assert(m_blockchain.back().bl.majorVersion <= m_targetVersion - 1);
        } else {
          assert(m_blockchain.back().bl.majorVersion >= m_targetVersion);
        }

      } else if (m_votingCompleteHeight != UNDEF_HEIGHT) {
        assert(m_blockchain.size() > m_votingCompleteHeight);

        if (m_blockchain.size() <= upgradeHeight()) {
          assert(m_blockchain.back().bl.majorVersion == m_targetVersion - 1);

          if (m_blockchain.size() % (60 * 60 / m_currency.difficultyTarget()) == 0) {
            auto interval = m_currency.difficultyTarget() * (upgradeHeight() - m_blockchain.size() + 2);
            time_t upgradeTimestamp = time(nullptr) + static_cast<time_t>(interval);
            struct tm* upgradeTime = localtime(&upgradeTimestamp);;
            char upgradeTimeStr[40];
            strftime(upgradeTimeStr, 40, "%H:%M:%S %Y.%m.%d", upgradeTime);

            logger(Logging::TRACE, Logging::BRIGHT_GREEN) << "###### UPGRADE is going to happen after block index " << upgradeHeight() << " at about " <<
              upgradeTimeStr << " (in " << Common::timeIntervalToString(interval) << ")! Current last block index " << (m_blockchain.size() - 1) <<
              ", hash " << get_block_hash(m_blockchain.back().bl);
          }
        } else if (m_blockchain.size() == upgradeHeight() + 1) {
          assert(m_blockchain.back().bl.majorVersion == m_targetVersion - 1);

          logger(Logging::TRACE, Logging::BRIGHT_GREEN) << "###### UPGRADE has happened! Starting from block index " << (upgradeHeight() + 1) <<
            " blocks with major version below " << static_cast<int>(m_targetVersion) << " will be rejected!";
        } else {
          assert(m_blockchain.back().bl.majorVersion == m_targetVersion);
        }

      } else {
        uint32_t lastBlockHeight = m_blockchain.size() - 1;
        if (isVotingComplete(lastBlockHeight)) {
          m_votingCompleteHeight = lastBlockHeight;
          logger(Logging::TRACE, Logging::BRIGHT_GREEN) << "###### UPGRADE voting complete at block index " << m_votingCompleteHeight <<
            "! UPGRADE is going to happen after block index " << upgradeHeight() << "!";
        }
      }
    }

    void blockPopped() {
      if (m_votingCompleteHeight != UNDEF_HEIGHT) {
        assert(m_currency.upgradeHeight(m_targetVersion) == UNDEF_HEIGHT);

        if (m_blockchain.size() == m_votingCompleteHeight) {
          logger(Logging::TRACE, Logging::BRIGHT_YELLOW) << "###### UPGRADE after block index " << upgradeHeight() << " has been canceled!";
          m_votingCompleteHeight = UNDEF_HEIGHT;
        } else {
          assert(m_blockchain.size() > m_votingCompleteHeight);
        }
      }
    }

    uint64_t getNumberOfVotes(uint32_t height) {
      if (height < m_currency.upgradeVotingWindow() - 1) {
        return 0;
      }

      uint64_t voteCounter = 0;
      for (uint64_t i = height + 1 - m_currency.upgradeVotingWindow(); i <= height; ++i) {
        const auto& b = m_blockchain[i].bl;
        voteCounter += (b.majorVersion == m_targetVersion - 1) && (b.minorVersion == BLOCK_MINOR_VERSION_1) ? 1 : 0;
      }

      return voteCounter;
    }

  private:
    uint32_t findVotingCompleteHeight(uint32_t probableUpgradeHeight) {
      assert(m_currency.upgradeHeight(m_targetVersion) == UNDEF_HEIGHT);

      uint32_t probableVotingCompleteHeight = probableUpgradeHeight > m_currency.maxUpgradeDistance() ? probableUpgradeHeight - m_currency.maxUpgradeDistance() : 0;
      for (uint64_t i = probableVotingCompleteHeight; i <= probableUpgradeHeight; ++i) {
        if (isVotingComplete(static_cast<uint32_t>(i))) {
          return static_cast<uint32_t>(i);
        }
      }

      return UNDEF_HEIGHT;
    }

    bool isVotingComplete(uint32_t height) {
      assert(m_currency.upgradeHeight(m_targetVersion) == UNDEF_HEIGHT);
      assert(m_currency.upgradeVotingWindow() > 1);
      assert(m_currency.upgradeVotingThreshold() > 0 && m_currency.upgradeVotingThreshold() <= 100);

      uint64_t voteCounter = getNumberOfVotes(height);
      return m_currency.upgradeVotingThreshold() * m_currency.upgradeVotingWindow() <= 100 * voteCounter;
    }

  private:
    Logging::LoggerRef logger;
    const Currency& m_currency;
    BC& m_blockchain;
    uint8_t m_targetVersion;
    uint32_t m_votingCompleteHeight;
  };
}

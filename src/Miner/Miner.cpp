// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2021 Conceal Network & Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "Miner.h"

#include <functional>

#include "crypto/crypto.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"

#include <System/InterruptedException.h>

namespace cn {

miner::miner(platform_system::Dispatcher& dispatcher, logging::ILogger& logger) :
  m_dispatcher(dispatcher),
  m_miningStopped(dispatcher),
  m_state(MiningState::MINING_STOPPED),
  m_logger(logger, "Miner") {
}

miner::~miner() {
  assert(m_state != MiningState::MINING_IN_PROGRESS);
}

Block miner::mine(const BlockMiningParameters& blockMiningParameters, size_t threadCount) {
  if (threadCount == 0) {
    throw std::runtime_error("Miner requires at least one thread");
  }

  if (m_state == MiningState::MINING_IN_PROGRESS) {
    throw std::runtime_error("Mining is already in progress");
  }

  m_state = MiningState::MINING_IN_PROGRESS;
  m_miningStopped.clear();

  runWorkers(blockMiningParameters, threadCount);

  assert(m_state != MiningState::MINING_IN_PROGRESS);
  if (m_state == MiningState::MINING_STOPPED) {
    m_logger(logging::DEBUGGING) << "Mining has been stopped";
    throw platform_system::InterruptedException();
  }

  assert(m_state == MiningState::BLOCK_FOUND);
  return m_block;
}

void miner::stop() {
  MiningState state = MiningState::MINING_IN_PROGRESS;

  if (m_state.compare_exchange_weak(state, MiningState::MINING_STOPPED)) {
    m_miningStopped.wait();
    m_miningStopped.clear();
  }
}

void miner::runWorkers(BlockMiningParameters blockMiningParameters, size_t threadCount) {
  assert(threadCount > 0);

  m_logger(logging::INFO) << "Starting mining for difficulty " << blockMiningParameters.difficulty;

  try {
    blockMiningParameters.blockTemplate.nonce = crypto::rand<uint32_t>();

    for (size_t i = 0; i < threadCount; ++i) {
      m_workers.emplace_back(std::unique_ptr<platform_system::RemoteContext<void>> (
        new platform_system::RemoteContext<void>(m_dispatcher, std::bind(&miner::workerFunc, this, blockMiningParameters.blockTemplate, blockMiningParameters.difficulty, threadCount)))
      );

      blockMiningParameters.blockTemplate.nonce++;
    }

    m_workers.clear();

  } catch (std::exception& e) {
    m_logger(logging::ERROR) << "Error occured during mining: " << e.what();
    m_state = MiningState::MINING_STOPPED;
  }

  m_miningStopped.set();
}

void miner::workerFunc(const Block& blockTemplate, difficulty_type difficulty, uint32_t nonceStep) {
  try {
    Block block = blockTemplate;
    crypto::cn_context cryptoContext;

    while (m_state == MiningState::MINING_IN_PROGRESS) {
      crypto::Hash hash;
      if (!get_block_longhash(cryptoContext, block, hash)) {
        //error occured
        m_logger(logging::DEBUGGING) << "calculating long hash error occured";
        m_state = MiningState::MINING_STOPPED;
        return;
      }

      if (check_hash(hash, difficulty)) {
        m_logger(logging::INFO) << "Found block for difficulty " << difficulty;

        if (!setStateBlockFound()) {
          m_logger(logging::DEBUGGING) << "block is already found or mining stopped";
          return;
        }

        m_block = block;
        return;
      }

      block.nonce += nonceStep;
    }
  } catch (std::exception& e) {
    m_logger(logging::ERROR) << "Miner got error: " << e.what();
    m_state = MiningState::MINING_STOPPED;
  }
}

bool miner::setStateBlockFound() {
  auto state = m_state.load();

  for (;;) {
    switch (state) {
      case MiningState::BLOCK_FOUND:
        return false;

      case MiningState::MINING_IN_PROGRESS:
        if (m_state.compare_exchange_weak(state, MiningState::BLOCK_FOUND)) {
          return true;
        }
        break;

      case MiningState::MINING_STOPPED:
        return false;

      default:
        assert(false);
        return false;
    }
  }
}

} //namespace cn

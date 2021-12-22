// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once 

#include "Chaingen.h"

#include "CryptoNoteCore/Currency.h"
#include "TransactionBuilder.h"
#include <Logging/LoggerGroup.h>

class TestGenerator {
public:
  TestGenerator(const test_generator& gen, const cn::AccountBase& miner, cn::Block last, const cn::Currency& currency, std::vector<test_event_entry>& eventsRef) :
    lastBlock(last),
      generator(gen),
      minerAccount(miner),
      events(eventsRef) {
    minerAccount.generate();
  }

  TestGenerator(
    const cn::Currency& currency, 
    std::vector<test_event_entry>& eventsRef) :
      generator(currency),
      events(eventsRef) {
    minerAccount.generate();
    generator.constructBlock(genesisBlock, minerAccount, 1338224400);
    events.push_back(genesisBlock);
    lastBlock = genesisBlock;
	height = 0;
  }

  const cn::Currency& currency() const { return generator.currency(); }

  void makeNextBlock(const std::list<cn::Transaction>& txs = std::list<cn::Transaction>()) {
    cn::Block block;
    generator.constructBlock(block, lastBlock, minerAccount, txs);
    events.push_back(block);
    lastBlock = block;
	++height;
  }

  void makeNextBlock(const cn::Transaction& tx) {
    std::list<cn::Transaction> txs;
    txs.push_back(tx);
    makeNextBlock(txs);
  }

  void generateBlocks() {
    generateBlocks(currency().minedMoneyUnlockWindow());
  }

  void generateBlocks(size_t count, uint8_t majorVersion = cn::BLOCK_MAJOR_VERSION_1) {
    while (count--) {
      cn::Block next;
      generator.constructBlockManually(next, lastBlock, minerAccount, test_generator::bf_major_ver, majorVersion);
      lastBlock = next;
	  ++height;
      events.push_back(next);
    }
  }

  TransactionBuilder createTxBuilder(const cn::AccountBase& from, const cn::AccountBase& to, uint64_t amount, uint64_t fee) {

    std::vector<cn::TransactionSourceEntry> sources;
    std::vector<cn::TransactionDestinationEntry> destinations;

    fillTxSourcesAndDestinations(sources, destinations, from, to, amount, fee);

    TransactionBuilder builder(generator.currency());

    builder.setInput(sources, from.getAccountKeys());
    builder.setOutput(destinations);

    return builder;
  }

  void fillTxSourcesAndDestinations(
    std::vector<cn::TransactionSourceEntry>& sources, 
    std::vector<cn::TransactionDestinationEntry>& destinations,
    const cn::AccountBase& from, const cn::AccountBase& to, uint64_t amount, uint64_t fee, size_t nmix = 0) {
    fill_tx_sources_and_destinations(events, lastBlock, from, to, amount, fee, nmix, sources, destinations);
  }

  void constructTxToKey(
    cn::Transaction& tx,
    const cn::AccountBase& from,
    const cn::AccountBase& to,
    uint64_t amount,
    uint64_t fee,
    size_t nmix = 0) {
    construct_tx_to_key(logger, events, tx, lastBlock, from, to, amount, fee, nmix);
  }

  void addEvent(const test_event_entry& e) {
    events.push_back(e);
  }

  void addCallback(const std::string& name) {
    callback_entry cb;
    cb.callback_name = name;
    events.push_back(cb);
  }

  void addCheckAccepted() {
    addCallback("check_block_accepted");
  }

  void addCheckPurged() {
    addCallback("check_block_purged");
  }

  logging::LoggerGroup logger;
  test_generator generator;
  cn::Block genesisBlock;
  cn::Block lastBlock;
  cn::AccountBase minerAccount;
  std::vector<test_event_entry>& events;
  
  uint32_t height;
};

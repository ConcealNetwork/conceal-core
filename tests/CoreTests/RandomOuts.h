// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once 

#include "Chaingen.h"

struct GetRandomOutputs : public test_chain_unit_base
{
  GetRandomOutputs();

  // bool check_tx_verification_context(const cn::tx_verification_context& tvc, bool tx_added, size_t event_idx, const cn::Transaction& tx);
  // bool check_block_verification_context(const cn::block_verification_context& bvc, size_t event_idx, const cn::Block& block);
  // bool mark_last_valid_block(cn::core& c, size_t ev_index, const std::vector<test_event_entry>& events);

  bool generate(std::vector<test_event_entry>& events) const;


private:

  bool checkHalfUnlocked(cn::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool checkFullyUnlocked(cn::core& c, size_t ev_index, const std::vector<test_event_entry>& events);

  bool request(cn::core& c, uint64_t amount, size_t mixin, cn::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response& resp);

};

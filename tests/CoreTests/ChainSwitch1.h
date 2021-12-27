// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once 
#include "Chaingen.h"

/************************************************************************/
/*                                                                      */
/************************************************************************/
class gen_chain_switch_1 : public test_chain_unit_base
{
public: 
  gen_chain_switch_1();

  bool generate(std::vector<test_event_entry>& events) const;

  bool check_split_not_switched(cn::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_split_switched(cn::core& c, size_t ev_index, const std::vector<test_event_entry>& events);

private:
  std::list<cn::Block> m_chain_1;

  cn::AccountBase m_recipient_account_1;
  cn::AccountBase m_recipient_account_2;
  cn::AccountBase m_recipient_account_3;
  cn::AccountBase m_recipient_account_4;

  std::vector<cn::Transaction> m_tx_pool;
};

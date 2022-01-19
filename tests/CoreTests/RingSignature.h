// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once 
#include "Chaingen.h"

/************************************************************************/
/*                                                                      */
/************************************************************************/
class gen_ring_signature_1 : public test_chain_unit_base
{
public:
  gen_ring_signature_1();

  bool generate(std::vector<test_event_entry>& events) const;

  bool check_balances_1(cn::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_balances_2(cn::core& c, size_t ev_index, const std::vector<test_event_entry>& events);

private:
  cn::AccountBase m_bob_account;
  cn::AccountBase m_alice_account;
};


/************************************************************************/
/*                                                                      */
/************************************************************************/
class gen_ring_signature_2 : public test_chain_unit_base
{
public:
  gen_ring_signature_2();

  bool generate(std::vector<test_event_entry>& events) const;

  bool check_balances_1(cn::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_balances_2(cn::core& c, size_t ev_index, const std::vector<test_event_entry>& events);

private:
  cn::AccountBase m_bob_account;
  cn::AccountBase m_alice_account;
};


/************************************************************************/
/*                                                                      */
/************************************************************************/
class gen_ring_signature_big : public test_chain_unit_base
{
public:
  gen_ring_signature_big();

  bool generate(std::vector<test_event_entry>& events) const;

  bool check_balances_1(cn::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_balances_2(cn::core& c, size_t ev_index, const std::vector<test_event_entry>& events);

private:
  size_t m_test_size;
  uint64_t m_tx_amount;

  cn::AccountBase m_bob_account;
  cn::AccountBase m_alice_account;
};

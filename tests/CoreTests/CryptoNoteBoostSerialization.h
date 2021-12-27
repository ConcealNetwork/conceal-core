// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2016 SDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/variant.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/map.hpp>
#include <boost/foreach.hpp>
#include <boost/serialization/is_bitwise_serializable.hpp>
#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "UnorderedContainersBoostSerialization.h"
#include "crypto/crypto.h"

//namespace cn {
namespace boost
{
  namespace serialization
  {

  //---------------------------------------------------
  template <class Archive>
  inline void serialize(Archive &a, crypto::PublicKey &x, const boost::serialization::version_type ver)
  {
    a & reinterpret_cast<char (&)[sizeof(crypto::PublicKey)]>(x);
  }
  template <class Archive>
  inline void serialize(Archive &a, crypto::SecretKey &x, const boost::serialization::version_type ver)
  {
    a & reinterpret_cast<char (&)[sizeof(crypto::SecretKey)]>(x);
  }
  template <class Archive>
  inline void serialize(Archive &a, crypto::KeyDerivation &x, const boost::serialization::version_type ver)
  {
    a & reinterpret_cast<char (&)[sizeof(crypto::KeyDerivation)]>(x);
  }
  template <class Archive>
  inline void serialize(Archive &a, crypto::KeyImage &x, const boost::serialization::version_type ver)
  {
    a & reinterpret_cast<char (&)[sizeof(crypto::KeyImage)]>(x);
  }

  template <class Archive>
  inline void serialize(Archive &a, crypto::Signature &x, const boost::serialization::version_type ver)
  {
    a & reinterpret_cast<char (&)[sizeof(crypto::Signature)]>(x);
  }
  template <class Archive>
  inline void serialize(Archive &a, crypto::Hash &x, const boost::serialization::version_type ver)
  {
    a & reinterpret_cast<char (&)[sizeof(crypto::Hash)]>(x);
  }
  
  template <class Archive> void serialize(Archive& archive, cn::MultisignatureInput &output, unsigned int version) {
    archive & output.amount;
    archive & output.signatureCount;
    archive & output.outputIndex;
  }

  template <class Archive> void serialize(Archive& archive, cn::MultisignatureOutput &output, unsigned int version) {
    archive & output.keys;
    archive & output.requiredSignatureCount;
  }

  template <class Archive>
  inline void serialize(Archive &a, cn::KeyOutput &x, const boost::serialization::version_type ver)
  {
    a & x.key;
  }

  template <class Archive>
  inline void serialize(Archive &a, cn::BaseInput &x, const boost::serialization::version_type ver)
  {
    a & x.blockIndex;
  }

  template <class Archive>
  inline void serialize(Archive &a, cn::KeyInput &x, const boost::serialization::version_type ver)
  {
    a & x.amount;
    a & x.outputIndexes;
    a & x.keyImage;
  }

  template <class Archive>
  inline void serialize(Archive &a, cn::TransactionOutput &x, const boost::serialization::version_type ver)
  {
    a & x.amount;
    a & x.target;
  }


  template <class Archive>
  inline void serialize(Archive &a, cn::Transaction &x, const boost::serialization::version_type ver)
  {
    a & x.version;
    a & x.unlockTime;
    a & x.inputs;
    a & x.outputs;
    a & x.extra;
    a & x.signatures;
  }


  template <class Archive>
  inline void serialize(Archive &a, cn::Block &b, const boost::serialization::version_type ver)
  {
    a & b.majorVersion;
    a & b.minorVersion;
    a & b.timestamp;
    a & b.previousBlockHash;
    a & b.nonce;
    //------------------
    a & b.baseTransaction;
    a & b.transactionHashes;
  }
}
}

//}

// Copyright (c) 2014-2017, The Monero Project
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#pragma once
#include "cryptonote_protocol/cryptonote_protocol_defs.h"
#include "cryptonote_core/cryptonote_basic_impl.h"
#include "account.h"
#include "subaddress_index.h"
#include "include_base_utils.h"
#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "crypto/cn_slow_hash.hpp"
#include "ringct/rctOps.h"
#include <unordered_map>

namespace cryptonote
{
  //---------------------------------------------------------------
  void get_transaction_prefix_hash(const transaction_prefix& tx, crypto::hash& h);
  crypto::hash get_transaction_prefix_hash(const transaction_prefix& tx);
  bool parse_and_validate_tx_from_blob(const blobdata& tx_blob, transaction& tx, crypto::hash& tx_hash, crypto::hash& tx_prefix_hash);
  bool parse_and_validate_tx_from_blob(const blobdata& tx_blob, transaction& tx);
  bool construct_miner_tx(size_t height, size_t median_size, uint64_t already_generated_coins, size_t current_block_size, uint64_t fee, const account_public_address &miner_address, transaction& tx, const blobdata& extra_nonce = blobdata(), size_t max_outs = 1, uint8_t hard_fork_version = 1);
  bool encrypt_payment_id(std::string& payment_id, const crypto::public_key& public_key, const crypto::secret_key& secret_key);
  bool decrypt_payment_id(std::string& payment_id, const crypto::public_key& public_key, const crypto::secret_key& secret_key);

  struct tx_source_entry
  {
    typedef std::pair<uint64_t, rct::ctkey> output_entry;

    std::vector<output_entry> outputs;  //index + key + optional ringct commitment
    size_t real_output;                 //index in outputs vector of real output_entry
    crypto::public_key real_out_tx_key; //incoming real tx public key
    std::vector<crypto::public_key> real_out_additional_tx_keys; //incoming real tx additional public keys
    size_t real_output_in_tx_index;     //index in transaction outputs vector
    uint64_t amount;                    //money
    bool rct;                           //true if the output is rct
    rct::key mask;                      //ringct amount mask

    void push_output(uint64_t idx, const crypto::public_key &k, uint64_t amount) { outputs.push_back(std::make_pair(idx, rct::ctkey({rct::pk2rct(k), rct::zeroCommit(amount)}))); }

    BEGIN_SERIALIZE_OBJECT()
      FIELD(outputs)
      VARINT_FIELD(real_output)
      FIELD(real_out_tx_key)
      VARINT_FIELD(real_output_in_tx_index)
      VARINT_FIELD(amount)
      FIELD(rct)
      FIELD(mask)
    END_SERIALIZE()
  };

  struct tx_destination_entry
  {
    uint64_t amount;                    //money
    account_public_address addr;        //destination address
    bool is_subaddress;

    tx_destination_entry() : amount(0), addr(AUTO_VAL_INIT(addr)), is_subaddress(false) { }
    tx_destination_entry(uint64_t a, const account_public_address &ad, bool is_subaddress) : amount(a), addr(ad), is_subaddress(is_subaddress) { }

    BEGIN_SERIALIZE_OBJECT()
      VARINT_FIELD(amount)
      FIELD(addr)
      FIELD(is_subaddress)
    END_SERIALIZE()
  };

  //---------------------------------------------------------------
  bool construct_tx(const account_keys& sender_account_keys, const std::vector<tx_source_entry>& sources, const std::vector<tx_destination_entry>& destinations, std::vector<uint8_t> extra, transaction& tx, uint64_t unlock_time);
  bool construct_tx_and_get_tx_key(const account_keys& sender_account_keys, const std::unordered_map<crypto::public_key, subaddress_index>& subaddresses, const std::vector<tx_source_entry>& sources, const std::vector<tx_destination_entry>& destinations, const cryptonote::account_public_address& change_addr, std::vector<uint8_t> extra, transaction& tx, uint64_t unlock_time, crypto::secret_key &tx_key, std::vector<crypto::secret_key> &additional_tx_keys, bool rct = true);

  template<typename T>
  bool find_tx_extra_field_by_type(const std::vector<tx_extra_field>& tx_extra_fields, T& field, size_t index = 0)
  {
    auto it = std::find_if(tx_extra_fields.begin(), tx_extra_fields.end(), [&index](const tx_extra_field& f) { return typeid(T) == f.type() && !index--; });
    if (it == tx_extra_fields.end())
      return false;

    field = boost::get<T>(*it);
    return true;
  }

  bool parse_tx_extra(const std::vector<uint8_t>& tx_extra, std::vector<tx_extra_field>& tx_extra_fields);
  crypto::public_key get_tx_pub_key_from_extra(const std::vector<uint8_t>& tx_extra, size_t pk_index = 0);
  crypto::public_key get_tx_pub_key_from_extra(const transaction_prefix& tx, size_t pk_index = 0);
  crypto::public_key get_tx_pub_key_from_extra(const transaction& tx, size_t pk_index = 0);
  bool add_tx_pub_key_to_extra(transaction& tx, const crypto::public_key& tx_pub_key);
  std::vector<crypto::public_key> get_additional_tx_pub_keys_from_extra(const std::vector<uint8_t>& tx_extra);
  std::vector<crypto::public_key> get_additional_tx_pub_keys_from_extra(const transaction_prefix& tx);
  bool add_additional_tx_pub_keys_to_extra(std::vector<uint8_t>& tx_extra, const std::vector<crypto::public_key>& additional_pub_keys);
  bool add_extra_nonce_to_tx_extra(std::vector<uint8_t>& tx_extra, const blobdata& extra_nonce);
  bool remove_field_from_tx_extra(std::vector<uint8_t>& tx_extra, const std::type_info &type);
  void set_payment_id_to_tx_extra_nonce(blobdata& extra_nonce, const std::string& payment_id);
  void set_encrypted_payment_id_to_tx_extra_nonce(blobdata& extra_nonce, const std::string& payment_id);
  bool get_payment_id_from_tx_extra_nonce(const blobdata& extra_nonce, std::string& payment_id);
  bool get_encrypted_payment_id_from_tx_extra_nonce(const blobdata& extra_nonce, std::string& payment_id);
  void convert_alias(std::string& alias);
  bool is_out_to_acc(const account_keys& acc, const txout_to_key& out_key, const crypto::public_key& tx_pub_key, const std::vector<crypto::public_key>& additional_tx_pub_keys, size_t output_index);
  struct subaddress_receive_info
  {
    subaddress_index index;
    crypto::key_derivation derivation;
  };
  boost::optional<subaddress_receive_info> is_out_to_acc_precomp(const std::unordered_map<crypto::public_key, subaddress_index>& subaddresses, const crypto::public_key& out_key, const crypto::key_derivation& derivation, const std::vector<crypto::key_derivation>& additional_derivations, size_t output_index);

  bool lookup_acc_outs(const account_keys& acc, const transaction& tx, const crypto::public_key& tx_pub_key, const std::vector<crypto::public_key>& additional_tx_pub_keys, std::vector<size_t>& outs, uint64_t& money_transfered);
  bool lookup_acc_outs(const account_keys& acc, const transaction& tx, std::vector<size_t>& outs, uint64_t& money_transfered);
  bool get_tx_fee(const transaction& tx, uint64_t & fee);
  uint64_t get_tx_fee(const transaction& tx);
  crypto::secret_key get_subaddress_secret_key(const crypto::secret_key& a, const subaddress_index& index);
  std::vector<crypto::public_key> get_subaddress_spend_public_keys(const cryptonote::account_keys &keys, uint32_t account, uint32_t begin, uint32_t end);
  bool generate_key_image_helper(const account_keys& ack, const std::unordered_map<crypto::public_key, subaddress_index>& subaddresses, const crypto::public_key& out_key, const crypto::public_key& tx_public_key, const std::vector<crypto::public_key>& additional_tx_public_keys, size_t real_output_index, keypair& in_ephemeral, crypto::key_image& ki);
  bool generate_key_image_helper_precomp(const account_keys& ack, const crypto::public_key& out_key, const crypto::key_derivation& recv_derivation, size_t real_output_index, const subaddress_index& received_index, keypair& in_ephemeral, crypto::key_image& ki);
  void get_blob_hash(const blobdata& blob, crypto::hash& res);
  crypto::hash get_blob_hash(const blobdata& blob);
  std::string short_hash_str(const crypto::hash& h);

  crypto::hash get_transaction_hash(const transaction& t);
  bool get_transaction_hash(const transaction& t, crypto::hash& res);
  bool get_transaction_hash(const transaction& t, crypto::hash& res, size_t& blob_size);
  bool get_transaction_hash(const transaction& t, crypto::hash& res, size_t* blob_size);
  blobdata get_block_hashing_blob(const block& b);
  bool get_block_hash(const block& b, crypto::hash& res);
  std::string get_genesis_tx_hex();
  crypto::hash get_block_hash(const block& b);
  bool get_block_longhash(const block& b, cn_pow_hash_v2 &ctx, crypto::hash& res);
  bool generate_genesis_block(
      block& bl
    , std::string const & genesis_tx
    , uint32_t nonce
    );
  bool parse_and_validate_block_from_blob(const blobdata& b_blob, block& b);
  bool get_inputs_money_amount(const transaction& tx, uint64_t& money);
  uint64_t get_outs_money_amount(const transaction& tx);
  bool check_inputs_types_supported(const transaction& tx);
  bool check_outs_valid(const transaction& tx);
  bool parse_amount(uint64_t& amount, const std::string& str_amount);

  bool check_money_overflow(const transaction& tx);
  bool check_outs_overflow(const transaction& tx);
  bool check_inputs_overflow(const transaction& tx);
  uint64_t get_block_height(const block& b);
  std::vector<uint64_t> relative_output_offsets_to_absolute(const std::vector<uint64_t>& off);
  std::vector<uint64_t> absolute_output_offsets_to_relative(const std::vector<uint64_t>& off);
  std::string print_money(uint64_t amount);
  //---------------------------------------------------------------
  template<class t_object>
  bool t_serializable_object_to_blob(const t_object& to, blobdata& b_blob)
  {
    std::stringstream ss;
    binary_archive<true> ba(ss);
    bool r = ::serialization::serialize(ba, const_cast<t_object&>(to));
    b_blob = ss.str();
    return r;
  }
  //---------------------------------------------------------------
  template<class t_object>
  blobdata t_serializable_object_to_blob(const t_object& to)
  {
    blobdata b;
    t_serializable_object_to_blob(to, b);
    return b;
  }
  //---------------------------------------------------------------
  template<class t_object>
  bool get_object_hash(const t_object& o, crypto::hash& res)
  {
    get_blob_hash(t_serializable_object_to_blob(o), res);
    return true;
  }
  //---------------------------------------------------------------
  template<class t_object>
  size_t get_object_blobsize(const t_object& o)
  {
    blobdata b = t_serializable_object_to_blob(o);
    return b.size();
  }
  //---------------------------------------------------------------
  template<class t_object>
  bool get_object_hash(const t_object& o, crypto::hash& res, size_t& blob_size)
  {
    blobdata bl = t_serializable_object_to_blob(o);
    blob_size = bl.size();
    get_blob_hash(bl, res);
    return true;
  }
  //---------------------------------------------------------------
  template <typename T>
  std::string obj_to_json_str(T& obj)
  {
    std::stringstream ss;
    json_archive<true> ar(ss, true);
    bool r = ::serialization::serialize(ar, obj);
    CHECK_AND_ASSERT_MES(r, "", "obj_to_json_str failed: serialization::serialize returned false");
    return ss.str();
  }
  //---------------------------------------------------------------
  // 62387455827 -> 455827 + 7000000 + 80000000 + 300000000 + 2000000000 + 60000000000, where 455827 <= dust_threshold
  template<typename chunk_handler_t, typename dust_handler_t>
  void decompose_amount_into_digits(uint64_t amount, uint64_t dust_threshold, const chunk_handler_t& chunk_handler, const dust_handler_t& dust_handler)
  {
    if (0 == amount)
    {
      return;
    }

    bool is_dust_handled = false;
    uint64_t dust = 0;
    uint64_t order = 1;
    while (0 != amount)
    {
      uint64_t chunk = (amount % 10) * order;
      amount /= 10;
      order *= 10;

      if (dust + chunk <= dust_threshold)
      {
        dust += chunk;
      }
      else
      {
        if (!is_dust_handled && 0 != dust)
        {
          dust_handler(dust);
          is_dust_handled = true;
        }
        if (0 != chunk)
        {
          chunk_handler(chunk);
        }
      }
    }

    if (!is_dust_handled && 0 != dust)
    {
      dust_handler(dust);
    }
  }
  //---------------------------------------------------------------
  blobdata block_to_blob(const block& b);
  bool block_to_blob(const block& b, blobdata& b_blob);
  blobdata tx_to_blob(const transaction& b);
  bool tx_to_blob(const transaction& b, blobdata& b_blob);
  void get_tx_tree_hash(const std::vector<crypto::hash>& tx_hashes, crypto::hash& h);
  crypto::hash get_tx_tree_hash(const std::vector<crypto::hash>& tx_hashes);
  crypto::hash get_tx_tree_hash(const block& b);
  bool is_valid_decomposed_amount(uint64_t amount);

#define CHECKED_GET_SPECIFIC_VARIANT(variant_var, specific_type, variable_name, fail_return_val) \
  CHECK_AND_ASSERT_MES(variant_var.type() == typeid(specific_type), fail_return_val, "wrong variant type: " << variant_var.type().name() << ", expected " << typeid(specific_type).name()); \
  specific_type& variable_name = boost::get<specific_type>(variant_var);

}

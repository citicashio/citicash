// Copyright (c) 2018, The CitiCash Project
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

#include <unordered_set>
#include "include_base_utils.h"
using namespace epee;

#include "cryptonote_format_utils.h"
#include <boost/foreach.hpp>
#include "cryptonote_config.h"
#include "miner.h"
#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "ringct/rctSigs.h"

#define ENCRYPTED_PAYMENT_ID_TAIL 0x8d

static const uint64_t valid_decomposed_outputs[] = {
  (uint64_t)1, (uint64_t)2, (uint64_t)3, (uint64_t)4, (uint64_t)5, (uint64_t)6, (uint64_t)7, (uint64_t)8, (uint64_t)9, // 1 piconero
  (uint64_t)10, (uint64_t)20, (uint64_t)30, (uint64_t)40, (uint64_t)50, (uint64_t)60, (uint64_t)70, (uint64_t)80, (uint64_t)90,
  (uint64_t)100, (uint64_t)200, (uint64_t)300, (uint64_t)400, (uint64_t)500, (uint64_t)600, (uint64_t)700, (uint64_t)800, (uint64_t)900,
  (uint64_t)1000, (uint64_t)2000, (uint64_t)3000, (uint64_t)4000, (uint64_t)5000, (uint64_t)6000, (uint64_t)7000, (uint64_t)8000, (uint64_t)9000,
  (uint64_t)10000, (uint64_t)20000, (uint64_t)30000, (uint64_t)40000, (uint64_t)50000, (uint64_t)60000, (uint64_t)70000, (uint64_t)80000, (uint64_t)90000,
  (uint64_t)100000, (uint64_t)200000, (uint64_t)300000, (uint64_t)400000, (uint64_t)500000, (uint64_t)600000, (uint64_t)700000, (uint64_t)800000, (uint64_t)900000,
  (uint64_t)1000000, (uint64_t)2000000, (uint64_t)3000000, (uint64_t)4000000, (uint64_t)5000000, (uint64_t)6000000, (uint64_t)7000000, (uint64_t)8000000, (uint64_t)9000000, // 1 micronero
  (uint64_t)10000000, (uint64_t)20000000, (uint64_t)30000000, (uint64_t)40000000, (uint64_t)50000000, (uint64_t)60000000, (uint64_t)70000000, (uint64_t)80000000, (uint64_t)90000000,
  (uint64_t)100000000, (uint64_t)200000000, (uint64_t)300000000, (uint64_t)400000000, (uint64_t)500000000, (uint64_t)600000000, (uint64_t)700000000, (uint64_t)800000000, (uint64_t)900000000,
  (uint64_t)1000000000, (uint64_t)2000000000, (uint64_t)3000000000, (uint64_t)4000000000, (uint64_t)5000000000, (uint64_t)6000000000, (uint64_t)7000000000, (uint64_t)8000000000, (uint64_t)9000000000,
  (uint64_t)10000000000, (uint64_t)20000000000, (uint64_t)30000000000, (uint64_t)40000000000, (uint64_t)50000000000, (uint64_t)60000000000, (uint64_t)70000000000, (uint64_t)80000000000, (uint64_t)90000000000,
  (uint64_t)100000000000, (uint64_t)200000000000, (uint64_t)300000000000, (uint64_t)400000000000, (uint64_t)500000000000, (uint64_t)600000000000, (uint64_t)700000000000, (uint64_t)800000000000, (uint64_t)900000000000,
  (uint64_t)1000000000000, (uint64_t)2000000000000, (uint64_t)3000000000000, (uint64_t)4000000000000, (uint64_t)5000000000000, (uint64_t)6000000000000, (uint64_t)7000000000000, (uint64_t)8000000000000, (uint64_t)9000000000000, // 1 sumo
  (uint64_t)10000000000000, (uint64_t)20000000000000, (uint64_t)30000000000000, (uint64_t)40000000000000, (uint64_t)50000000000000, (uint64_t)60000000000000, (uint64_t)70000000000000, (uint64_t)80000000000000, (uint64_t)90000000000000,
  (uint64_t)100000000000000, (uint64_t)200000000000000, (uint64_t)300000000000000, (uint64_t)400000000000000, (uint64_t)500000000000000, (uint64_t)600000000000000, (uint64_t)700000000000000, (uint64_t)800000000000000, (uint64_t)900000000000000,
  (uint64_t)1000000000000000, (uint64_t)2000000000000000, (uint64_t)3000000000000000, (uint64_t)4000000000000000, (uint64_t)5000000000000000, (uint64_t)6000000000000000, (uint64_t)7000000000000000, (uint64_t)8000000000000000, (uint64_t)9000000000000000,
  (uint64_t)10000000000000000, (uint64_t)20000000000000000, (uint64_t)30000000000000000, (uint64_t)40000000000000000, (uint64_t)50000000000000000, (uint64_t)60000000000000000, (uint64_t)70000000000000000, (uint64_t)80000000000000000, (uint64_t)90000000000000000,
  (uint64_t)100000000000000000, (uint64_t)200000000000000000, (uint64_t)300000000000000000, (uint64_t)400000000000000000, (uint64_t)500000000000000000, (uint64_t)600000000000000000, (uint64_t)700000000000000000, (uint64_t)800000000000000000, (uint64_t)900000000000000000,
  (uint64_t)1000000000000000000, (uint64_t)2000000000000000000, (uint64_t)3000000000000000000, (uint64_t)4000000000000000000, (uint64_t)5000000000000000000, (uint64_t)6000000000000000000, (uint64_t)7000000000000000000, (uint64_t)8000000000000000000, (uint64_t)9000000000000000000, // 1 meganero
  (uint64_t)10000000000000000000ull
};

namespace cryptonote
{
  //---------------------------------------------------------------
  void get_transaction_prefix_hash(const transaction_prefix& tx, crypto::hash& h)
  {
    std::ostringstream s;
    binary_archive<true> a(s);
    ::serialization::serialize(a, const_cast<transaction_prefix&>(tx));
    crypto::cn_fast_hash(s.str().data(), s.str().size(), h);
  }
  //---------------------------------------------------------------
  crypto::hash get_transaction_prefix_hash(const transaction_prefix& tx)
  {
    crypto::hash h = null_hash;
    get_transaction_prefix_hash(tx, h);
    return h;
  }
  //---------------------------------------------------------------
  bool parse_and_validate_tx_from_blob(const blobdata& tx_blob, transaction& tx)
  {
    std::stringstream ss;
    ss << tx_blob;
    binary_archive<false> ba(ss);
    bool r = ::serialization::serialize(ba, tx);
    CHECK_AND_ASSERT_MES(r, false, "Failed to parse transaction from blob");
    return true;
  }
  //---------------------------------------------------------------
  bool parse_and_validate_tx_from_blob(const blobdata& tx_blob, transaction& tx, crypto::hash& tx_hash, crypto::hash& tx_prefix_hash)
  {
    std::stringstream ss;
    ss << tx_blob;
    binary_archive<false> ba(ss);
    bool r = ::serialization::serialize(ba, tx);
    CHECK_AND_ASSERT_MES(r, false, "Failed to parse transaction from blob");
    //TODO: validate tx

    get_transaction_hash(tx, tx_hash);
    get_transaction_prefix_hash(tx, tx_prefix_hash);
    return true;
  }
  //---------------------------------------------------------------
  bool construct_miner_tx(size_t height, size_t median_size, uint64_t already_generated_coins, size_t current_block_size, uint64_t fee, const account_public_address &miner_address, transaction& tx, const blobdata& extra_nonce, size_t max_outs, uint8_t hard_fork_version) {
    tx.vin.clear();
    tx.vout.clear();
    tx.extra.clear();

    keypair txkey = keypair::generate();
    add_tx_pub_key_to_extra(tx, txkey.pub);
    if (!extra_nonce.empty() && !add_extra_nonce_to_tx_extra(tx.extra, extra_nonce))
      return false;

    txin_gen in;
    in.height = height;

    uint64_t block_reward;
    if (!get_block_reward(median_size, current_block_size, already_generated_coins, block_reward, height))
    {
      LOG_PRINT_L0("Block is too big");
      return false;
    }

#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
    LOG_PRINT_L1("Creating block template: reward " << block_reward <<
      ", fee " << fee);
#endif
    block_reward += fee;

    // from hard fork 4, we use a single "dusty" output. This makes the tx even smaller,
    // and avoids the quantization. These outputs will be added as rct outputs with identity
    // masks, to they can be used as rct inputs.
    std::vector<uint64_t> out_amounts;
    decompose_amount_into_digits(block_reward, 0,
      [&out_amounts](uint64_t a_chunk) { out_amounts.push_back(a_chunk); },
      [&out_amounts](uint64_t a_dust) { out_amounts.push_back(a_dust); });

    CHECK_AND_ASSERT_MES(1 <= max_outs, false, "max_out must be non-zero");

    while (max_outs < out_amounts.size())
    {
      out_amounts[1] += out_amounts[0];
      for (size_t n = 1; n < out_amounts.size(); ++n)
        out_amounts[n - 1] = out_amounts[n];
      out_amounts.resize(out_amounts.size() - 1);
    }

    uint64_t summary_amounts = 0;
    for (size_t no = 0; no < out_amounts.size(); no++)
    {
      crypto::key_derivation derivation = AUTO_VAL_INIT(derivation);;
      crypto::public_key out_eph_public_key = AUTO_VAL_INIT(out_eph_public_key);
      bool r = crypto::generate_key_derivation(miner_address.m_view_public_key, txkey.sec, derivation);
      CHECK_AND_ASSERT_MES(r, false, "while creating outs: failed to generate_key_derivation(" << miner_address.m_view_public_key << ", " << txkey.sec << ")");

      r = crypto::derive_public_key(derivation, no, miner_address.m_spend_public_key, out_eph_public_key);
      CHECK_AND_ASSERT_MES(r, false, "while creating outs: failed to derive_public_key(" << derivation << ", " << no << ", "<< miner_address.m_spend_public_key << ")");

      txout_to_key tk;
      tk.key = out_eph_public_key;

      tx_out out;
      summary_amounts += out.amount = out_amounts[no];
      out.target = tk;
      tx.vout.push_back(out);
    }

    CHECK_AND_ASSERT_MES(summary_amounts == block_reward, false, "Failed to construct miner tx, summary_amounts = " << summary_amounts << " not equal block_reward = " << block_reward);

    tx.version = CURRENT_TRANSACTION_VERSION;
    tx.unlock_time = height + CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW;
    tx.vin.push_back(in);
    //LOG_PRINT("MINER_TX generated ok, block_reward=" << print_money(block_reward) << "("  << print_money(block_reward - fee) << "+" << print_money(fee)
    //  << "), current_block_size=" << current_block_size << ", already_generated_coins=" << already_generated_coins << ", tx_id=" << get_transaction_hash(tx), LOG_LEVEL_2);
    return true;
  }
  //---------------------------------------------------------------
  crypto::secret_key get_subaddress_secret_key(const crypto::secret_key& a, const subaddress_index& index)
  {
    const char prefix[] = "SubAddr";
    char data[sizeof(prefix) + sizeof(crypto::secret_key) + sizeof(subaddress_index)];
    memcpy(data, prefix, sizeof(prefix));
    memcpy(data + sizeof(prefix), &a, sizeof(crypto::secret_key));
    memcpy(data + sizeof(prefix) + sizeof(crypto::secret_key), &index, sizeof(subaddress_index));
    crypto::secret_key m;
    crypto::hash_to_scalar(data, sizeof(data), m);
    return m;
  }
  //---------------------------------------------------------------
  std::vector<crypto::public_key> get_subaddress_spend_public_keys(const cryptonote::account_keys &keys, uint32_t account, uint32_t begin, uint32_t end)
  {
    CHECK_AND_ASSERT_THROW_MES(begin <= end, "begin > end");

    std::vector<crypto::public_key> pkeys;
    pkeys.reserve(end - begin);
    cryptonote::subaddress_index index = { account, begin };

    ge_p3 p3;
    ge_cached cached;
    CHECK_AND_ASSERT_THROW_MES(ge_frombytes_vartime(&p3, (const unsigned char*)keys.m_account_address.m_spend_public_key.data) == 0,
      "ge_frombytes_vartime failed to convert spend public key");
    ge_p3_to_cached(&cached, &p3);

    for (uint32_t idx = begin; idx < end; ++idx)
    {
      index.minor = idx;
      if (index.is_zero())
      {
        pkeys.push_back(keys.m_account_address.m_spend_public_key);
        continue;
      }
      const crypto::secret_key m = cryptonote::get_subaddress_secret_key(keys.m_view_secret_key, index);

      // M = m*G
      ge_scalarmult_base(&p3, (const unsigned char*)m.data);

      // D = B + M
      crypto::public_key D;
      ge_p1p1 p1p1;
      ge_add(&p1p1, &p3, &cached);
      ge_p1p1_to_p3(&p3, &p1p1);
      ge_p3_tobytes((unsigned char*)D.data, &p3);

      pkeys.push_back(D);
    }
    return pkeys;
  }
  //---------------------------------------------------------------
  bool generate_key_image_helper(const account_keys& ack, const std::unordered_map<crypto::public_key, subaddress_index>& subaddresses, const crypto::public_key& out_key, const crypto::public_key& tx_public_key, const std::vector<crypto::public_key>& additional_tx_public_keys, size_t real_output_index, keypair& in_ephemeral, crypto::key_image& ki)
  {
    crypto::key_derivation recv_derivation = AUTO_VAL_INIT(recv_derivation);
    bool r = crypto::generate_key_derivation(tx_public_key, ack.m_view_secret_key, recv_derivation);
    CHECK_AND_ASSERT_MES(r, false, "key image helper: failed to generate_key_derivation(" << tx_public_key << ", " << ack.m_view_secret_key << ")");

    std::vector<crypto::key_derivation> additional_recv_derivations;
    for (size_t i = 0; i < additional_tx_public_keys.size(); ++i)
    {
      crypto::key_derivation additional_recv_derivation = AUTO_VAL_INIT(additional_recv_derivation);
      r = crypto::generate_key_derivation(additional_tx_public_keys[i], ack.m_view_secret_key, additional_recv_derivation);
      CHECK_AND_ASSERT_MES(r, false, "key image helper: failed to generate_key_derivation(" << additional_tx_public_keys[i] << ", " << ack.m_view_secret_key << ")");
      additional_recv_derivations.push_back(additional_recv_derivation);
    }

    boost::optional<subaddress_receive_info> subaddr_recv_info = is_out_to_acc_precomp(subaddresses, out_key, recv_derivation, additional_recv_derivations, real_output_index);
    CHECK_AND_ASSERT_MES(subaddr_recv_info, false, "key image helper: given output pubkey doesn't seem to belong to this address");

    return generate_key_image_helper_precomp(ack, out_key, subaddr_recv_info->derivation, real_output_index, subaddr_recv_info->index, in_ephemeral, ki);
  }
  //---------------------------------------------------------------
  bool generate_key_image_helper_precomp(const account_keys& ack, const crypto::public_key& out_key, const crypto::key_derivation& recv_derivation, size_t real_output_index, const subaddress_index& received_index, keypair& in_ephemeral, crypto::key_image& ki)
  {
    if (ack.m_spend_secret_key == null_skey)
    {
      // for watch-only wallet, simply copy the known output pubkey
      in_ephemeral.pub = out_key;
      in_ephemeral.sec = null_skey;
    }
    else
    {
      // derive secret key with subaddress - step 1: original CN derivation
      crypto::secret_key scalar_step1;
      crypto::derive_secret_key(recv_derivation, real_output_index, ack.m_spend_secret_key, scalar_step1); // computes Hs(a*R) + b

      // step 2: add Hs(a || index_major || index_minor)
      crypto::secret_key scalar_step2;
      if (received_index.is_zero())
      {
        scalar_step2 = scalar_step1;    // treat index=(0,0) as a special case representing the main address
      }
      else
      {
        crypto::secret_key m = get_subaddress_secret_key(ack.m_view_secret_key, received_index);
        sc_add((unsigned char*)&scalar_step2, (unsigned char*)&scalar_step1, (unsigned char*)&m);
      }

      in_ephemeral.sec = scalar_step2;
      crypto::secret_key_to_public_key(in_ephemeral.sec, in_ephemeral.pub);
      CHECK_AND_ASSERT_MES(in_ephemeral.pub == out_key, false, "key image helper precomp: given output pubkey doesn't match the derived one");
    }

    crypto::generate_key_image(in_ephemeral.pub, in_ephemeral.sec, ki);
    return true;
  }
  //---------------------------------------------------------------
  uint64_t power_integral(uint64_t a, uint64_t b)
  {
    if(b == 0)
      return 1;
    uint64_t total = a;
    for(uint64_t i = 1; i != b; i++)
      total *= a;
    return total;
  }
  //---------------------------------------------------------------
  bool parse_amount(uint64_t& amount, const std::string& str_amount_)
  {
    std::string str_amount = str_amount_;
    boost::algorithm::trim(str_amount);

    size_t point_index = str_amount.find_first_of('.');
    size_t fraction_size;
    if (std::string::npos != point_index)
    {
      fraction_size = str_amount.size() - point_index - 1;
      while (CRYPTONOTE_DISPLAY_DECIMAL_POINT < fraction_size && '0' == str_amount.back())
      {
        str_amount.erase(str_amount.size() - 1, 1);
        --fraction_size;
      }
      if (CRYPTONOTE_DISPLAY_DECIMAL_POINT < fraction_size)
        return false;
      str_amount.erase(point_index, 1);
    }
    else
    {
      fraction_size = 0;
    }

    if (str_amount.empty())
      return false;

    if (fraction_size < CRYPTONOTE_DISPLAY_DECIMAL_POINT)
    {
      str_amount.append(CRYPTONOTE_DISPLAY_DECIMAL_POINT - fraction_size, '0');
    }

    return string_tools::get_xtype_from_string(amount, str_amount);
  }
  //---------------------------------------------------------------
  bool get_tx_fee(const transaction& tx, uint64_t & fee)
  {
     fee = tx.rct_signatures.txnFee;
     return true;
  }
  //---------------------------------------------------------------
  uint64_t get_tx_fee(const transaction& tx)
  {
    uint64_t r = 0;
    if(!get_tx_fee(tx, r))
      return 0;
    return r;
  }
  //---------------------------------------------------------------
  bool parse_tx_extra(const std::vector<uint8_t>& tx_extra, std::vector<tx_extra_field>& tx_extra_fields)
  {
    tx_extra_fields.clear();

    if(tx_extra.empty())
      return true;

    std::string extra_str(reinterpret_cast<const char*>(tx_extra.data()), tx_extra.size());
    std::istringstream iss(extra_str);
    binary_archive<false> ar(iss);

    bool eof = false;
    while (!eof)
    {
      tx_extra_field field;
      bool r = ::do_serialize(ar, field);
      CHECK_AND_NO_ASSERT_MES_L1(r, false, "failed to deserialize extra field. extra = " << string_tools::buff_to_hex_nodelimer(std::string(reinterpret_cast<const char*>(tx_extra.data()), tx_extra.size())));
      tx_extra_fields.push_back(field);

      std::ios_base::iostate state = iss.rdstate();
      eof = (EOF == iss.peek());
      iss.clear(state);
    }
    CHECK_AND_NO_ASSERT_MES_L1(::serialization::check_stream_state(ar), false, "failed to deserialize extra field. extra = " << string_tools::buff_to_hex_nodelimer(std::string(reinterpret_cast<const char*>(tx_extra.data()), tx_extra.size())));

    return true;
  }
  //---------------------------------------------------------------
  crypto::public_key get_tx_pub_key_from_extra(const std::vector<uint8_t>& tx_extra, size_t pk_index)
  {
    std::vector<tx_extra_field> tx_extra_fields;
    parse_tx_extra(tx_extra, tx_extra_fields);

    tx_extra_pub_key pub_key_field;
    if(!find_tx_extra_field_by_type(tx_extra_fields, pub_key_field, pk_index))
      return null_pkey;

    return pub_key_field.pub_key;
  }
  //---------------------------------------------------------------
  crypto::public_key get_tx_pub_key_from_extra(const transaction_prefix& tx_prefix, size_t pk_index)
  {
    return get_tx_pub_key_from_extra(tx_prefix.extra, pk_index);
  }
  //---------------------------------------------------------------
  crypto::public_key get_tx_pub_key_from_extra(const transaction& tx, size_t pk_index)
  {
    return get_tx_pub_key_from_extra(tx.extra, pk_index);
  }
  //---------------------------------------------------------------
  bool add_tx_pub_key_to_extra(transaction& tx, const crypto::public_key& tx_pub_key)
  {
    tx.extra.resize(tx.extra.size() + 1 + sizeof(crypto::public_key));
    tx.extra[tx.extra.size() - 1 - sizeof(crypto::public_key)] = TX_EXTRA_TAG_PUBKEY;
    *reinterpret_cast<crypto::public_key*>(&tx.extra[tx.extra.size() - sizeof(crypto::public_key)]) = tx_pub_key;
    return true;
  }
  //---------------------------------------------------------------
  std::vector<crypto::public_key> get_additional_tx_pub_keys_from_extra(const std::vector<uint8_t>& tx_extra)
  {
    // parse
    std::vector<tx_extra_field> tx_extra_fields;
    parse_tx_extra(tx_extra, tx_extra_fields);
    // find corresponding field
    tx_extra_additional_pub_keys additional_pub_keys;
    if (!find_tx_extra_field_by_type(tx_extra_fields, additional_pub_keys))
      return{};
    return additional_pub_keys.data;
  }
  //---------------------------------------------------------------
  std::vector<crypto::public_key> get_additional_tx_pub_keys_from_extra(const transaction_prefix& tx)
  {
    return get_additional_tx_pub_keys_from_extra(tx.extra);
  }
  //---------------------------------------------------------------
  bool add_additional_tx_pub_keys_to_extra(std::vector<uint8_t>& tx_extra, const std::vector<crypto::public_key>& additional_pub_keys)
  {
    // convert to variant
    tx_extra_field field = tx_extra_additional_pub_keys{ additional_pub_keys };
    // serialize
    std::ostringstream oss;
    binary_archive<true> ar(oss);
    bool r = ::do_serialize(ar, field);
    CHECK_AND_NO_ASSERT_MES_L1(r, false, "failed to serialize tx extra additional tx pub keys");
    // append
    std::string tx_extra_str = oss.str();
    size_t pos = tx_extra.size();
    tx_extra.resize(tx_extra.size() + tx_extra_str.size());
    memcpy(&tx_extra[pos], tx_extra_str.data(), tx_extra_str.size());
    return true;
  }
  //---------------------------------------------------------------
  bool add_extra_nonce_to_tx_extra(std::vector<uint8_t>& tx_extra, const blobdata& extra_nonce)
  {
    CHECK_AND_ASSERT_MES(extra_nonce.size() <= TX_EXTRA_NONCE_MAX_COUNT, false, "extra nonce could be 255 bytes max");
    size_t start_pos = tx_extra.size();
    tx_extra.resize(tx_extra.size() + 2 + extra_nonce.size());
    //write tag
    tx_extra[start_pos] = TX_EXTRA_NONCE;
    //write len
    ++start_pos;
    tx_extra[start_pos] = static_cast<uint8_t>(extra_nonce.size());
    //write data
    ++start_pos;
    memcpy(&tx_extra[start_pos], extra_nonce.data(), extra_nonce.size());
    return true;
  }
  //---------------------------------------------------------------
  bool remove_field_from_tx_extra(std::vector<uint8_t>& tx_extra, const std::type_info &type)
  {
    std::string extra_str(reinterpret_cast<const char*>(tx_extra.data()), tx_extra.size());
    std::istringstream iss(extra_str);
    binary_archive<false> ar(iss);
    std::ostringstream oss;
    binary_archive<true> newar(oss);

    bool eof = false;
    while (!eof)
    {
      tx_extra_field field;
      bool r = ::do_serialize(ar, field);
      CHECK_AND_NO_ASSERT_MES_L1(r, false, "failed to deserialize extra field. extra = " << string_tools::buff_to_hex_nodelimer(std::string(reinterpret_cast<const char*>(tx_extra.data()), tx_extra.size())));
      if (field.type() != type)
        ::do_serialize(newar, field);

      std::ios_base::iostate state = iss.rdstate();
      eof = (EOF == iss.peek());
      iss.clear(state);
    }
    CHECK_AND_NO_ASSERT_MES_L1(::serialization::check_stream_state(ar), false, "failed to deserialize extra field. extra = " << string_tools::buff_to_hex_nodelimer(std::string(reinterpret_cast<const char*>(tx_extra.data()), tx_extra.size())));
    tx_extra.clear();
    std::string s = oss.str();
    tx_extra.reserve(s.size());
    std::copy(s.begin(), s.end(), std::back_inserter(tx_extra));
    return true;
  }
  
  void set_payment_id_to_tx_extra_nonce(blobdata& extra_nonce, const std::string& payment_id) {
    extra_nonce = (char)TX_EXTRA_NONCE_PAYMENT_ID + payment_id;
  }
  
  void set_encrypted_payment_id_to_tx_extra_nonce(blobdata& extra_nonce, const std::string& payment_id) {
    extra_nonce = (char)TX_EXTRA_NONCE_ENCRYPTED_PAYMENT_ID + payment_id;
  }
  
  bool get_payment_id_from_tx_extra_nonce(const blobdata& extra_nonce, std::string& payment_id) {
    if (extra_nonce.size() > HASH_SIZE+1 || extra_nonce.empty() || extra_nonce[0] != TX_EXTRA_NONCE_PAYMENT_ID)
      return false;
    payment_id = extra_nonce.substr(1);
    return true;
  }
  
  bool get_encrypted_payment_id_from_tx_extra_nonce(const blobdata& extra_nonce, std::string& payment_id) {
    if (extra_nonce.size() > HASH_SIZE+1 || extra_nonce.empty() || extra_nonce[0] != TX_EXTRA_NONCE_ENCRYPTED_PAYMENT_ID)
      return false;
    payment_id = extra_nonce.substr(1);
    return true;
  }

  void convert_alias(std::string& alias) {
    for (char& c : alias) {
      if (c == '-' || c == '_' || c == '.' || c == '@' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z'))
        continue;
      if (c >= 'A' && c <= 'Z') {
        c = std::tolower(c);
        continue;
      }
      alias = "";
      return;
    }
  }

  crypto::public_key get_destination_view_key_pub(const std::vector<tx_destination_entry> &destinations, const account_keys &sender_keys)
  {
    if (destinations.empty()) {
      LOG_ERROR("Destinations are empty");
      return null_pkey;
    }
    for (size_t n = 1; n < destinations.size(); ++n)
    {
      if (!memcmp(&destinations[n].addr, &sender_keys.m_account_address, sizeof(destinations[0].addr)))
        continue;
      if (destinations[n].amount == 0)
        continue;
      if (memcmp(&destinations[n].addr, &destinations[0].addr, sizeof(destinations[0].addr)))
        return null_pkey;
    }
    return destinations[0].addr.m_view_public_key;
  }
  
  bool encrypt_payment_id(std::string& payment_id, const crypto::public_key& public_key, const crypto::secret_key& secret_key) {
    crypto::key_derivation derivation;
    crypto::hash hash;
    char data[HASH_SIZE+1]; /* A hash, and an extra byte */

    if (!generate_key_derivation(public_key, secret_key, derivation))
      return false;

    memcpy(data, &derivation, 32);
    data[32] = ENCRYPTED_PAYMENT_ID_TAIL;
    cn_fast_hash(data, HASH_SIZE+1, hash);

    for (size_t b = 0; b < 32; ++b)
      payment_id[b] ^= hash.data[b];

    return true;
  }
  
  bool decrypt_payment_id(std::string& payment_id, const crypto::public_key& public_key, const crypto::secret_key& secret_key) {
    return encrypt_payment_id(payment_id, public_key, secret_key); // Encryption and decryption are the same operation (xor with a key)
  }
  
  bool construct_tx_and_get_tx_key(const account_keys& sender_account_keys, const std::unordered_map<crypto::public_key, subaddress_index>& subaddresses, const std::vector<tx_source_entry>& sources, const std::vector<tx_destination_entry>& destinations, const cryptonote::account_public_address& change_addr, std::vector<uint8_t> extra, transaction& tx, uint64_t unlock_time, crypto::secret_key &tx_key, std::vector<crypto::secret_key> &additional_tx_keys, bool rct) {
    if (destinations.empty()) {
      LOG_ERROR("The destinations must be non-empty");
      return false;
    }

    std::vector<rct::key> amount_keys;
    tx.set_null();
    amount_keys.clear();

    tx.version = CURRENT_TRANSACTION_VERSION;
    tx.unlock_time = unlock_time;

    tx.extra = extra;
    crypto::public_key txkey_pub;

    keypair txkey = keypair::generate();
    tx_key = txkey.sec;
    
    // if we have a stealth payment id, find it and encrypt it with the tx key now
    std::vector<tx_extra_field> tx_extra_fields;
    if (parse_tx_extra(tx.extra, tx_extra_fields)) {
      tx_extra_nonce extra_nonce;
      if (find_tx_extra_field_by_type(tx_extra_fields, extra_nonce)) {
        std::string payment_id;
        if (get_encrypted_payment_id_from_tx_extra_nonce(extra_nonce.nonce, payment_id)) {
          LOG_PRINT_L2("Encrypting payment id " << payment_id);
          crypto::public_key view_key_pub = get_destination_view_key_pub(destinations, sender_account_keys);
          if (view_key_pub == null_pkey) {
            LOG_ERROR("Destinations have to have exactly one output to support encrypted payment ids");
            return false;
          }

          if (!encrypt_payment_id(payment_id, view_key_pub, tx_key)) {
            LOG_ERROR("Failed to encrypt payment id");
            return false;
          }

          std::string extra_nonce;
          set_encrypted_payment_id_to_tx_extra_nonce(extra_nonce, payment_id);
          remove_field_from_tx_extra(tx.extra, typeid(tx_extra_nonce));
          if (!add_extra_nonce_to_tx_extra(tx.extra, extra_nonce)) {
            LOG_ERROR("Failed to add encrypted payment id to tx extra");
            return false;
          }
          LOG_PRINT_L1("Encrypted payment ID: " << payment_id);
        }
      }
    }
    else {
      LOG_ERROR("Failed to parse tx extra");
      return false;
    }

    struct input_generation_context_data
    {
      keypair in_ephemeral;
    };
    std::vector<input_generation_context_data> in_contexts;

    uint64_t summary_inputs_money = 0;
    //fill inputs
    int idx = -1;
    BOOST_FOREACH(const tx_source_entry& src_entr,  sources)
    {
      ++idx;
      if(src_entr.real_output >= src_entr.outputs.size())
      {
        LOG_ERROR("real_output index (" << src_entr.real_output << ")bigger than output_keys.size()=" << src_entr.outputs.size());
        return false;
      }
      summary_inputs_money += src_entr.amount;

      //key_derivation recv_derivation;
      in_contexts.push_back(input_generation_context_data());
      keypair& in_ephemeral = in_contexts.back().in_ephemeral;
      crypto::key_image img;
      const auto& out_key = reinterpret_cast<const crypto::public_key&>(src_entr.outputs[src_entr.real_output].second.dest);
      if (!generate_key_image_helper(sender_account_keys, subaddresses, out_key, src_entr.real_out_tx_key, src_entr.real_out_additional_tx_keys, src_entr.real_output_in_tx_index, in_ephemeral, img))
      {
        LOG_ERROR("Key image generation failed!");
        return false;
      }

      //check that derivated key is equal with real output key
      if( !(in_ephemeral.pub == src_entr.outputs[src_entr.real_output].second.dest) )
      {
        LOG_ERROR("derived public key mismatch with output public key at index " << idx << ", real out " << src_entr.real_output << "! "<< ENDL << "derived_key:"
          << string_tools::pod_to_hex(in_ephemeral.pub) << ENDL << "real output_public_key:"
          << string_tools::pod_to_hex(src_entr.outputs[src_entr.real_output].second) );
        LOG_ERROR("amount " << src_entr.amount << ", rct " << src_entr.rct);
        LOG_ERROR("tx pubkey " << src_entr.real_out_tx_key << ", real_output_in_tx_index " << src_entr.real_output_in_tx_index);
        return false;
      }

      //put key image into tx input
      txin_to_key input_to_key;
      input_to_key.amount = src_entr.amount;
      input_to_key.k_image = img;

      //fill outputs array and use relative offsets
      BOOST_FOREACH(const tx_source_entry::output_entry& out_entry, src_entr.outputs)
        input_to_key.key_offsets.push_back(out_entry.first);

      input_to_key.key_offsets = absolute_output_offsets_to_relative(input_to_key.key_offsets);
      tx.vin.push_back(input_to_key);
    }

    // "Shuffle" outs
    std::vector<tx_destination_entry> shuffled_dsts(destinations);
    std::sort(shuffled_dsts.begin(), shuffled_dsts.end(), [](const tx_destination_entry& de1, const tx_destination_entry& de2) { return de1.amount < de2.amount; } );

    // figure out if we need to make additional tx pubkeys
    size_t num_stdaddresses = 0;
    size_t num_subaddresses = 0;
    std::unordered_set<cryptonote::account_public_address> unique_dst_addresses;
    account_public_address single_dest_subaddress;
    BOOST_FOREACH(const tx_destination_entry& dst_entr, destinations)
    {
      if (dst_entr.addr == change_addr)
        continue;
      if (unique_dst_addresses.count(dst_entr.addr) == 0)
      {
        unique_dst_addresses.insert(dst_entr.addr);
        if (dst_entr.is_subaddress)
        {
          ++num_subaddresses;
          single_dest_subaddress = dst_entr.addr;
        }
        else
        {
          ++num_stdaddresses;
        }
      }
    }
    LOG_PRINT_L2("destinations include " << num_stdaddresses << " standard addresses and " << num_subaddresses << "subaddresses");

    // if this is a single-destination transfer to a subaddress, we set the tx pubkey to R=s*D
    if (num_stdaddresses == 0 && num_subaddresses == 1)
    {
      txkey_pub = rct::rct2pk(rct::scalarmultKey(rct::pk2rct(single_dest_subaddress.m_spend_public_key), rct::sk2rct(tx_key)));
    }
    else
    {
      txkey_pub = rct::rct2pk(rct::scalarmultBase(rct::sk2rct(tx_key)));
    }
    remove_field_from_tx_extra(tx.extra, typeid(tx_extra_pub_key));
    add_tx_pub_key_to_extra(tx, txkey_pub);


    std::vector<crypto::public_key> additional_tx_public_keys;
    
    // we don't need to include additional tx keys if:
    //   - all the destinations are standard addresses
    //   - there's only one destination which is a subaddress
    bool need_additional_txkeys = num_subaddresses > 0 && (num_stdaddresses > 0 || num_subaddresses > 1);
    if (need_additional_txkeys)
    {
      additional_tx_keys.clear();
      BOOST_FOREACH(const tx_destination_entry& d, destinations){
        additional_tx_keys.push_back(keypair::generate().sec);
      }
      CHECK_AND_ASSERT_MES(destinations.size() == additional_tx_keys.size(), false, "Wrong amount of additional tx keys");
    }
    
    uint64_t summary_outs_money = 0;
    //fill outputs
    size_t output_index = 0;
    BOOST_FOREACH(const tx_destination_entry& dst_entr,  destinations)
    {
      CHECK_AND_ASSERT_MES(dst_entr.amount > 0 || tx.version > 1, false, "Destination with wrong amount: " << dst_entr.amount);
      crypto::key_derivation derivation;
      crypto::public_key out_eph_public_key;

      // make additional tx pubkey if necessary
      keypair additional_txkey;
      if (need_additional_txkeys)
      {
        additional_txkey.sec = additional_tx_keys[output_index];
        if (dst_entr.is_subaddress)
          additional_txkey.pub = rct::rct2pk(rct::scalarmultKey(rct::pk2rct(dst_entr.addr.m_spend_public_key), rct::sk2rct(additional_txkey.sec)));
        else
          additional_txkey.pub = rct::rct2pk(rct::scalarmultBase(rct::sk2rct(additional_txkey.sec)));
      }

      bool r;
      if (dst_entr.addr == change_addr)
      {
        // sending change to yourself; derivation = a*R
        r = crypto::generate_key_derivation(txkey.pub, sender_account_keys.m_view_secret_key, derivation);
        CHECK_AND_ASSERT_MES(r, false, "at creation outs: failed to generate_key_derivation(" << txkey.pub << ", " << sender_account_keys.m_view_secret_key << ")");
      }
      else
      {
        // sending to the recipient; derivation = r*A (or s*C in the subaddress scheme)
        r = crypto::generate_key_derivation(dst_entr.addr.m_view_public_key, dst_entr.is_subaddress && need_additional_txkeys ? additional_txkey.sec : tx_key, derivation);
        CHECK_AND_ASSERT_MES(r, false, "at creation outs: failed to generate_key_derivation(" << dst_entr.addr.m_view_public_key << ", " << (dst_entr.is_subaddress && need_additional_txkeys ? additional_txkey.sec : tx_key) << ")");
      }
      
      if (need_additional_txkeys)
      {
        additional_tx_public_keys.push_back(additional_txkey.pub);
      }

      crypto::secret_key scalar1;
      crypto::derivation_to_scalar(derivation, output_index, scalar1);
      amount_keys.push_back(rct::sk2rct(scalar1));

      r = crypto::derive_public_key(derivation, output_index, dst_entr.addr.m_spend_public_key, out_eph_public_key);
      CHECK_AND_ASSERT_MES(r, false, "at creation outs: failed to derive_public_key(" << derivation << ", " << output_index << ", "<< dst_entr.addr.m_spend_public_key << ")");

      tx_out out;
      out.amount = dst_entr.amount;
      txout_to_key tk;
      tk.key = out_eph_public_key;
      out.target = tk;
      tx.vout.push_back(out);
      output_index++;
      summary_outs_money += dst_entr.amount;
    }
    CHECK_AND_ASSERT_MES(additional_tx_public_keys.size() == additional_tx_keys.size(), false, "Internal error creating additional public keys");

    remove_field_from_tx_extra(tx.extra, typeid(tx_extra_additional_pub_keys));
    
    LOG_PRINT_L2("tx pubkey: " << txkey.pub);
    if (need_additional_txkeys)
    {
      LOG_PRINT_L2("additional tx pubkeys: ");
      for (size_t i = 0; i < additional_tx_public_keys.size(); ++i)
        LOG_PRINT_L2(additional_tx_public_keys[i]);
      add_additional_tx_pub_keys_to_extra(tx.extra, additional_tx_public_keys);
    }

    //check money
    if(summary_outs_money > summary_inputs_money )
    {
      LOG_ERROR("Transaction inputs money ("<< summary_inputs_money << ") less than outputs money (" << summary_outs_money << ")");
      return false;
    }

    // check for watch only wallet
    bool zero_secret_key = true;
    for (size_t i = 0; i < sizeof(sender_account_keys.m_spend_secret_key); ++i)
      zero_secret_key &= (sender_account_keys.m_spend_secret_key.data[i] == 0);
    if (zero_secret_key)
    {
      LOG_PRINT_L1("Null secret key, skipping signatures");
    }


    size_t n_total_outs = sources[0].outputs.size(); // only for non-simple rct

    // the non-simple version is slightly smaller, but assumes all real inputs
    // are on the same index, so can only be used if there just one ring.
    bool use_simple_rct = sources.size() > 1;

    if (!use_simple_rct)
    {
      // non simple ringct requires all real inputs to be at the same index for all inputs
      BOOST_FOREACH(const tx_source_entry& src_entr,  sources)
      {
        if(src_entr.real_output != sources.begin()->real_output)
        {
          LOG_ERROR("All inputs must have the same index for non-simple ringct");
          return false;
        }
      }

      // enforce same mixin for all outputs
      for (size_t i = 1; i < sources.size(); ++i) {
        if (n_total_outs != sources[i].outputs.size()) {
          LOG_ERROR("Non-simple ringct transaction has varying mixin");
          return false;
        }
      }
    }

    uint64_t amount_in = 0, amount_out = 0;
    rct::ctkeyV inSk;
    // mixRing indexing is done the other way round for simple
    rct::ctkeyM mixRing(use_simple_rct ? sources.size() : n_total_outs);
    rct::keyV rct_destinations;
    std::vector<uint64_t> inamounts, outamounts;
    std::vector<unsigned int> index;
    for (size_t i = 0; i < sources.size(); ++i)
    {
      rct::ctkey ctkey;
      amount_in += sources[i].amount;
      inamounts.push_back(sources[i].amount);
      index.push_back(sources[i].real_output);
      // inSk: (secret key, mask)
      ctkey.dest = rct::sk2rct(in_contexts[i].in_ephemeral.sec);
      ctkey.mask = sources[i].mask;
      inSk.push_back(ctkey);
      // inPk: (public key, commitment)
      // will be done when filling in mixRing
    }
    for (size_t i = 0; i < tx.vout.size(); ++i)
    {
      rct_destinations.push_back(rct::pk2rct(boost::get<txout_to_key>(tx.vout[i].target).key));
      outamounts.push_back(tx.vout[i].amount);
      amount_out += tx.vout[i].amount;
    }

    if (use_simple_rct)
    {
      // mixRing indexing is done the other way round for simple
      for (size_t i = 0; i < sources.size(); ++i)
      {
        mixRing[i].resize(sources[i].outputs.size());
        for (size_t n = 0; n < sources[i].outputs.size(); ++n)
        {
          mixRing[i][n] = sources[i].outputs[n].second;
        }
      }
    }
    else
    {
      for (size_t i = 0; i < n_total_outs; ++i) // same index assumption
      {
        mixRing[i].resize(sources.size());
        for (size_t n = 0; n < sources.size(); ++n)
        {
          mixRing[i][n] = sources[n].outputs[i].second;
        }
      }
    }

    // fee
    if (!use_simple_rct && amount_in > amount_out)
      outamounts.push_back(amount_in - amount_out);

    // zero out all amounts to mask rct outputs, real amounts are now encrypted
    for (size_t i = 0; i < tx.vin.size(); ++i)
    {
      if (sources[i].rct)
        boost::get<txin_to_key>(tx.vin[i]).amount = 0;
    }
    for (size_t i = 0; i < tx.vout.size(); ++i)
      tx.vout[i].amount = 0;

    crypto::hash tx_prefix_hash;
    get_transaction_prefix_hash(tx, tx_prefix_hash);
    rct::ctkeyV outSk;
    if (use_simple_rct)
      tx.rct_signatures = rct::genRctSimple(rct::hash2rct(tx_prefix_hash), inSk, rct_destinations, inamounts, outamounts, amount_in - amount_out, mixRing, amount_keys, index, outSk);
    else
      tx.rct_signatures = rct::genRct(rct::hash2rct(tx_prefix_hash), inSk, rct_destinations, outamounts, mixRing, amount_keys, sources[0].real_output, outSk); // same index assumption

    CHECK_AND_ASSERT_MES(tx.vout.size() == outSk.size(), false, "outSk size does not match vout");

    LOG_PRINT2("construct_tx.log", "transaction_created: " << get_transaction_hash(tx) << ENDL << obj_to_json_str(tx) << ENDL, LOG_LEVEL_3);

    return true;
  }
  //---------------------------------------------------------------
  bool construct_tx(const account_keys& sender_account_keys, const std::vector<tx_source_entry>& sources, const std::vector<tx_destination_entry>& destinations, std::vector<uint8_t> extra, transaction& tx, uint64_t unlock_time)
  {
    std::unordered_map<crypto::public_key, cryptonote::subaddress_index> subaddresses;
    subaddresses[sender_account_keys.m_account_address.m_spend_public_key] = { 0, 0 };
    crypto::secret_key tx_key;
    std::vector<crypto::secret_key> additional_tx_keys;
    return construct_tx_and_get_tx_key(sender_account_keys, subaddresses, sources, destinations, destinations.back().addr, extra, tx, unlock_time, tx_key, additional_tx_keys);
  }
  //---------------------------------------------------------------
  bool get_inputs_money_amount(const transaction& tx, uint64_t& money)
  {
    money = 0;
    BOOST_FOREACH(const auto& in, tx.vin)
    {
      CHECKED_GET_SPECIFIC_VARIANT(in, const txin_to_key, tokey_in, false);
      money += tokey_in.amount;
    }
    return true;
  }
  //---------------------------------------------------------------
  uint64_t get_block_height(const block& b)
  {
    CHECK_AND_ASSERT_MES(b.miner_tx.vin.size() == 1, 0, "wrong miner tx in block: " << get_block_hash(b) << ", b.miner_tx.vin.size() != 1");
    CHECKED_GET_SPECIFIC_VARIANT(b.miner_tx.vin[0], const txin_gen, coinbase_in, 0);
    return coinbase_in.height;
  }
  //---------------------------------------------------------------
  bool check_inputs_types_supported(const transaction& tx)
  {
    BOOST_FOREACH(const auto& in, tx.vin)
    {
      CHECK_AND_ASSERT_MES(in.type() == typeid(txin_to_key), false, "wrong variant type: "
        << in.type().name() << ", expected " << typeid(txin_to_key).name()
        << ", in transaction id=" << get_transaction_hash(tx));

    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool check_outs_valid(const transaction& tx)
  {
    BOOST_FOREACH(const tx_out& out, tx.vout)
    {
      CHECK_AND_ASSERT_MES(out.target.type() == typeid(txout_to_key), false, "wrong variant type: "
        << out.target.type().name() << ", expected " << typeid(txout_to_key).name()
        << ", in transaction id=" << get_transaction_hash(tx));

      if (tx.version == 1)
      {
        CHECK_AND_NO_ASSERT_MES(0 < out.amount, false, "zero amount output in transaction id=" << get_transaction_hash(tx));
      }

      if(!check_key(boost::get<txout_to_key>(out.target).key))
        return false;
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool check_money_overflow(const transaction& tx)
  {
    return check_inputs_overflow(tx) && check_outs_overflow(tx);
  }
  //---------------------------------------------------------------
  bool check_inputs_overflow(const transaction& tx)
  {
    uint64_t money = 0;
    BOOST_FOREACH(const auto& in, tx.vin)
    {
      CHECKED_GET_SPECIFIC_VARIANT(in, const txin_to_key, tokey_in, false);
      if(money > tokey_in.amount + money)
        return false;
      money += tokey_in.amount;
    }
    return true;
  }
  //---------------------------------------------------------------
  bool check_outs_overflow(const transaction& tx)
  {
    uint64_t money = 0;
    BOOST_FOREACH(const auto& o, tx.vout)
    {
      if(money > o.amount + money)
        return false;
      money += o.amount;
    }
    return true;
  }
  //---------------------------------------------------------------
  uint64_t get_outs_money_amount(const transaction& tx)
  {
    uint64_t outputs_amount = 0;
    BOOST_FOREACH(const auto& o, tx.vout)
      outputs_amount += o.amount;
    return outputs_amount;
  }
  //---------------------------------------------------------------
  std::string short_hash_str(const crypto::hash& h)
  {
    std::string res = string_tools::pod_to_hex(h);
    CHECK_AND_ASSERT_MES(res.size() == 64, res, "wrong hash256 with string_tools::pod_to_hex conversion");
    auto erased_pos = res.erase(8, 48);
    res.insert(8, "....");
    return res;
  }
  //---------------------------------------------------------------
  bool is_out_to_acc(const account_keys& acc, const txout_to_key& out_key, const crypto::public_key& tx_pub_key, const std::vector<crypto::public_key>& additional_tx_pub_keys, size_t output_index)
  {
    crypto::key_derivation derivation;
    generate_key_derivation(tx_pub_key, acc.m_view_secret_key, derivation);
    crypto::public_key pk;
    derive_public_key(derivation, output_index, acc.m_account_address.m_spend_public_key, pk);
    if (pk == out_key.key)
      return true;
    // try additional tx pubkeys if available
    if (!additional_tx_pub_keys.empty())
    {
      CHECK_AND_ASSERT_MES(output_index < additional_tx_pub_keys.size(), false, "wrong number of additional tx pubkeys");
      generate_key_derivation(additional_tx_pub_keys[output_index], acc.m_view_secret_key, derivation);
      derive_public_key(derivation, output_index, acc.m_account_address.m_spend_public_key, pk);
      return pk == out_key.key;
    }
    return false;
  }
  //---------------------------------------------------------------
  boost::optional<subaddress_receive_info> is_out_to_acc_precomp(const std::unordered_map<crypto::public_key, subaddress_index>& subaddresses, const crypto::public_key& out_key, const crypto::key_derivation& derivation, const std::vector<crypto::key_derivation>& additional_derivations, size_t output_index)
  {
    // try the shared tx pubkey
    crypto::public_key subaddress_spendkey;
    derive_subaddress_public_key(out_key, derivation, output_index, subaddress_spendkey);
    auto found = subaddresses.find(subaddress_spendkey);
    if (found != subaddresses.end())
      return subaddress_receive_info{ found->second, derivation };
    // try additional tx pubkeys if available
    if (!additional_derivations.empty())
    {
      CHECK_AND_ASSERT_MES(output_index < additional_derivations.size(), boost::none, "wrong number of additional derivations");
      derive_subaddress_public_key(out_key, additional_derivations[output_index], output_index, subaddress_spendkey);
      found = subaddresses.find(subaddress_spendkey);
      if (found != subaddresses.end())
        return subaddress_receive_info{ found->second, additional_derivations[output_index] };
    }
    return boost::none;
  }
  //---------------------------------------------------------------
  bool lookup_acc_outs(const account_keys& acc, const transaction& tx, std::vector<size_t>& outs, uint64_t& money_transfered)
  {
    crypto::public_key tx_pub_key = get_tx_pub_key_from_extra(tx);
    if (null_pkey == tx_pub_key)
      return false;
    std::vector<crypto::public_key> additional_tx_pub_keys = get_additional_tx_pub_keys_from_extra(tx);
    return lookup_acc_outs(acc, tx, tx_pub_key, additional_tx_pub_keys, outs, money_transfered);
  }
  //---------------------------------------------------------------
  bool lookup_acc_outs(const account_keys& acc, const transaction& tx, const crypto::public_key& tx_pub_key, const std::vector<crypto::public_key>& additional_tx_pub_keys, std::vector<size_t>& outs, uint64_t& money_transfered)
  {
    CHECK_AND_ASSERT_MES(additional_tx_pub_keys.empty() || additional_tx_pub_keys.size() == tx.vout.size(), false, "wrong number of additional pubkeys");
    money_transfered = 0;
    size_t i = 0;
    for (const tx_out& o : tx.vout)
    {
      CHECK_AND_ASSERT_MES(o.target.type() == typeid(txout_to_key), false, "wrong type id in transaction out");
      if (is_out_to_acc(acc, boost::get<txout_to_key>(o.target), tx_pub_key, additional_tx_pub_keys, i))
      {
        outs.push_back(i);
        money_transfered += o.amount;
      }
      i++;
    }
    return true;
  }
  //---------------------------------------------------------------
  void get_blob_hash(const blobdata& blob, crypto::hash& res)
  {
    cn_fast_hash(blob.data(), blob.size(), res);
  }
  //---------------------------------------------------------------
  std::string print_money(uint64_t amount)
  {
    std::string s = std::to_string(amount);
    if(s.size() < CRYPTONOTE_DISPLAY_DECIMAL_POINT+1)
    {
      s.insert(0, CRYPTONOTE_DISPLAY_DECIMAL_POINT+1 - s.size(), '0');
    }
    s.insert(s.size() - CRYPTONOTE_DISPLAY_DECIMAL_POINT, ".");
    return s;
  }
  //---------------------------------------------------------------
  crypto::hash get_blob_hash(const blobdata& blob)
  {
    crypto::hash h = null_hash;
    get_blob_hash(blob, h);
    return h;
  }
  //---------------------------------------------------------------
  crypto::hash get_transaction_hash(const transaction& t)
  {
    crypto::hash h = null_hash;
    get_transaction_hash(t, h, NULL);
    return h;
  }
  //---------------------------------------------------------------
  bool get_transaction_hash(const transaction& t, crypto::hash& res)
  {
    return get_transaction_hash(t, res, NULL);
  }
  //---------------------------------------------------------------
  bool get_transaction_hash(const transaction& t, crypto::hash& res, size_t* blob_size)
  {
    // v2 transactions hash different parts together, than hash the set of those hashes
    crypto::hash hashes[3];

    // prefix
    get_transaction_prefix_hash(t, hashes[0]);

    transaction &tt = const_cast<transaction&>(t);

    // base rct
    {
      std::stringstream ss;
      binary_archive<true> ba(ss);
      const size_t inputs = t.vin.size();
      const size_t outputs = t.vout.size();
      bool r = tt.rct_signatures.serialize_rctsig_base(ba, inputs, outputs);
      CHECK_AND_ASSERT_MES(r, false, "Failed to serialize rct signatures base");
      cryptonote::get_blob_hash(ss.str(), hashes[1]);
    }

    // prunable rct
    if (t.rct_signatures.type == rct::RCTTypeNull)
    {
      hashes[2] = cryptonote::null_hash;
    }
    else
    {
      std::stringstream ss;
      binary_archive<true> ba(ss);
      const size_t inputs = t.vin.size();
      const size_t outputs = t.vout.size();
      const size_t mixin = t.vin.empty() ? 0 : t.vin[0].type() == typeid(txin_to_key) ? boost::get<txin_to_key>(t.vin[0]).key_offsets.size() - 1 : 0;
      bool r = tt.rct_signatures.p.serialize_rctsig_prunable(ba, t.rct_signatures.type, inputs, outputs, mixin);
      CHECK_AND_ASSERT_MES(r, false, "Failed to serialize rct signatures prunable");
      cryptonote::get_blob_hash(ss.str(), hashes[2]);
    }

    // the tx hash is the hash of the 3 hashes
    res = cn_fast_hash(hashes, sizeof(hashes));

    // we still need the size
    if (blob_size)
      *blob_size = get_object_blobsize(t);

    return true;
  }
  //---------------------------------------------------------------
  bool get_transaction_hash(const transaction& t, crypto::hash& res, size_t& blob_size)
  {
    return get_transaction_hash(t, res, &blob_size);
  }
  //---------------------------------------------------------------
  blobdata get_block_hashing_blob(const block& b)
  {
    blobdata blob = t_serializable_object_to_blob(static_cast<block_header>(b));
    crypto::hash tree_root_hash = get_tx_tree_hash(b);
    blob.append(reinterpret_cast<const char*>(&tree_root_hash), sizeof(tree_root_hash));
    blob.append(tools::get_varint_data(b.tx_hashes.size()+1));
    return blob;
  }
  //---------------------------------------------------------------
  bool get_block_hash(const block& b, crypto::hash& res)
  {
    bool hash_result = get_object_hash(get_block_hashing_blob(b), res);

    return hash_result;
  }
  //---------------------------------------------------------------
  crypto::hash get_block_hash(const block& b)
  {
    crypto::hash p = null_hash;
    get_block_hash(b, p);
    return p;
  }
  //---------------------------------------------------------------
  void generate_genesis_tx(transaction& tx) {
	  account_public_address ac = boost::value_initialized<account_public_address>();
	  std::vector<size_t> sz;
	  construct_miner_tx(0, 0, 0, 0, 0, ac, tx); // zero fee in genesis
	  blobdata txb = tx_to_blob(tx);
	  
  }
  //---------------------------------------------------------------
  std::string get_genesis_tx_hex() {
	transaction tx;
	generate_genesis_tx(tx);
	blobdata txb = tx_to_blob(tx);
	std::string hex_tx_represent = string_tools::buff_to_hex_nodelimer(txb);

	return hex_tx_represent;
  }

  bool generate_genesis_block(
      block& bl
    , std::string const & genesis_tx
    , uint32_t nonce
    )
  {

    //genesis block
    bl = boost::value_initialized<block>();

	  account_public_address addr = boost::value_initialized<account_public_address>();
    construct_miner_tx(0, 0, 0, 0, 0, addr, bl.miner_tx); // zero fee in genesis

    std::string genesis_coinbase_tx_hex = config::GENESIS_TX;
    blobdata tx_bl;
    string_tools::parse_hexstr_to_binbuff(genesis_coinbase_tx_hex, tx_bl);
    bool r = parse_and_validate_tx_from_blob(tx_bl, bl.miner_tx);
    CHECK_AND_ASSERT_MES(r, false, "failed to parse coinbase tx from hard coded blob");
    bl.major_version = CURRENT_BLOCK_MAJOR_VERSION;
    bl.minor_version = CURRENT_BLOCK_MINOR_VERSION;
    bl.timestamp = 0;
    bl.nonce = nonce;
    miner::find_nonce_for_given_block(bl, 1, 0);
    return true;
  }
  //---------------------------------------------------------------
  bool get_block_longhash(const block& b, cn_pow_hash_v2 &ctx, crypto::hash& res)
  {
	block b_local = b; //workaround to avoid const errors with do_serialize
	blobdata bd = get_block_hashing_blob(b);
	
	ctx.hash(bd.data(), bd.size(), res.data);

	return true;
  }
  //---------------------------------------------------------------
  std::vector<uint64_t> relative_output_offsets_to_absolute(const std::vector<uint64_t>& off)
  {
    std::vector<uint64_t> res = off;
    for(size_t i = 1; i < res.size(); i++)
      res[i] += res[i-1];
    return res;
  }
  //---------------------------------------------------------------
  std::vector<uint64_t> absolute_output_offsets_to_relative(const std::vector<uint64_t>& off)
  {
    std::vector<uint64_t> res = off;
    if(!off.size())
      return res;
    std::sort(res.begin(), res.end());//just to be sure, actually it is already should be sorted
    for(size_t i = res.size()-1; i != 0; i--)
      res[i] -= res[i-1];

    return res;
  }
  //---------------------------------------------------------------
  bool parse_and_validate_block_from_blob(const blobdata& b_blob, block& b)
  {
    std::stringstream ss;
    ss << b_blob;
    binary_archive<false> ba(ss);
    bool r = ::serialization::serialize(ba, b);
    CHECK_AND_ASSERT_MES(r, false, "Failed to parse block from blob");
    return true;
  }
  //---------------------------------------------------------------
  blobdata block_to_blob(const block& b)
  {
    return t_serializable_object_to_blob(b);
  }
  //---------------------------------------------------------------
  bool block_to_blob(const block& b, blobdata& b_blob)
  {
    return t_serializable_object_to_blob(b, b_blob);
  }
  //---------------------------------------------------------------
  blobdata tx_to_blob(const transaction& tx)
  {
    return t_serializable_object_to_blob(tx);
  }
  //---------------------------------------------------------------
  bool tx_to_blob(const transaction& tx, blobdata& b_blob)
  {
    return t_serializable_object_to_blob(tx, b_blob);
  }
  //---------------------------------------------------------------
  void get_tx_tree_hash(const std::vector<crypto::hash>& tx_hashes, crypto::hash& h)
  {
    tree_hash(tx_hashes.data(), tx_hashes.size(), h);
  }
  //---------------------------------------------------------------
  crypto::hash get_tx_tree_hash(const std::vector<crypto::hash>& tx_hashes)
  {
    crypto::hash h = null_hash;
    get_tx_tree_hash(tx_hashes, h);
    return h;
  }
  //---------------------------------------------------------------
  crypto::hash get_tx_tree_hash(const block& b)
  {
    std::vector<crypto::hash> txs_ids;
    crypto::hash h = null_hash;
    size_t bl_sz = 0;
    get_transaction_hash(b.miner_tx, h, bl_sz);
    txs_ids.push_back(h);
    BOOST_FOREACH(auto& th, b.tx_hashes)
      txs_ids.push_back(th);
    return get_tx_tree_hash(txs_ids);
  }
  //---------------------------------------------------------------
  bool is_valid_decomposed_amount(uint64_t amount)
  {
    const uint64_t *begin = valid_decomposed_outputs;
    const uint64_t *end = valid_decomposed_outputs + sizeof(valid_decomposed_outputs) / sizeof(valid_decomposed_outputs[0]);
    return std::binary_search(begin, end, amount);
  }
  //---------------------------------------------------------------
}

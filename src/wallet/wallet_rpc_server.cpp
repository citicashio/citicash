// Copyright (c) 2018, The CitiCash Project
// Copyright (c) 2017, SUMOKOIN
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
#include <boost/asio/ip/address.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/format.hpp>
#include <cstdint>
#include "include_base_utils.h"
using namespace epee;

#include "wallet_rpc_server.h"
#include "wallet/wallet_args.h"
#include "common/command_line.h"
#include "common/scoped_message_writer.h"
#include "common/i18n.h"
#include "common/util.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/account.h"
#include "wallet_rpc_server_commands_defs.h"
#include "misc_language.h"
#include "string_coding.h"
#include "string_tools.h"
#include "crypto/hash.h"
#include "rpc/rpc_args.h"

namespace
{
  const command_line::arg_descriptor<std::string, true> arg_rpc_bind_port = {"rpc-bind-port", "Sets bind port for server"};
  const command_line::arg_descriptor<bool> arg_disable_rpc_login = {"disable-rpc-login", "Disable HTTP authentication for RPC connections served by this process"};

  constexpr const char default_rpc_username[] = "citicash";
}

namespace tools
{
  const char* wallet_rpc_server::tr(const char* str)
  {
    return i18n_translate(str, "tools::wallet_rpc_server");
  }
  
  wallet_rpc_server::wallet_rpc_server(wallet2& w):m_wallet(w), m_stop(false) {}

  wallet_rpc_server::~wallet_rpc_server() {}
  
  bool wallet_rpc_server::run()
  {
    m_stop = false;
    m_net_server.add_idle_handler([this](){
      try {
        m_wallet.refresh();
      } catch (const std::exception& ex) {
        LOG_ERROR("Exception at while refreshing, what=" << ex.what());
      }
      return true;
    }, 20000);
    m_net_server.add_idle_handler([this](){
      if (m_stop.load(std::memory_order_relaxed))
      {
        send_stop_signal();
        return false;
      }
      return true;
    }, 500);

    //DO NOT START THIS SERVER IN MORE THEN 1 THREADS WITHOUT REFACTORING
    return epee::http_server_impl_base<wallet_rpc_server, connection_context>::run(1, true);
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::init(const boost::program_options::variables_map& vm)
  {
    auto rpc_config = cryptonote::rpc_args::process(vm);
    if (!rpc_config)
      return false;

    boost::optional<epee::net_utils::http::login> http_login{};
    std::string bind_port = command_line::get_arg(vm, arg_rpc_bind_port);
    const bool disable_auth = command_line::get_arg(vm, arg_disable_rpc_login);

    if (disable_auth) {
      if (rpc_config->login) {
        const cryptonote::rpc_args::descriptors arg{};
        LOG_ERROR(tr("Cannot specify --") << arg_disable_rpc_login.name << tr(" and --") << arg.rpc_login.name);
        return false;
      }
    }
    else {// auth enabled
      if (!rpc_config->login) {
        std::array<std::uint8_t, 16> rand_128bit{{}};
        crypto::rand(rand_128bit.size(), rand_128bit.data());
        http_login.emplace(default_rpc_username, string_encoding::base64_encode(rand_128bit.data(), rand_128bit.size()));
      }
      else
        http_login.emplace(std::move(rpc_config->login->username), std::move(rpc_config->login->password).password());
      
      assert(bool(http_login));
      tools::scoped_message_writer(epee::log_space::console_color_white, true) << "rpc-login: " << http_login->username << ":" << http_login->password;
    }

    m_net_server.set_threads_prefix("RPC");
    return epee::http_server_impl_base<wallet_rpc_server, connection_context>::init(
      std::move(bind_port), std::move(rpc_config->bind_ip), std::move(http_login)
    );
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::not_open(epee::json_rpc::error& er)
  {
    er.code = WALLET_RPC_ERROR_CODE_NOT_OPEN;
    er.message = "No wallet file";
    return false;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  void wallet_rpc_server::fill_transfer_entry(tools::wallet_rpc::transfer_entry& entry, const crypto::hash& txid, const std::string& payment_id, const tools::wallet2::payment_details& pd)
  {
    entry.txid = string_tools::pod_to_hex(pd.m_tx_hash);
    entry.payment_id = payment_id;
    entry.alias = pd.m_alias;
    entry.height = pd.m_block_height;
    entry.unlock_height = pd.m_unlock_time;
    entry.timestamp = pd.m_timestamp;
    entry.amount = pd.m_amount;
    entry.fee = 0; // TODO
    entry.note = m_wallet.get_tx_note(pd.m_tx_hash);
    entry.type = "in";
    entry.subaddr_index = pd.m_subaddr_index;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  void wallet_rpc_server::fill_transfer_entry(tools::wallet_rpc::transfer_entry& entry, const crypto::hash& txid, const tools::wallet2::confirmed_transfer_details& pd)
  {
    entry.txid = string_tools::pod_to_hex(txid);
    entry.payment_id = pd.m_payment_id;
    entry.alias = pd.m_alias;
    entry.height = pd.m_block_height;
    entry.unlock_height = pd.m_unlock_time;
    entry.timestamp = pd.m_timestamp;
    entry.fee = pd.m_amount_in - pd.m_amount_out;
    uint64_t change = pd.m_change == (uint64_t)-1 ? 0 : pd.m_change; // change may not be known
    entry.amount = pd.m_amount_in - change - entry.fee;
    entry.note = m_wallet.get_tx_note(txid);

    for (const auto &d : pd.m_dests) {
      entry.destinations.push_back(wallet_rpc::transfer_destination());
      wallet_rpc::transfer_destination &td = entry.destinations.back();
      td.amount = d.amount;
      td.address = get_account_address_as_str(m_wallet.testnet(), pd.m_dest_subaddr, d.addr);
    }

    entry.type = "out";
    entry.subaddr_index = { pd.m_subaddr_account, 0 };
  }
  //------------------------------------------------------------------------------------------------------------------------------
  void wallet_rpc_server::fill_transfer_entry(tools::wallet_rpc::transfer_entry& entry, const crypto::hash& txid, const tools::wallet2::unconfirmed_transfer_details& pd)
  {
    bool is_failed = pd.m_state == tools::wallet2::unconfirmed_transfer_details::failed;
    entry.txid = string_tools::pod_to_hex(txid);
    entry.payment_id = pd.m_payment_id;
    entry.alias = pd.m_alias;
    entry.height = 0;
    entry.unlock_height = pd.m_unlock_time;
    entry.timestamp = pd.m_timestamp;
    entry.fee = pd.m_amount_in - pd.m_amount_out;
    entry.amount = pd.m_amount_in - pd.m_change - entry.fee;
    entry.note = m_wallet.get_tx_note(txid);
    entry.type = is_failed ? "failed" : "pending";
    entry.subaddr_index = { pd.m_subaddr_account, 0 };
  }
  //------------------------------------------------------------------------------------------------------------------------------
  void wallet_rpc_server::fill_transfer_entry(tools::wallet_rpc::transfer_entry& entry, const std::string& payment_id, const tools::wallet2::payment_details& pd)
  {
    entry.txid = string_tools::pod_to_hex(pd.m_tx_hash);
    entry.payment_id = payment_id;
    entry.alias = pd.m_alias;
    entry.height = 0;
    entry.unlock_height = pd.m_unlock_time;
    entry.timestamp = pd.m_timestamp;
    entry.amount = pd.m_amount;
    entry.fee = 0; // TODO
    entry.note = m_wallet.get_tx_note(pd.m_tx_hash);
    entry.type = "pool";
    entry.subaddr_index = pd.m_subaddr_index;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_getbalance(const wallet_rpc::COMMAND_RPC_GET_BALANCE::request& req, wallet_rpc::COMMAND_RPC_GET_BALANCE::response& res, epee::json_rpc::error& er)
  {
    try
    {
      res.balance = m_wallet.balance(req.account_index);
      res.unlocked_balance = m_wallet.unlocked_balance(req.account_index);
      std::map<uint32_t, uint64_t> balance_per_subaddress = m_wallet.balance_per_subaddress(req.account_index);
      std::map<uint32_t, uint64_t> unlocked_balance_per_subaddress = m_wallet.unlocked_balance_per_subaddress(req.account_index);
      std::vector<tools::wallet2::transfer_details> transfers;
      m_wallet.get_transfers(transfers);
      for (const auto& i : balance_per_subaddress)
      {
        wallet_rpc::COMMAND_RPC_GET_BALANCE::per_subaddress_info info;
        info.address_index = i.first;
        cryptonote::subaddress_index index = { req.account_index, info.address_index };
        info.address = m_wallet.get_subaddress_as_str(index);
        info.balance = i.second;
        info.unlocked_balance = unlocked_balance_per_subaddress[i.first];
        info.label = m_wallet.get_subaddress_label(index);
        info.num_unspent_outputs = std::count_if(transfers.begin(), transfers.end(), [&](const tools::wallet2::transfer_details& td) { return !td.m_spent && td.m_subaddr_index == index; });
        res.per_subaddress.push_back(info);
      }
    }
    catch (std::exception& e)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = e.what();
      return false;
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_getaddress(const wallet_rpc::COMMAND_RPC_GET_ADDRESS::request& req, wallet_rpc::COMMAND_RPC_GET_ADDRESS::response& res, epee::json_rpc::error& er)
  {
    try
    {
      res.addresses.resize(m_wallet.get_num_subaddresses(req.account_index));
      tools::wallet2::transfer_container transfers;
      m_wallet.get_transfers(transfers);
      cryptonote::subaddress_index index = { req.account_index, 0 };
      for (; index.minor < m_wallet.get_num_subaddresses(req.account_index); ++index.minor)
      {
        auto& info = res.addresses[index.minor];
        info.address = m_wallet.get_subaddress_as_str(index);
        info.label = m_wallet.get_subaddress_label(index);
        info.address_index = index.minor;
        info.used = std::find_if(transfers.begin(), transfers.end(), [&](const tools::wallet2::transfer_details& td) { return td.m_subaddr_index == index; }) != transfers.end();
      }
      res.address = m_wallet.get_subaddress_as_str({ req.account_index, 0 });
    }
    catch (std::exception& e)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = e.what();
      return false;
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_create_address(const wallet_rpc::COMMAND_RPC_CREATE_ADDRESS::request& req, wallet_rpc::COMMAND_RPC_CREATE_ADDRESS::response& res, epee::json_rpc::error& er)
  {
    if (!&m_wallet) return not_open(er);
    m_wallet.add_subaddress(req.account_index, req.label);
    res.address_index = m_wallet.get_num_subaddresses(req.account_index) - 1;
    res.address = m_wallet.get_subaddress_as_str({ req.account_index, res.address_index });
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_label_address(const wallet_rpc::COMMAND_RPC_LABEL_ADDRESS::request& req, wallet_rpc::COMMAND_RPC_LABEL_ADDRESS::response& res, epee::json_rpc::error& er)
  {
    if (!&m_wallet) return not_open(er);
    if (req.index.major >= m_wallet.get_num_subaddress_accounts())
    {
      er.code = WALLET_RPC_ERROR_CODE_ACCOUNT_INDEX_OUTOFBOUND;
      er.message = "Account index is out of bound";
      return false;
    }
    if (req.index.minor >= m_wallet.get_num_subaddresses(req.index.major))
    {
      er.code = WALLET_RPC_ERROR_CODE_ADDRESS_INDEX_OUTOFBOUND;
      er.message = "Address index is out of bound";
      return false;
    }
    try
    {
      m_wallet.set_subaddress_label(req.index, req.label);
    }
    catch (const std::exception& e)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = e.what();
      return false;
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_get_accounts(const wallet_rpc::COMMAND_RPC_GET_ACCOUNTS::request& req, wallet_rpc::COMMAND_RPC_GET_ACCOUNTS::response& res, epee::json_rpc::error& er)
  {
    if (!&m_wallet) return not_open(er);
    try
    {
      res.total_balance = 0;
      res.total_unlocked_balance = 0;
      cryptonote::subaddress_index subaddr_index = { 0,0 };
      for (; subaddr_index.major < m_wallet.get_num_subaddress_accounts(); ++subaddr_index.major)
      {
        wallet_rpc::COMMAND_RPC_GET_ACCOUNTS::subaddress_account_info info;
        info.account_index = subaddr_index.major;
        info.base_address = m_wallet.get_subaddress_as_str(subaddr_index);
        info.balance = m_wallet.balance(subaddr_index.major);
        info.unlocked_balance = m_wallet.unlocked_balance(subaddr_index.major);
        info.label = m_wallet.get_subaddress_label(subaddr_index);
        res.subaddress_accounts.push_back(info);
        res.total_balance += info.balance;
        res.total_unlocked_balance += info.unlocked_balance;
      }
    }
    catch (const std::exception& e)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = e.what();
      return false;
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_create_account(const wallet_rpc::COMMAND_RPC_CREATE_ACCOUNT::request& req, wallet_rpc::COMMAND_RPC_CREATE_ACCOUNT::response& res, epee::json_rpc::error& er)
  {
    if (!&m_wallet) return not_open(er);
    try
    {
      m_wallet.add_subaddress_account(req.label);
      res.account_index = m_wallet.get_num_subaddress_accounts() - 1;
      res.address = m_wallet.get_subaddress_as_str({ res.account_index, 0 });
    }
    catch (const std::exception& e)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = e.what();
      return false;
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_label_account(const wallet_rpc::COMMAND_RPC_LABEL_ACCOUNT::request& req, wallet_rpc::COMMAND_RPC_LABEL_ACCOUNT::response& res, epee::json_rpc::error& er)
  {
    if (!&m_wallet) return not_open(er);
    if (req.account_index >= m_wallet.get_num_subaddress_accounts())
    {
      er.code = WALLET_RPC_ERROR_CODE_ACCOUNT_INDEX_OUTOFBOUND;
      er.message = "Account index is out of bound";
      return false;
    }
    try
    {
      m_wallet.set_subaddress_label({ req.account_index, 0 }, req.label);
    }
    catch (const std::exception& e)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = e.what();
      return false;
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_getheight(const wallet_rpc::COMMAND_RPC_GET_HEIGHT::request& req, wallet_rpc::COMMAND_RPC_GET_HEIGHT::response& res, epee::json_rpc::error& er)
  {
    try
    {
      res.height = m_wallet.get_blockchain_current_height();
    }
    catch (std::exception& e)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = e.what();
      return false;
    }
    return true;
  }

  bool wallet_rpc_server::validate_transfer(const std::list<wallet_rpc::transfer_destination>& destinations, const std::string& payment_id, std::vector<cryptonote::tx_destination_entry>& dsts, std::vector<uint8_t>& extra, epee::json_rpc::error& er)
  {
    bool integrated_payment_id_found = false;
    std::string extra_nonce;
    for (auto it = destinations.begin(); it != destinations.end(); it++)
    {
      cryptonote::address_parse_info info;
      cryptonote::tx_destination_entry de;
      if (!get_account_address_from_str_or_url(info, m_wallet.testnet(), it->address, false)) {
        std::string address = m_wallet.get_alias_address(it->address, false);
        if (address.empty() || !get_account_address_from_str(info, m_wallet.testnet(), address)) {
          er.code = WALLET_RPC_ERROR_CODE_WRONG_ADDRESS;
          er.message = std::string("WALLET_RPC_ERROR_CODE_WRONG_ADDRESS: ") + (address.empty() ? it->address : address);
          return false;
        }
      }
      de.addr = info.address;
      de.is_subaddress = info.is_subaddress;
      de.amount = it->amount;
      dsts.push_back(de);

      if (info.has_payment_id)
      {
        if (!payment_id.empty() || integrated_payment_id_found)
        {
          er.code = WALLET_RPC_ERROR_CODE_WRONG_PAYMENT_ID;
          er.message = "A single payment id is allowed per transaction";
          return false;
        }

        integrated_payment_id_found = true;
        cryptonote::set_encrypted_payment_id_to_tx_extra_nonce(extra_nonce, info.payment_id);

        /* Append Payment ID data into extra */
        if (!cryptonote::add_extra_nonce_to_tx_extra(extra, extra_nonce)) {
          er.code = WALLET_RPC_ERROR_CODE_WRONG_PAYMENT_ID;
          er.message = "Something went wrong with integrated payment_id.";
          return false;
        }
      }
    }

    if (!payment_id.empty())
    {

      /* Just to clarify */
      const std::string& payment_id_str = payment_id;

      std::string long_payment_id;

      /* Parse payment ID */
      if (wallet2::parse_payment_note(payment_id_str, long_payment_id))
        cryptonote::set_payment_id_to_tx_extra_nonce(extra_nonce, payment_id_str);
      else {
        er.code = WALLET_RPC_ERROR_CODE_WRONG_PAYMENT_ID;
        er.message = "Payment id has invalid format: \"" + payment_id_str + "\", expected 1-32 character string";
        return false;
      }

      /* Append Payment ID data into extra */
      if (!cryptonote::add_extra_nonce_to_tx_extra(extra, extra_nonce)) {
        er.code = WALLET_RPC_ERROR_CODE_WRONG_PAYMENT_ID;
        er.message = "Something went wrong with payment_id. Please check its format: \"" + payment_id_str + "\", expected 1-32 character string";
        return false;
      }

    }
    return true;
  }

  // LUKAS TODO fix duplicate code with CLI
  bool wallet_rpc_server::on_alias_address(wallet_rpc::COMMAND_RPC_ALIAS_ADDRESS::request& req, wallet_rpc::COMMAND_RPC_ALIAS_ADDRESS::response& res, epee::json_rpc::error& er) {
    if (req.alias.empty()) {
      er.code = WALLET_RPC_ERROR_CODE_WRONG_ALIAS;
      er.message = "Missing alias.";
      return false;
    }

    cryptonote::convert_alias(req.alias);
    if (req.alias.empty()) {
      er.code = WALLET_RPC_ERROR_CODE_WRONG_ALIAS;
      er.message = "Alias can contain only a-z (conveting A-Z), 0-9, \'-\', \'_\', \'.\' and \'@\'.";
      return false;
    }

    if (!m_wallet.get_alias_address(req.alias).empty()) {
      er.code = WALLET_RPC_ERROR_CODE_WRONG_ALIAS;
      er.message = "Alias already exists.";
      return false;
    }

    if (m_wallet.restricted()) {
      er.code = WALLET_RPC_ERROR_CODE_DENIED;
      er.message = "Command unavailable in restricted mode.";
      return false;
    }

    cryptonote::tx_destination_entry dst;
    dst.addr = m_wallet.get_account().get_keys().m_account_address; // LUKAS TODO consider extending to subaddresses via wallet2::get_subaddress(const cryptonote::subaddress_index& index)
    dst.is_subaddress = false;
    dst.amount = 1;
    
    // append alias, wallet_address and signature into extra
    const std::string wallet_address = m_wallet.get_account().get_public_address_str(m_wallet.testnet()); // LUKAS TODO consider extending to subaddresses via get_subaddress_as_str(const cryptonote::subaddress_index& index)
    std::vector<uint8_t> extra;
    if (!cryptonote::add_extra_nonce_to_tx_extra(extra, (char)TX_EXTRA_NONCE_ALIAS + req.alias)
      || !cryptonote::add_extra_nonce_to_tx_extra(extra, (char)TX_EXTRA_NONCE_ADDRESS + wallet_address)
      || !cryptonote::add_extra_nonce_to_tx_extra(extra, (char)TX_EXTRA_NONCE_SIGNATURE + m_wallet.sign(req.alias))) 
    { // LUKAS TODO consider encrypting dst.addr and signature to chars and use them instead, then decrypt via get_account_address_as_str (or get_subaddress_as_str)
      er.code = WALLET_RPC_ERROR_CODE_WRONG_ALIAS;
      er.message = "Something went wrong with alias. Please check its format: \"" + req.alias + "\"";
      return false;
    }

    try {
      std::vector<wallet2::pending_tx> ptx_vector = m_wallet.create_transactions({dst}, DEFAULT_MIXIN, req.unlock_time, req.priority, extra, 0/*req.account_index, LUKAS TODO prolly subaddress index*/, std::set<uint32_t>()/*LUKAS TODO I didn't check this parameter at all*/, false);
      if (!req.do_not_relay)
        m_wallet.commit_tx(ptx_vector.back());

      // populate response with tx hash
      res.tx_hash = epee::string_tools::pod_to_hex(cryptonote::get_transaction_hash(ptx_vector.back().tx));
      res.fee = ptx_vector.back().fee;
    }
    catch (const tools::error::daemon_busy& e) {
      er.code = WALLET_RPC_ERROR_CODE_DAEMON_IS_BUSY;
      er.message = e.what();
      return false;
    }
    catch (const std::exception& e) {
      er.code = WALLET_RPC_ERROR_CODE_GENERIC_TRANSFER_ERROR;
      er.message = e.what();
      return false;
    }
    catch (...) {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = "WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR";
      return false;
    }
    return true;
  }

  // LUKAS TODO fix duplicate code with CLI
  bool wallet_rpc_server::on_get_aliases(wallet_rpc::COMMAND_RPC_GET_ALIASES::request& req, wallet_rpc::COMMAND_RPC_GET_ALIASES::response& res, epee::json_rpc::error& er) {
    const std::string wallet_address = m_wallet.get_account().get_public_address_str(m_wallet.testnet()); // LUKAS TODO consider extending to subaddresses via get_subaddress_as_str(const cryptonote::subaddress_index& index)
    res.aliases = m_wallet.get_address_aliases(wallet_address);
    return true;
  }

  bool wallet_rpc_server::on_get_alias_address(wallet_rpc::COMMAND_RPC_GET_ALIAS_ADDRESS::request& req, wallet_rpc::COMMAND_RPC_GET_ALIAS_ADDRESS::response& res, epee::json_rpc::error& er) {
    if (req.alias.empty()) {
      er.code = WALLET_RPC_ERROR_CODE_WRONG_ALIAS;
      er.message = "Missing alias.";
      return false;
    }

    cryptonote::convert_alias(req.alias);
    if (req.alias.empty()) {
      er.code = WALLET_RPC_ERROR_CODE_WRONG_ALIAS;
      er.message = "Alias can contain only a-z (conveting A-Z), 0-9, \'-\', \'_\', \'.\' and \'@\'.";
      return false;
    }

    res.address = m_wallet.get_alias_address(req.alias, false);
    return true;
  }

  bool wallet_rpc_server::on_transfer(const wallet_rpc::COMMAND_RPC_TRANSFER::request& req, wallet_rpc::COMMAND_RPC_TRANSFER::response& res, epee::json_rpc::error& er)
  {
    std::vector<cryptonote::tx_destination_entry> dsts;
    std::vector<uint8_t> extra;

    if (m_wallet.restricted())
    {
      er.code = WALLET_RPC_ERROR_CODE_DENIED;
      er.message = "Command unavailable in restricted mode.";
      return false;
    }

    // validate the transfer requested and populate dsts & extra
    if (!validate_transfer(req.destinations, req.payment_id, dsts, extra, er))
      return false;

    try
    {
      uint64_t mixin = req.mixin;
      if (mixin < DEFAULT_MIXIN){
        LOG_PRINT_L1("Requested mixin " << req.mixin << " too low, using " << DEFAULT_MIXIN);
        mixin = DEFAULT_MIXIN;
      }
      else if (mixin > MAX_MIXIN){
        LOG_PRINT_L1("Requested mixin " << req.mixin << " too high, using " << MAX_MIXIN);
        mixin = MAX_MIXIN;
      }
      
      std::vector<wallet2::pending_tx> ptx_vector = m_wallet.create_transactions(dsts, mixin, req.unlock_time, req.priority, extra, req.account_index, req.subaddr_indices, req.trusted_daemon);

      // reject proposed transactions if there are more than one.  see on_transfer_split below.
      if (ptx_vector.size() != 1)
      {
        er.code = WALLET_RPC_ERROR_CODE_GENERIC_TRANSFER_ERROR;
        er.message = "Transaction would be too large.  try /transfer_split.";
        return false;
      }

      if (!req.do_not_relay)
        m_wallet.commit_tx(ptx_vector);

      res.tx_hash = epee::string_tools::pod_to_hex(cryptonote::get_transaction_hash(ptx_vector.back().tx));
      if (req.get_tx_key)
      {
        res.tx_key = epee::string_tools::pod_to_hex(ptx_vector.back().tx_key);
      }
      res.fee = ptx_vector.back().fee;
    }
    catch (const tools::error::daemon_busy& e)
    {
      er.code = WALLET_RPC_ERROR_CODE_DAEMON_IS_BUSY;
      er.message = e.what();
      return false;
    }
    catch (const std::exception& e)
    {
      er.code = WALLET_RPC_ERROR_CODE_GENERIC_TRANSFER_ERROR;
      er.message = e.what();
      return false;
    }
    catch (...)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = "WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR";
      return false;
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_transfer_split(const wallet_rpc::COMMAND_RPC_TRANSFER_SPLIT::request& req, wallet_rpc::COMMAND_RPC_TRANSFER_SPLIT::response& res, epee::json_rpc::error& er, float tx_size_target_factor)
  {
    std::vector<cryptonote::tx_destination_entry> dsts;
    std::vector<uint8_t> extra;

    if (m_wallet.restricted())
    {
      er.code = WALLET_RPC_ERROR_CODE_DENIED;
      er.message = "Command unavailable in restricted mode.";
      return false;
    }

    // validate the transfer requested and populate dsts & extra; RPC_TRANSFER::request and RPC_TRANSFER_SPLIT::request are identical types.
    if (!validate_transfer(req.destinations, req.payment_id, dsts, extra, er))
      return false;

    bool retry = false;
    try
    {
      uint64_t mixin = req.mixin;
      uint64_t ptx_amount;
      if (mixin < DEFAULT_MIXIN){
        LOG_PRINT_L1("Requested mixin " << req.mixin << " too low, using " << DEFAULT_MIXIN);
        mixin = DEFAULT_MIXIN;
      }
      else if (mixin > MAX_MIXIN){
        LOG_PRINT_L1("Requested mixin " << req.mixin << " too high, using " << MAX_MIXIN);
        mixin = MAX_MIXIN;
      }
      
      std::vector<wallet2::pending_tx> ptx_vector;
      ptx_vector = m_wallet.create_transactions(dsts, mixin, req.unlock_time, req.priority, extra, req.account_index, req.subaddr_indices, req.trusted_daemon);

      if (!req.do_not_relay)
        m_wallet.commit_tx(ptx_vector);

      // populate response with tx hashes
      for (auto & ptx : ptx_vector)
      {
        res.tx_hash_list.push_back(epee::string_tools::pod_to_hex(cryptonote::get_transaction_hash(ptx.tx)));
        if (req.get_tx_keys)
        {
          res.tx_key_list.push_back(epee::string_tools::pod_to_hex(ptx.tx_key));
        }
        // Compute amount leaving wallet in tx. By convention dests does not include change outputs
        ptx_amount = 0;
        for (auto & dt : ptx.dests)
          ptx_amount += dt.amount;
        res.amount_list.push_back(ptx_amount);

        res.fee_list.push_back(ptx.fee);
      }

      return true;
    }
    catch (const tools::error::daemon_busy& e)
    {
      er.code = WALLET_RPC_ERROR_CODE_DAEMON_IS_BUSY;
      er.message = e.what();
      return false;
    }
    catch (const tools::error::tx_too_big& e)
    {
      tx_size_target_factor = floorf((tx_size_target_factor * e.tx_size_limit() / get_object_blobsize(e.tx())) * 100) / 100;
      LOG_ERROR( boost::format(tr("constructed tx too big: tx size = %s bytes, limit = %s bytes; retrying with smaller tx_size_target_factor = %s...")) % get_object_blobsize(e.tx()) % e.tx_size_limit() % tx_size_target_factor );
      retry = true;
    }
    catch (const std::exception& e)
    {
      er.code = WALLET_RPC_ERROR_CODE_GENERIC_TRANSFER_ERROR;
      er.message = e.what();
      return false;
    }
    catch (...)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = "WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR";
      return false;
    }

    if (retry){
      on_transfer_split(req, res, er, tx_size_target_factor);
    }

    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_sweep_dust(const wallet_rpc::COMMAND_RPC_SWEEP_DUST::request& req, wallet_rpc::COMMAND_RPC_SWEEP_DUST::response& res, epee::json_rpc::error& er)
  {
    if (m_wallet.restricted())
    {
      er.code = WALLET_RPC_ERROR_CODE_DENIED;
      er.message = "Command unavailable in restricted mode.";
      return false;
    }

    try
    {
      std::vector<wallet2::pending_tx> ptx_vector = m_wallet.create_unmixable_sweep_transactions(req.trusted_daemon);

      if (!req.do_not_relay)
        m_wallet.commit_tx(ptx_vector);

      // populate response with tx hashes
      for (auto & ptx : ptx_vector)
      {
        res.tx_hash_list.push_back(epee::string_tools::pod_to_hex(cryptonote::get_transaction_hash(ptx.tx)));
        if (req.get_tx_keys)
        {
          res.tx_key_list.push_back(epee::string_tools::pod_to_hex(ptx.tx_key));
        }
        res.fee_list.push_back(ptx.fee);
      }

      return true;
    }
    catch (const tools::error::daemon_busy& e)
    {
      er.code = WALLET_RPC_ERROR_CODE_DAEMON_IS_BUSY;
      er.message = e.what();
      return false;
    }
    catch (const std::exception& e)
    {
      er.code = WALLET_RPC_ERROR_CODE_GENERIC_TRANSFER_ERROR;
      er.message = e.what();
      return false;
    }
    catch (...)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = "WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR";
      return false;
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_sweep_all(const wallet_rpc::COMMAND_RPC_SWEEP_ALL::request& req, wallet_rpc::COMMAND_RPC_SWEEP_ALL::response& res, epee::json_rpc::error& er)
  {
    std::vector<cryptonote::tx_destination_entry> dsts;
    std::vector<uint8_t> extra;

    if (m_wallet.restricted())
    {
      er.code = WALLET_RPC_ERROR_CODE_DENIED;
      er.message = "Command unavailable in restricted mode.";
      return false;
    }

    // validate the transfer requested and populate dsts & extra
    std::list<wallet_rpc::transfer_destination> destination;
    destination.push_back(wallet_rpc::transfer_destination());
    destination.back().amount = 0;
    destination.back().address = req.address;
    if (!validate_transfer(destination, req.payment_id, dsts, extra, er))
      return false;

    try
    {
      std::vector<wallet2::pending_tx> ptx_vector = m_wallet.create_transactions_all(req.below_amount, dsts[0].addr, req.mixin, req.unlock_time, req.priority, extra, dsts[0].is_subaddress, req.account_index, req.subaddr_indices, req.trusted_daemon);

      if (!req.do_not_relay)
        m_wallet.commit_tx(ptx_vector);

      // populate response with tx hashes
      for (auto & ptx : ptx_vector)
      {
        res.tx_hash_list.push_back(epee::string_tools::pod_to_hex(cryptonote::get_transaction_hash(ptx.tx)));
        if (req.get_tx_keys)
        {
          res.tx_key_list.push_back(epee::string_tools::pod_to_hex(ptx.tx_key));
        }
        res.fee_list.push_back(ptx.fee);
      }

      return true;
    }
    catch (const tools::error::daemon_busy& e)
    {
      er.code = WALLET_RPC_ERROR_CODE_DAEMON_IS_BUSY;
      er.message = e.what();
      return false;
    }
    catch (const std::exception& e)
    {
      er.code = WALLET_RPC_ERROR_CODE_GENERIC_TRANSFER_ERROR;
      er.message = e.what();
      return false;
    }
    catch (...)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = "WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR";
      return false;
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_make_integrated_address(const wallet_rpc::COMMAND_RPC_MAKE_INTEGRATED_ADDRESS::request& req, wallet_rpc::COMMAND_RPC_MAKE_INTEGRATED_ADDRESS::response& res, epee::json_rpc::error& er)
  {
    try
    {
      crypto::hash8 payment_id; // TODO is depreciated? Is this correct
      if (req.payment_id.empty())
      {
        payment_id = crypto::rand<crypto::hash8>();
      }

      if (req.index.major >= m_wallet.get_num_subaddress_accounts())
      {
        er.code = WALLET_RPC_ERROR_CODE_ACCOUNT_INDEX_OUTOFBOUND;
        er.message = "Account index is out of bound";
        return false;
      }
      if (req.index.minor >= m_wallet.get_num_subaddresses(req.index.major))
      {
        er.code = WALLET_RPC_ERROR_CODE_ADDRESS_INDEX_OUTOFBOUND;
        er.message = "Address index is out of bound";
        return false;
      }

      res.integrated_address = m_wallet.get_integrated_subaddress_as_str(req.index, epee::string_tools::pod_to_str(payment_id));
      res.payment_id = epee::string_tools::pod_to_str(payment_id);
      res.label = m_wallet.get_subaddress_label(req.index);
      tools::wallet2::transfer_container transfers;
      m_wallet.get_transfers(transfers);
      res.used = std::find_if(transfers.begin(), transfers.end(), [&](const tools::wallet2::transfer_details& td) { return td.m_subaddr_index == req.index; }) != transfers.end();
      return true;
    }
    catch (std::exception &e)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = e.what();
      return false;
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_split_integrated_address(const wallet_rpc::COMMAND_RPC_SPLIT_INTEGRATED_ADDRESS::request& req, wallet_rpc::COMMAND_RPC_SPLIT_INTEGRATED_ADDRESS::response& res, epee::json_rpc::error& er)
  {
    try
    {
      cryptonote::address_parse_info info;

      if (!get_account_address_from_str(info, m_wallet.testnet(), req.integrated_address))
      {
        er.code = WALLET_RPC_ERROR_CODE_WRONG_ADDRESS;
        er.message = "Invalid address";
        return false;
      }
      if (!info.has_payment_id)
      {
        er.code = WALLET_RPC_ERROR_CODE_WRONG_ADDRESS;
        er.message = "Address is not an integrated address";
        return false;
      }
      res.standard_address = get_account_address_as_str(m_wallet.testnet(), info.is_subaddress, info.address);
      res.payment_id = info.payment_id;
      return true;
    }
    catch (std::exception &e)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = e.what();
      return false;
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_store(const wallet_rpc::COMMAND_RPC_STORE::request& req, wallet_rpc::COMMAND_RPC_STORE::response& res, epee::json_rpc::error& er)
  {
    if (m_wallet.restricted())
    {
      er.code = WALLET_RPC_ERROR_CODE_DENIED;
      er.message = "Command unavailable in restricted mode.";
      return false;
    }

    try
    {
      m_wallet.store();
    }
    catch (std::exception& e)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = e.what();
      return false;
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_get_payments(const wallet_rpc::COMMAND_RPC_GET_PAYMENTS::request& req, wallet_rpc::COMMAND_RPC_GET_PAYMENTS::response& res, epee::json_rpc::error& er)
  {
    if (req.payment_id.size() > HASH_SIZE) {
      er.code = WALLET_RPC_ERROR_CODE_WRONG_PAYMENT_ID;
      er.message = "Payment ID has invalid size: " + req.payment_id;
      return false;
    }

    res.payments.clear();
    std::list<wallet2::payment_details> payment_list;
    m_wallet.get_payments(req.payment_id, payment_list);
    for (auto & payment : payment_list) {
      wallet_rpc::payment_details rpc_payment;
      rpc_payment.payment_id   = req.payment_id;
      rpc_payment.tx_hash      = epee::string_tools::pod_to_hex(payment.m_tx_hash);
      rpc_payment.amount       = payment.m_amount;
      rpc_payment.block_height = payment.m_block_height;
      rpc_payment.unlock_height  = payment.m_unlock_time;
      rpc_payment.subaddr_index = payment.m_subaddr_index;
      res.payments.push_back(rpc_payment);
    }

    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_get_bulk_payments(const wallet_rpc::COMMAND_RPC_GET_BULK_PAYMENTS::request& req, wallet_rpc::COMMAND_RPC_GET_BULK_PAYMENTS::response& res, epee::json_rpc::error& er)
  {
    res.payments.clear();

    /* If the payment ID list is empty, we get payments to any payment ID (or lack thereof) */
    if (req.payment_ids.empty())
    {
      std::list<std::pair<std::string, wallet2::payment_details>> payment_list;
      m_wallet.get_payments(payment_list, req.min_block_height);

      for (auto& payment : payment_list) {
        wallet_rpc::payment_details rpc_payment;
        rpc_payment.payment_id   = payment.first;
        rpc_payment.tx_hash      = epee::string_tools::pod_to_hex(payment.second.m_tx_hash);
        rpc_payment.amount       = payment.second.m_amount;
        rpc_payment.block_height = payment.second.m_block_height;
        rpc_payment.unlock_height  = payment.second.m_unlock_time;
        rpc_payment.subaddr_index = payment.second.m_subaddr_index;
        res.payments.push_back(std::move(rpc_payment));
      }

      return true;
    }

    for (auto& payment_id_str : req.payment_ids) {
      // TODO - should the whole thing fail because of one bad id?

      if (payment_id_str.size() > HASH_SIZE) {
        er.code = WALLET_RPC_ERROR_CODE_WRONG_PAYMENT_ID;
        er.message = "Payment ID has invalid size: " + payment_id_str;
        return false;
      }

      std::list<wallet2::payment_details> payment_list;
      m_wallet.get_payments(payment_id_str, payment_list, req.min_block_height);

      for (auto& payment : payment_list) {
        wallet_rpc::payment_details rpc_payment;
        rpc_payment.payment_id   = payment_id_str;
        rpc_payment.tx_hash      = epee::string_tools::pod_to_hex(payment.m_tx_hash);
        rpc_payment.amount       = payment.m_amount;
        rpc_payment.block_height = payment.m_block_height;
        rpc_payment.unlock_height  = payment.m_unlock_time;
        rpc_payment.subaddr_index = payment.m_subaddr_index;
        res.payments.push_back(std::move(rpc_payment));
      }
    }

    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_incoming_transfers(const wallet_rpc::COMMAND_RPC_INCOMING_TRANSFERS::request& req, wallet_rpc::COMMAND_RPC_INCOMING_TRANSFERS::response& res, epee::json_rpc::error& er)
  {
    if (!&m_wallet) return not_open(er);
    if (req.transfer_type.compare("all") != 0 && req.transfer_type.compare("available") != 0 && req.transfer_type.compare("unavailable") != 0)
    {
      er.code = WALLET_RPC_ERROR_CODE_TRANSFER_TYPE;
      er.message = "Transfer type must be one of: all, available, or unavailable";
      return false;
    }

    bool filter = false;
    bool available = false;
    if (req.transfer_type.compare("available") == 0)
    {
      filter = true;
      available = true;
    }
    else if (req.transfer_type.compare("unavailable") == 0)
    {
      filter = true;
      available = false;
    }

    wallet2::transfer_container transfers;
    m_wallet.get_transfers(transfers);

    bool transfers_found = false;
    for (const auto& td : transfers)
    {
      if (!filter || available != td.m_spent)
      {
        if (req.account_index != td.m_subaddr_index.major || (!req.subaddr_indices.empty() && req.subaddr_indices.count(td.m_subaddr_index.minor) == 0))
          continue;
        if (!transfers_found)
        {
          transfers_found = true;
        }
        auto txBlob = t_serializable_object_to_blob(td.m_tx);
        wallet_rpc::transfer_details rpc_transfers;
        rpc_transfers.amount = td.amount();
        rpc_transfers.spent = td.m_spent;
        rpc_transfers.global_index = td.m_global_output_index;
        rpc_transfers.tx_hash = epee::string_tools::pod_to_hex(td.m_txid);
        rpc_transfers.tx_size = txBlob.size();
        rpc_transfers.subaddr_index = td.m_subaddr_index.minor;
        res.transfers.push_back(rpc_transfers);
      }
    }

    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_query_key(const wallet_rpc::COMMAND_RPC_QUERY_KEY::request& req, wallet_rpc::COMMAND_RPC_QUERY_KEY::response& res, epee::json_rpc::error& er)
  {
      if (m_wallet.restricted())
      {
        er.code = WALLET_RPC_ERROR_CODE_DENIED;
        er.message = "Command unavailable in restricted mode.";
        return false;
      }

      if (req.key_type.compare("mnemonic") == 0)
      {
        if (!m_wallet.get_seed(res.key))
        {
            er.message = "The wallet is non-deterministic. Cannot display seed.";
            return false;
        }
      }
      else if(req.key_type.compare("view_key") == 0)
      {
          res.key = string_tools::pod_to_hex(m_wallet.get_account().get_keys().m_view_secret_key);
      }
      else if (req.key_type.compare("spend_key") == 0)
      {
          res.key = string_tools::pod_to_hex(m_wallet.get_account().get_keys().m_spend_secret_key);
      }
      else
      {
          er.message = "key_type " + req.key_type + " not found";
          return false;
      }

      return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_rescan_blockchain(const wallet_rpc::COMMAND_RPC_RESCAN_BLOCKCHAIN::request& req, wallet_rpc::COMMAND_RPC_RESCAN_BLOCKCHAIN::response& res, epee::json_rpc::error& er)
  {
    if (m_wallet.restricted())
    {
      er.code = WALLET_RPC_ERROR_CODE_DENIED;
      er.message = "Command unavailable in restricted mode.";
      return false;
    }

    try
    {
      m_wallet.rescan_blockchain();
    }
    catch (std::exception& e)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = e.what();
      return false;
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_sign(const wallet_rpc::COMMAND_RPC_SIGN::request& req, wallet_rpc::COMMAND_RPC_SIGN::response& res, epee::json_rpc::error& er)
  {
    if (m_wallet.restricted())
    {
      er.code = WALLET_RPC_ERROR_CODE_DENIED;
      er.message = "Command unavailable in restricted mode.";
      return false;
    }

    res.signature = m_wallet.sign(req.data);
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_verify(const wallet_rpc::COMMAND_RPC_VERIFY::request& req, wallet_rpc::COMMAND_RPC_VERIFY::response& res, epee::json_rpc::error& er)
  {
    if (m_wallet.restricted())
    {
      er.code = WALLET_RPC_ERROR_CODE_DENIED;
      er.message = "Command unavailable in restricted mode.";
      return false;
    }

    cryptonote::address_parse_info info;
    if (!get_account_address_from_str_or_url(info, m_wallet.testnet(), req.address, false))
    {
      er.code = WALLET_RPC_ERROR_CODE_WRONG_ADDRESS;
      er.message = "";
      return false;
    }

    res.good = m_wallet.verify(req.data, info.address, req.signature);
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_stop_wallet(const wallet_rpc::COMMAND_RPC_STOP_WALLET::request& req, wallet_rpc::COMMAND_RPC_STOP_WALLET::response& res, epee::json_rpc::error& er)
  {
    if (m_wallet.restricted())
    {
      er.code = WALLET_RPC_ERROR_CODE_DENIED;
      er.message = "Command unavailable in restricted mode.";
      return false;
    }

    try
    {
      m_wallet.store();
      m_stop.store(true, std::memory_order_relaxed);
    }
    catch (std::exception& e)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = e.what();
      return false;
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_set_tx_notes(const wallet_rpc::COMMAND_RPC_SET_TX_NOTES::request& req, wallet_rpc::COMMAND_RPC_SET_TX_NOTES::response& res, epee::json_rpc::error& er)
  {
    if (req.txids.size() != req.notes.size())
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = "Different amount of txids and notes";
      return false;
    }

    std::list<crypto::hash> txids;
    std::list<std::string>::const_iterator i = req.txids.begin();
    while (i != req.txids.end())
    {
      cryptonote::blobdata txid_blob;
      if(!epee::string_tools::parse_hexstr_to_binbuff(*i++, txid_blob))
      {
        er.code = WALLET_RPC_ERROR_CODE_WRONG_TXID;
        er.message = "TX ID has invalid format";
        return false;
      }

      crypto::hash txid = *reinterpret_cast<const crypto::hash*>(txid_blob.data());
      txids.push_back(txid);
    }

    std::list<crypto::hash>::const_iterator il = txids.begin();
    std::list<std::string>::const_iterator in = req.notes.begin();
    while (il != txids.end())
    {
      m_wallet.set_tx_note(*il++, *in++);
    }

    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_get_tx_notes(const wallet_rpc::COMMAND_RPC_GET_TX_NOTES::request& req, wallet_rpc::COMMAND_RPC_GET_TX_NOTES::response& res, epee::json_rpc::error& er)
  {
    res.notes.clear();

    std::list<crypto::hash> txids;
    std::list<std::string>::const_iterator i = req.txids.begin();
    while (i != req.txids.end())
    {
      cryptonote::blobdata txid_blob;
      if(!epee::string_tools::parse_hexstr_to_binbuff(*i++, txid_blob))
      {
        er.code = WALLET_RPC_ERROR_CODE_WRONG_TXID;
        er.message = "TX ID has invalid format";
        return false;
      }

      crypto::hash txid = *reinterpret_cast<const crypto::hash*>(txid_blob.data());
      txids.push_back(txid);
    }

    std::list<crypto::hash>::const_iterator il = txids.begin();
    while (il != txids.end())
    {
      res.notes.push_back(m_wallet.get_tx_note(*il++));
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_get_transfers(const wallet_rpc::COMMAND_RPC_GET_TRANSFERS::request& req, wallet_rpc::COMMAND_RPC_GET_TRANSFERS::response& res, epee::json_rpc::error& er)
  {
    if (m_wallet.restricted())
    {
      er.code = WALLET_RPC_ERROR_CODE_DENIED;
      er.message = "Command unavailable in restricted mode.";
      return false;
    }

    uint64_t min_height = 0, max_height = (uint64_t)-1;
    if (req.filter_by_height)
    {
      min_height = req.min_height;
      max_height = req.max_height;
    }
    uint64_t min_timestamp = 0, max_timestamp = (uint64_t)-1;
    if (req.filter_by_timestamp)
    {
      min_timestamp = req.min_timestamp;
      max_timestamp = req.max_timestamp;
    }

    std::list<std::pair<int, tools::wallet_rpc::transfer_entry>> transfer_entries;

    if (req.in) {
      std::list<std::pair<std::string, tools::wallet2::payment_details>> payments;
      m_wallet.get_payments(payments, min_height, max_height, min_timestamp, max_timestamp, req.account_index, req.subaddr_indices);
      for (std::list<std::pair<std::string, tools::wallet2::payment_details>>::const_iterator i = payments.begin(); i != payments.end(); ++i) {
        transfer_entries.emplace_back(0, wallet_rpc::transfer_entry());
        fill_transfer_entry(transfer_entries.back().second, i->second.m_tx_hash, i->first, i->second);
      }
    }

    if (req.out) {
      std::list<std::pair<crypto::hash, tools::wallet2::confirmed_transfer_details>> payments;
      m_wallet.get_payments_out(payments, min_height, max_height, min_timestamp, max_timestamp, req.account_index, req.subaddr_indices);
      for (std::list<std::pair<crypto::hash, tools::wallet2::confirmed_transfer_details>>::const_iterator i = payments.begin(); i != payments.end(); ++i) {
        transfer_entries.emplace_back(1, wallet_rpc::transfer_entry());
        fill_transfer_entry(transfer_entries.back().second, i->first, i->second);
      }
    }

    if (req.pending || req.failed) {
      std::list<std::pair<crypto::hash, tools::wallet2::unconfirmed_transfer_details>> upayments;
      m_wallet.get_unconfirmed_payments_out(upayments, req.account_index, req.subaddr_indices);
      for (std::list<std::pair<crypto::hash, tools::wallet2::unconfirmed_transfer_details>>::const_iterator i = upayments.begin(); i != upayments.end(); ++i) {
        const tools::wallet2::unconfirmed_transfer_details &pd = i->second;
        bool is_failed = pd.m_state == tools::wallet2::unconfirmed_transfer_details::failed;
        if (!((req.failed && is_failed) || (!is_failed && req.pending)))
          continue;
        transfer_entries.emplace_back(is_failed ? 2 : 3, wallet_rpc::transfer_entry());
        fill_transfer_entry(transfer_entries.back().second, i->first, i->second);
      }
    }

    if (req.pool) {
      m_wallet.update_pool_state();

      std::list<std::pair<std::string, tools::wallet2::payment_details>> payments;
      m_wallet.get_unconfirmed_payments(payments, req.account_index, req.subaddr_indices);
      for (std::list<std::pair<std::string, tools::wallet2::payment_details>>::const_iterator i = payments.begin(); i != payments.end(); ++i) {
        transfer_entries.emplace_back(4, wallet_rpc::transfer_entry());
        fill_transfer_entry(transfer_entries.back().second, i->first, i->second);
      }
    }

    res.total_count = transfer_entries.size();

    if (req.pagination) {
      if (req.offset >= transfer_entries.size())
        return true;
      transfer_entries.sort(
        [](const std::pair<int, tools::wallet_rpc::transfer_entry>& first, const std::pair<int, tools::wallet_rpc::transfer_entry>& second) {
          return first.second.timestamp > second.second.timestamp; 
        });
      transfer_entries.assign(std::next(transfer_entries.begin(), req.offset), std::next(transfer_entries.begin(), std::min(req.offset + req.limit, static_cast<uint64_t>(transfer_entries.size()))));
    }
    for (const auto& transfer_entry : transfer_entries)
      switch (transfer_entry.first) {
        case 0: res.in.push_back(transfer_entry.second); // LUKAS TODO consider moving (std::move or something, if possible)
                break;
        case 1: res.out.push_back(transfer_entry.second);
                break;
        case 2: res.failed.push_back(transfer_entry.second);
                break;
        case 3: res.pending.push_back(transfer_entry.second);
                break;
        case 4: res.pool.push_back(transfer_entry.second);
      }

    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_get_transfer_by_txid(const wallet_rpc::COMMAND_RPC_GET_TRANSFER_BY_TXID::request& req, wallet_rpc::COMMAND_RPC_GET_TRANSFER_BY_TXID::response& res, epee::json_rpc::error& er)
  {
    if (!&m_wallet) return not_open(er);
    if (m_wallet.restricted())
    {
      er.code = WALLET_RPC_ERROR_CODE_DENIED;
      er.message = "Command unavailable in restricted mode.";
      return false;
    }

    crypto::hash txid;
    cryptonote::blobdata txid_blob;
    if (!epee::string_tools::parse_hexstr_to_binbuff(req.txid, txid_blob))
    {
      er.code = WALLET_RPC_ERROR_CODE_WRONG_TXID;
      er.message = "Transaction ID has invalid format";
      return false;
    }

    if (sizeof(txid) == txid_blob.size())
    {
      txid = *reinterpret_cast<const crypto::hash*>(txid_blob.data());
    }
    else
    {
      er.code = WALLET_RPC_ERROR_CODE_WRONG_TXID;
      er.message = "Transaction ID has invalid size: " + req.txid;
      return false;
    }

    std::list<std::pair<std::string, tools::wallet2::payment_details>> payments;
    m_wallet.get_payments(payments, 0);
    for (std::list<std::pair<std::string, tools::wallet2::payment_details>>::const_iterator i = payments.begin(); i != payments.end(); ++i) {
      if (i->second.m_tx_hash == txid)
      {
        fill_transfer_entry(res.transfer, i->second.m_tx_hash, i->first, i->second);
        return true;
      }
    }

    std::list<std::pair<crypto::hash, tools::wallet2::confirmed_transfer_details>> payments_out;
    m_wallet.get_payments_out(payments_out, 0);
    for (std::list<std::pair<crypto::hash, tools::wallet2::confirmed_transfer_details>>::const_iterator i = payments_out.begin(); i != payments_out.end(); ++i) {
      if (i->first == txid)
      {
        fill_transfer_entry(res.transfer, i->first, i->second);
        return true;
      }
    }

    std::list<std::pair<crypto::hash, tools::wallet2::unconfirmed_transfer_details>> upayments;
    m_wallet.get_unconfirmed_payments_out(upayments);
    for (std::list<std::pair<crypto::hash, tools::wallet2::unconfirmed_transfer_details>>::const_iterator i = upayments.begin(); i != upayments.end(); ++i) {
      if (i->first == txid)
      {
        fill_transfer_entry(res.transfer, i->first, i->second);
        return true;
      }
    }

    m_wallet.update_pool_state();

    std::list<std::pair<std::string, tools::wallet2::payment_details>> pool_payments;
    m_wallet.get_unconfirmed_payments(pool_payments);
    for (std::list<std::pair<std::string, tools::wallet2::payment_details>>::const_iterator i = pool_payments.begin(); i != pool_payments.end(); ++i) {
      if (i->second.m_tx_hash == txid)
      {
        fill_transfer_entry(res.transfer, i->first, i->second);
        return true;
      }
    }

    er.code = WALLET_RPC_ERROR_CODE_WRONG_TXID;
    er.message = "Transaction not found.";
    return false;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_export_key_images(const wallet_rpc::COMMAND_RPC_EXPORT_KEY_IMAGES::request& req, wallet_rpc::COMMAND_RPC_EXPORT_KEY_IMAGES::response& res, epee::json_rpc::error& er)
  {
    try
    {
      std::vector<std::pair<crypto::key_image, crypto::signature>> ski = m_wallet.export_key_images();
      res.signed_key_images.resize(ski.size());
      for (size_t n = 0; n < ski.size(); ++n)
      {
         res.signed_key_images[n].key_image = epee::string_tools::pod_to_hex(ski[n].first);
         res.signed_key_images[n].signature = epee::string_tools::pod_to_hex(ski[n].second);
      }
    }

    catch (...)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = "Failed";
      return false;
    }

    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_import_key_images(const wallet_rpc::COMMAND_RPC_IMPORT_KEY_IMAGES::request& req, wallet_rpc::COMMAND_RPC_IMPORT_KEY_IMAGES::response& res, epee::json_rpc::error& er)
  {
    try
    {
      std::vector<std::pair<crypto::key_image, crypto::signature>> ski;
      ski.resize(req.signed_key_images.size());
      for (size_t n = 0; n < ski.size(); ++n)
      {
        cryptonote::blobdata bd;

        if(!epee::string_tools::parse_hexstr_to_binbuff(req.signed_key_images[n].key_image, bd))
        {
          er.code = WALLET_RPC_ERROR_CODE_WRONG_KEY_IMAGE;
          er.message = "failed to parse key image";
          return false;
        }
        ski[n].first = *reinterpret_cast<const crypto::key_image*>(bd.data());

        if(!epee::string_tools::parse_hexstr_to_binbuff(req.signed_key_images[n].signature, bd))
        {
          er.code = WALLET_RPC_ERROR_CODE_WRONG_SIGNATURE;
          er.message = "failed to parse signature";
          return false;
        }
        ski[n].second = *reinterpret_cast<const crypto::signature*>(bd.data());
      }
      uint64_t spent = 0, unspent = 0;
      uint64_t height = m_wallet.import_key_images(ski, spent, unspent);
      res.spent = spent;
      res.unspent = unspent;
      res.height = height;
    }

    catch (...)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = "Failed";
      return false;
    }

    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_make_uri(const wallet_rpc::COMMAND_RPC_MAKE_URI::request& req, wallet_rpc::COMMAND_RPC_MAKE_URI::response& res, epee::json_rpc::error& er)
  {
    std::string error;
    std::string uri = m_wallet.make_uri(req.address, req.payment_id, req.amount, req.tx_description, req.recipient_name, error);
    if (uri.empty())
    {
      er.code = WALLET_RPC_ERROR_CODE_WRONG_URI;
      er.message = std::string("Cannot make URI from supplied parameters: ") + error;
      return false;
    }

    res.uri = uri;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_parse_uri(const wallet_rpc::COMMAND_RPC_PARSE_URI::request& req, wallet_rpc::COMMAND_RPC_PARSE_URI::response& res, epee::json_rpc::error& er)
  {
    std::string error;
    if (!m_wallet.parse_uri(req.uri, res.uri.address, res.uri.payment_id, res.uri.amount, res.uri.tx_description, res.uri.recipient_name, res.unknown_parameters, error))
    {
      er.code = WALLET_RPC_ERROR_CODE_WRONG_URI;
      er.message = "Error parsing URI: " + error;
      return false;
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_get_address_book(const wallet_rpc::COMMAND_RPC_GET_ADDRESS_BOOK_ENTRY::request& req, wallet_rpc::COMMAND_RPC_GET_ADDRESS_BOOK_ENTRY::response& res, epee::json_rpc::error& er)
  {
    if (!&m_wallet) return not_open(er);
    const auto ab = m_wallet.get_address_book();
    if (req.entries.empty())
    {
      uint64_t idx = 0;
      for (const auto &entry : ab)
        res.entries.push_back(wallet_rpc::COMMAND_RPC_GET_ADDRESS_BOOK_ENTRY::entry{ idx++, get_account_address_as_str(m_wallet.testnet(), entry.m_is_subaddress, entry.m_address), entry.m_payment_id, entry.m_description });
    }
    else
    {
      for (uint64_t idx : req.entries)
      {
        if (idx >= ab.size())
        {
          er.code = WALLET_RPC_ERROR_CODE_WRONG_INDEX;
          er.message = "Index out of range: " + std::to_string(idx);
          return false;
        }
        const auto &entry = ab[idx];
        res.entries.push_back(wallet_rpc::COMMAND_RPC_GET_ADDRESS_BOOK_ENTRY::entry{ idx, get_account_address_as_str(m_wallet.testnet(), entry.m_is_subaddress, entry.m_address), entry.m_payment_id, entry.m_description });
      }
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_add_address_book(const wallet_rpc::COMMAND_RPC_ADD_ADDRESS_BOOK_ENTRY::request& req, wallet_rpc::COMMAND_RPC_ADD_ADDRESS_BOOK_ENTRY::response& res, epee::json_rpc::error& er)
  {
    if (!&m_wallet) return not_open(er);
    if (m_wallet.restricted())
    {
      er.code = WALLET_RPC_ERROR_CODE_DENIED;
      er.message = "Command unavailable in restricted mode.";
      return false;
    }

    cryptonote::address_parse_info info;
    std::string payment_id;
    if (!get_account_address_from_str_or_url(info, m_wallet.testnet(), req.address, false))
    {
      er.code = WALLET_RPC_ERROR_CODE_WRONG_ADDRESS;
      er.message = std::string("WALLET_RPC_ERROR_CODE_WRONG_ADDRESS: ") + req.address;
      return false;
    }
    if (info.has_payment_id)
      payment_id = info.payment_id;
    if (!req.payment_id.empty()) {
      if (info.has_payment_id) {
        er.code = WALLET_RPC_ERROR_CODE_WRONG_PAYMENT_ID;
        er.message = "Separate payment ID given with integrated address";
        return false;
      }

      if (!wallet2::parse_payment_note(req.payment_id, payment_id))
      {
          er.code = WALLET_RPC_ERROR_CODE_WRONG_PAYMENT_ID;
          er.message = "Payment id has invalid format: \"" + req.payment_id + "\", expected 16 or 64 character string";
          return false;
      }
    }
    if (!m_wallet.add_address_book_row(info.address, payment_id, req.description, info.is_subaddress))
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = "Failed to add address book entry";
      return false;
    }
    res.index = m_wallet.get_address_book().size() - 1;
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_delete_address_book(const wallet_rpc::COMMAND_RPC_DELETE_ADDRESS_BOOK_ENTRY::request& req, wallet_rpc::COMMAND_RPC_DELETE_ADDRESS_BOOK_ENTRY::response& res, epee::json_rpc::error& er)
  {
    if (!&m_wallet) return not_open(er);
    if (m_wallet.restricted())
    {
      er.code = WALLET_RPC_ERROR_CODE_DENIED;
      er.message = "Command unavailable in restricted mode.";
      return false;
    }

    const auto ab = m_wallet.get_address_book();
    if (req.index >= ab.size())
    {
      er.code = WALLET_RPC_ERROR_CODE_WRONG_INDEX;
      er.message = "Index out of range: " + std::to_string(req.index);
      return false;
    }
    if (!m_wallet.delete_address_book_row(req.index))
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = "Failed to delete address book entry";
      return false;
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
  bool wallet_rpc_server::on_rescan_spent(const wallet_rpc::COMMAND_RPC_RESCAN_SPENT::request& req, wallet_rpc::COMMAND_RPC_RESCAN_SPENT::response& res, epee::json_rpc::error& er)
  {
    if (!&m_wallet) return not_open(er);
    if (m_wallet.restricted())
    {
      er.code = WALLET_RPC_ERROR_CODE_DENIED;
      er.message = "Command unavailable in restricted mode.";
      return false;
    }
    try
    {
      m_wallet.rescan_spent();
      return true;
    }
    catch (const std::exception &e)
    {
      er.code = WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR;
      er.message = e.what();
      return false;
    }
    return true;
  }
  //------------------------------------------------------------------------------------------------------------------------------
}

int main(int argc, char** argv) {
  namespace po = boost::program_options;

  const auto arg_wallet_file = wallet_args::arg_wallet_file();
  const auto arg_from_json = wallet_args::arg_generate_from_json();

  po::options_description desc_params(wallet_args::tr("Wallet options"));
  tools::wallet2::init_options(desc_params);
  command_line::add_arg(desc_params, arg_rpc_bind_port);
  command_line::add_arg(desc_params, arg_disable_rpc_login);
  cryptonote::rpc_args::init_options(desc_params);
  command_line::add_arg(desc_params, arg_wallet_file);
  command_line::add_arg(desc_params, arg_from_json);

  const auto vm = wallet_args::main(
    argc, argv,
    "citicash-wallet-rpc [--wallet-file=<file>|--generate-from-json=<file>] [--rpc-bind-port=<port>]",
    desc_params,
    po::positional_options_description()
  );
  if (!vm)
  {
    return 1;
  }

  epee::log_space::log_singletone::add_logger(LOGGER_CONSOLE, NULL, NULL, LOG_LEVEL_2);

  std::unique_ptr<tools::wallet2> wal;
  try
  {
    const auto wallet_file = command_line::get_arg(*vm, arg_wallet_file);
    const auto from_json = command_line::get_arg(*vm, arg_from_json);

    if(!wallet_file.empty() && !from_json.empty())
    {
      LOG_ERROR(tools::wallet_rpc_server::tr("Can't specify more than one of --wallet-file and --generate-from-json"));
      return 1;
    }

    if (wallet_file.empty() && from_json.empty())
    {
      LOG_ERROR(tools::wallet_rpc_server::tr("Must specify --wallet-file or --generate-from-json"));
      return 1;
    }

    LOG_PRINT_L0(tools::wallet_rpc_server::tr("Loading wallet..."));
    if(!wallet_file.empty())
    {
      wal = tools::wallet2::make_from_file(*vm, wallet_file).first;
    }
    else
    {
      wal = tools::wallet2::make_from_json(*vm, from_json);
    }
    if (!wal)
    {
      return 1;
    }

    bool quit = false;
    tools::signal_handler::install([&wal, &quit](int) {
      assert(wal);
      quit = true;
      wal->stop();
    });

    wal->refresh();
    // if we ^C during potentially length load/refresh, there's no server loop yet
    if (quit)
    {
      LOG_PRINT_L0(tools::wallet_rpc_server::tr("Storing wallet..."));
      wal->store();
      LOG_PRINT_GREEN(tools::wallet_rpc_server::tr("Stored ok"), LOG_LEVEL_0);
      return 1;
    }
    LOG_PRINT_GREEN(tools::wallet_rpc_server::tr("Loaded ok"), LOG_LEVEL_0);
  }
  catch (const std::exception& e)
  {
    LOG_ERROR(tools::wallet_rpc_server::tr("Wallet initialization failed: ") << e.what());
    return 1;
  }
  tools::wallet_rpc_server wrpc(*wal);
  bool r = wrpc.init(*vm);
  CHECK_AND_ASSERT_MES(r, 1, tools::wallet_rpc_server::tr("Failed to initialize wallet rpc server"));
  tools::signal_handler::install([&wrpc, &wal](int) {
    wrpc.send_stop_signal();
  });
  LOG_PRINT_L0(tools::wallet_rpc_server::tr("Starting wallet rpc server"));
  wrpc.run();
  LOG_PRINT_L0(tools::wallet_rpc_server::tr("Stopped wallet rpc server"));
  try
  {
    LOG_PRINT_L0(tools::wallet_rpc_server::tr("Storing wallet..."));
    wal->store();
    LOG_PRINT_GREEN(tools::wallet_rpc_server::tr("Stored ok"), LOG_LEVEL_0);
  }
  catch (const std::exception& e)
  {
    LOG_ERROR(tools::wallet_rpc_server::tr("Failed to store wallet: ") << e.what());
    return 1;
  }
  return 0;
}

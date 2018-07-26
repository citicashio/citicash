// Copyright (c) 2018, The CitiCash Project
// Copyright (c) 2017, SUMOKOIN
// Copyright (c) 2014-2016, The Monero Project
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
#include "include_base_utils.h"

#include <set>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <boost/serialization/version.hpp>
#include <boost/utility.hpp>

#include "string_tools.h"
#include "syncobj.h"
#include "math_helper.h"
#include "cryptonote_basic_impl.h"
#include "verification_context.h"
#include "crypto/hash.h"
#include "rpc/core_rpc_server_commands_defs.h"

namespace cryptonote
{
  class Blockchain;
  /************************************************************************/
  /*                                                                      */
  /************************************************************************/

  //! pair of <transaction fee, transaction hash> for organization
  typedef std::pair<std::pair<uint32_t, std::time_t>, crypto::hash> tx_by_fee_and_receive_time_entry;

  class txCompare
  {
  public:
    bool operator()(const tx_by_fee_and_receive_time_entry& a, const tx_by_fee_and_receive_time_entry& b)
    {
      // sort by greatest first, not least
      if (a.first.first > b.first.first) return true;
      else if (a.first.first < b.first.first) return false;
      else if (a.first.second < b.first.second) return true;
      else if (a.first.second > b.first.second) return false;
      else if (a.second != b.second) return true;
      else return false;
    }
  };

  //! container for sorting transactions by fee per unit size
  typedef std::set<tx_by_fee_and_receive_time_entry, txCompare> sorted_tx_container;

  /**
   * @brief Transaction pool, handles transactions which are not part of a block
   *
   * This class handles all transactions which have been received, but not as
   * part of a block.
   *
   * This handling includes:
   *   storing the transactions
   *   organizing the transactions by fee per size
   *   taking/giving transactions to and from various other components
   *   saving the transactions to disk on shutdown
   *   helping create a new block template by choosing transactions for it
   *
   */
  class tx_memory_pool: boost::noncopyable
  {
  public:
    /**
     * @brief Constructor
     *
     * @param bchs a Blockchain class instance, for getting chain info
     */
    tx_memory_pool(Blockchain& bchs);


    /**
     * @copydoc add_tx(const transaction&, tx_verification_context&, bool, bool, uint8_t)
     *
     * @param id the transaction's hash
     * @param blob_size the transaction's size
     */
    bool add_tx(const transaction &tx, const crypto::hash &id, size_t blob_size, tx_verification_context& tvc, bool kept_by_block, bool relayed, uint8_t version);

    /**
     * @brief add a transaction to the transaction pool
     *
     * Most likely the transaction will come from the network, but it is
     * also possible for transactions to come from popped blocks during
     * a reorg, or from local clients creating a transaction and
     * submitting it to the network
     *
     * @param tx the transaction to be added
     * @param tvc return-by-reference status about the transaction verification
     * @param kept_by_block has this transaction been in a block?
     * @param relayed was this transaction from the network or a local client?
     * @param version the version used to create the transaction
     *
     * @return true if the transaction passes validations, otherwise false
     */
    bool add_tx(const transaction &tx, tx_verification_context& tvc, bool kept_by_block, bool relayed, uint8_t version);

    /**
     * @brief takes a transaction with the given hash from the pool
     *
     * @param id the hash of the transaction
     * @param tx return-by-reference the transaction taken
     * @param blob_size return-by-reference the transaction's size
     * @param fee the transaction fee
     * @param relayed return-by-reference was transaction relayed to us by the network?
     *
     * @return true unless the transaction cannot be found in the pool
     */
    bool take_tx(const crypto::hash &id, transaction &tx, size_t& blob_size, uint64_t& fee, bool &relayed);

    /**
     * @brief checks if the pool has a transaction with the given hash
     *
     * @param id the hash to look for
     *
     * @return true if the transaction is in the pool, otherwise false
     */
    bool have_tx(const crypto::hash &id) const;

    /**
     * @brief action to take when notified of a block added to the blockchain
     *
     * Currently does nothing
     *
     * @param new_block_height the height of the blockchain after the change
     * @param top_block_id the hash of the new top block
     *
     * @return true
     */
    bool on_blockchain_inc(uint64_t new_block_height, const crypto::hash& top_block_id);

    /**
     * @brief action to take when notified of a block removed from the blockchain
     *
     * Currently does nothing
     *
     * @param new_block_height the height of the blockchain after the change
     * @param top_block_id the hash of the new top block
     *
     * @return true
     */
    bool on_blockchain_dec(uint64_t new_block_height, const crypto::hash& top_block_id);

    /**
     * @brief action to take periodically
     *
     * Currently checks transaction pool for stale ("stuck") transactions
     */
    void on_idle();

    /**
     * @brief locks the transaction pool
     */
    void lock() const;

    /**
     * @brief unlocks the transaction pool
     */
    void unlock() const;

    // load/store operations

    /**
     * @brief loads pool state (if any) from disk, and initializes pool
     *
     * @param config_folder folder name where pool state will be
     *
     * @return true
     */
    bool init(const std::string& config_folder);

    /**
     * @brief attempts to save the transaction pool state to disk
     *
     * Currently fails (returns false) if the data directory from init()
     * does not exist and cannot be created, but returns true even if
     * saving to disk is unsuccessful.
     *
     * @return true in most cases (see above)
     */
    bool deinit();

    /**
     * @brief Chooses transactions for a block to include
     *
     * @param bl return-by-reference the block to fill in with transactions
     * @param median_size the current median block size
     * @param already_generated_coins the current total number of coins "minted"
     * @param total_size return-by-reference the total size of the new block
     * @param fee return-by-reference the total of fees from the included transactions
     *
     * @return true
     */
    bool fill_block_template(block &bl, size_t median_size, uint64_t already_generated_coins, size_t &total_size, uint64_t &fee, uint64_t height);

    /**
     * @brief get a list of all transactions in the pool
     *
     * @param txs return-by-reference the list of transactions
     */
    void get_transactions(std::list<transaction>& txs) const;

    /**
     * @brief get information about all transactions and key images in the pool
     *
     * see documentation on tx_info and spent_key_image_info for more details
     *
     * @param tx_infos return-by-reference the transactions' information
     * @param key_image_infos return-by-reference the spent key images' information
     *
     * @return true
     */
    bool get_transactions_and_spent_keys_info(std::vector<tx_info>& tx_infos, std::vector<spent_key_image_info>& key_image_infos) const;

    /**
     * @brief get a specific transaction from the pool
     *
     * @param h the hash of the transaction to get
     * @param tx return-by-reference the transaction requested
     *
     * @return true if the transaction is found, otherwise false
     */
    bool get_transaction(const crypto::hash& h, transaction& tx) const;

    /**
     * @brief get a list of all relayable transactions and their hashes
     *
     * "relayable" in this case means:
     *   nonzero fee
     *   hasn't been relayed too recently
     *   isn't old enough that relaying it is considered harmful
     *
     * @param txs return-by-reference the transactions and their hashes
     *
     * @return true
     */
    bool get_relayable_transactions(std::list<std::pair<crypto::hash, cryptonote::transaction>>& txs) const;

    /**
     * @brief tell the pool that certain transactions were just relayed
     *
     * @param txs the list of transactions (and their hashes)
     */
    void set_relayed(const std::list<std::pair<crypto::hash, cryptonote::transaction>>& txs);

    /**
     * @brief get the total number of transactions in the pool
     *
     * @return the number of transactions in the pool
     */
    size_t get_transactions_count() const;

    /**
     * @brief get a string containing human-readable pool information
     *
     * @param short_format whether to use a shortened format for the info
     *
     * @return the string
     */
    std::string print_pool(bool short_format) const;

    /**
     * @brief remove transactions from the pool which are no longer valid
     *
     * With new versions of the currency, what conditions render a transaction
     * invalid may change.  This function clears those which were received
     * before a version change and no longer conform to requirements.
     *
     * @param version the version the transactions must conform to
     *
     * @return the number of transactions removed
     */
    size_t validate(uint8_t version);


#define CURRENT_MEMPOOL_ARCHIVE_VER    11
#define CURRENT_MEMPOOL_TX_DETAILS_ARCHIVE_VER    11

    /**
     * @brief serialize the transaction pool to/from disk
     *
     * If the archive version passed is older than the version compiled
     * in, this function does nothing, as it cannot deserialize after a
     * format change.
     *
     * @tparam archive_t the archive class
     * @param a the archive to serialize to/from
     * @param version the archive version
     */
    template<class archive_t>
    void serialize(archive_t & a, const unsigned int version)
    {
      if(version < CURRENT_MEMPOOL_ARCHIVE_VER )
        return;
      CRITICAL_REGION_LOCAL(m_transactions_lock);
      a & m_transactions;
      a & m_spent_key_images;
      a & m_pending_aliases;
      a & m_timed_out_transactions;
    }

    /**
     * @brief information about a single transaction
     */
    struct tx_details
    {
      transaction tx;  //!< the transaction
      size_t blob_size;  //!< the transaction's size
      uint64_t fee;  //!< the transaction's fee amount
      crypto::hash max_used_block_id;  //!< the hash of the highest block referenced by an input
      uint64_t max_used_block_height;  //!< the height of the highest block referenced by an input

      //! whether or not the transaction has been in a block before
      /*! if the transaction was returned to the pool from the blockchain
       *  due to a reorg, then this will be true
       */
      bool kept_by_block;  

      //! the highest block the transaction referenced when last checking it failed
      /*! if verifying a transaction's inputs fails, it's possible this is due
       *  to a reorg since it was created (if it used recently created outputs
       *  as inputs).
       */
      uint64_t last_failed_height;  

      //! the hash of the highest block the transaction referenced when last checking it failed
      /*! if verifying a transaction's inputs fails, it's possible this is due
       *  to a reorg since it was created (if it used recently created outputs
       *  as inputs).
       */
      crypto::hash last_failed_id;

      time_t receive_time;  //!< the time when the transaction entered the pool

      time_t last_relayed_time;  //!< the last time the transaction was relayed to the network
      bool relayed;  //!< whether or not the transaction has been relayed to the network
    };

  private:

    /**
     * @brief remove old transactions from the pool
     *
     * After a certain time, it is assumed that a transaction which has not
     * yet been mined will likely not be mined.  These transactions are removed
     * from the pool to avoid buildup.
     *
     * @return true
     */
    bool remove_stuck_transactions();

    /**
     * @brief check if a transaction in the pool has a given spent key image
     *
     * @param key_im the spent key image to look for
     *
     * @return true if the spent key image is present, otherwise false
     */
    bool have_tx_keyimg_as_spent(const crypto::key_image& key_im) const;
    bool has_alias(const std::string& alias) const;

    /**
     * @brief check if any spent key image in a transaction is in the pool
     *
     * Checks if any of the spent key images in a given transaction are present
     * in any of the transactions in the transaction pool.
     *
     * @note see tx_pool::have_tx_keyimg_as_spent
     *
     * @param tx the transaction to check spent key images of
     *
     * @return true if any spent key images are present in the pool, otherwise false
     */
    bool have_tx_keyimges_as_spent(const transaction& tx) const;

    /**
     * @brief forget a transaction's spent key images
     *
     * Spent key images are stored separately from transactions for
     * convenience/speed, so this is part of the process of removing
     * a transaction from the pool.
     *
     * @param tx the transaction
     *
     * @return false if any key images to be removed cannot be found, otherwise true
     */
    bool remove_transaction_keyimages(const transaction& tx);
    bool remove_transaction_alias(const transaction& tx);

    /**
     * @brief check if any of a transaction's spent key images are present in a given set
     *
     * @param kic the set of key images to check against
     * @param tx the transaction to check
     *
     * @return true if any key images present in the set, otherwise false
     */
    static bool have_key_images(const std::unordered_set<crypto::key_image>& kic, const transaction& tx);

    /**
     * @brief append the key images from a transaction to the given set
     *
     * @param kic the set of key images to append to
     * @param tx the transaction
     *
     * @return false if any append fails, otherwise true
     */
    static bool append_key_images(std::unordered_set<crypto::key_image>& kic, const transaction& tx);

    /**
     * @brief check if a transaction is a valid candidate for inclusion in a block
     *
     * @param txd the transaction to check (and info about it)
     *
     * @return true if the transaction is good to go, otherwise false
     */
    bool is_transaction_ready_to_go(tx_details& txd) const;

    //! map transactions (and related info) by their hashes
    typedef std::unordered_map<crypto::hash, tx_details > transactions_container;

    //TODO: confirm the below comments and investigate whether or not this
    //      is the desired behavior
    //! map key images to transactions which spent them
    /*! this seems odd, but it seems that multiple transactions can exist
     *  in the pool which both have the same spent key.  This would happen
     *  in the event of a reorg where someone creates a new/different
     *  transaction on the assumption that the original will not be in a
     *  block again.
     */
    typedef std::unordered_map<crypto::key_image, std::unordered_set<crypto::hash> > key_images_container;

#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
public:
#endif
    mutable epee::critical_section m_transactions_lock;  //!< lock for the pool
    transactions_container m_transactions;  //!< container for transactions in the pool
#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
private:
#endif

    //! container for spent key images from the transactions in the pool
    key_images_container m_spent_key_images;

    std::unordered_map<std::string, std::unordered_set<crypto::hash>> m_pending_aliases;

    //TODO: this time should be a named constant somewhere, not hard-coded
    //! interval on which to check for stale/"stuck" transactions
    epee::math_helper::once_a_time_seconds<30> m_remove_stuck_tx_interval;

    //TODO: look into doing this better
    //!< container for transactions organized by fee per size and receive time
    sorted_tx_container m_txs_by_fee_and_receive_time;

    /**
     * @brief get an iterator to a transaction in the sorted container
     *
     * @param id the hash of the transaction to look for
     *
     * @return an iterator, possibly to the end of the container if not found
     */
    sorted_tx_container::iterator find_tx_in_sorted_container(const crypto::hash& id) const;

    //! transactions which are unlikely to be included in blocks
    /*! These transactions are kept in RAM in case they *are* included
     *  in a block eventually, but this container is not saved to disk.
     */
    std::unordered_set<crypto::hash> m_timed_out_transactions;

    std::string m_config_folder;  //!< the folder to save state to
    Blockchain& m_blockchain;  //!< reference to the Blockchain object
  };
}

namespace boost
{
  namespace serialization
  {
    template<class archive_t>
    void serialize(archive_t & ar, cryptonote::tx_memory_pool::tx_details& td, const unsigned int version)
    {
      ar & td.blob_size;
      ar & td.fee;
      ar & td.tx;
      ar & td.max_used_block_height;
      ar & td.max_used_block_id;
      ar & td.last_failed_height;
      ar & td.last_failed_id;
      ar & td.receive_time;
      ar & td.last_relayed_time;
      ar & td.relayed;
      if (version < 11)
        return;
      ar & td.kept_by_block;
    }
  }
}
BOOST_CLASS_VERSION(cryptonote::tx_memory_pool, CURRENT_MEMPOOL_ARCHIVE_VER)
BOOST_CLASS_VERSION(cryptonote::tx_memory_pool::tx_details, CURRENT_MEMPOOL_TX_DETAILS_ARCHIVE_VER)
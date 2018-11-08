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

/*!
 * \file electrum-words.cpp
 *
 * \brief Mnemonic seed generation and wallet restoration from them.
 *
 * This file and its header file are for translating Electrum-style word lists
 * into their equivalent byte representations for cross-compatibility with
 * that method of "backing up" one's wallet keys.
 */

#include <string>
#include <cassert>
#include <map>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <boost/algorithm/string.hpp>
#include "crypto/crypto.h"  // for declaration of crypto::secret_key
#include <fstream>
#include "mnemonics/electrum-words.h"
#include <stdexcept>
#include <boost/filesystem.hpp>
#include <boost/crc.hpp>
#include <boost/algorithm/string/join.hpp>

#include "english.h"
#include "italian.h"
#include "german.h"
#include "spanish.h"
#include "portuguese.h"
#include "japanese.h"
#include "russian.h"
#include "language_base.h"
#include "singleton.h"

namespace
{

  /*!
   * \brief Finds the word list that contains the seed words and puts the indices
   *        where matches occured in matched_indices.
   * \param  seed            List of words to match.
   * \param  has_checksum    If word list passed checksum test, we need to only do a prefix check.
   * \param  matched_indices The indices where the seed words were found are added to this.
   * \param  language        Language instance pointer to write to after it is found.
   * \return                 true if all the words were present in some language false if not.
   */
  bool find_seed_language(const std::vector<std::string> &seed,
    bool has_checksum, std::vector<uint32_t> &matched_indices, Language::Base **language)
  {
    // If there's a new language added, add an instance of it here.
    std::vector<Language::Base*> language_instances({
      Language::Singleton<Language::English>::instance(),
      Language::Singleton<Language::Spanish>::instance(),
      Language::Singleton<Language::German>::instance(),
      Language::Singleton<Language::Italian>::instance(),
      Language::Singleton<Language::Portuguese>::instance(),
      Language::Singleton<Language::Japanese>::instance(),
      Language::Singleton<Language::Russian>::instance()
    });

    // Iterate through all the languages and find a match
    for (std::vector<Language::Base*>::iterator it1 = language_instances.begin();
      it1 != language_instances.end(); it1++)
    {
      const std::unordered_map<std::string, uint32_t> &word_map = (*it1)->get_word_map();
      const std::unordered_map<std::string, uint32_t> &trimmed_word_map = (*it1)->get_trimmed_word_map();
      // To iterate through seed words
      std::vector<std::string>::const_iterator it2;
      bool full_match = true;

      std::string trimmed_word;
      // Iterate through all the words and see if they're all present
      for (it2 = seed.begin(); it2 != seed.end(); it2++)
      {
        if (has_checksum)
        {
          trimmed_word = Language::utf8prefix(*it2, (*it1)->get_unique_prefix_length());
          // Use the trimmed words and map
          if (trimmed_word_map.count(trimmed_word) == 0)
          {
            full_match = false;
            break;
          }
          matched_indices.push_back(trimmed_word_map.at(trimmed_word));
        }
        else
        {
          if (word_map.count(*it2) == 0)
          {
            full_match = false;
            break;
          }
          matched_indices.push_back(word_map.at(*it2));
        }
      }
      if (full_match)
      {
        *language = *it1;
        return true;
      }
      // Some didn't match. Clear the index array.
      matched_indices.clear();
    }
    return false;
  }

  /*!
   * \brief Creates a checksum index in the seed words.
   * \param  seed                 Vector of seed words
   * \param unique_prefix_length  the prefix length of each word to use for checksum
   * \return                      Checksum index
   */
  uint32_t create_checksum_index(const std::vector<std::string> &seed,
    uint32_t unique_prefix_length)
  {
    std::string trimmed_words = "";

    for (std::vector<std::string>::const_iterator it = seed.begin(); it != seed.end(); it++)
    {
      if (it->length() > unique_prefix_length)
      {
        trimmed_words += Language::utf8prefix(*it, unique_prefix_length);
      }
      else
      {
        trimmed_words += *it;
      }
    }
    boost::crc_32_type result;
    result.process_bytes(trimmed_words.data(), trimmed_words.length());
    return result.checksum() % crypto::ElectrumWords::seed_length;
  }


  /*!
  * \brief Creates a checksum index in the word list array on the list of words.
  * \param  seed                 Vector of seed words plus checksum (generated at create_checksum_index())
  * \param unique_prefix_length  the prefix length of each word to use for checksum
  * \param word_list_length      lengh of word list
  * \return                      Checksum2 index
  */
  uint32_t create_checksum_index2(const std::vector<std::string> &seed,
    uint32_t unique_prefix_length, uint32_t word_list_length)
  {
    std::string trimmed_words = "";

    for (std::vector<std::string>::const_iterator it = seed.begin(); it != seed.end(); it++)
    {
      if (it->length() > unique_prefix_length)
      {
        trimmed_words += Language::utf8prefix(*it, unique_prefix_length);
      }
      else
      {
        trimmed_words += *it;
      }
    }
    boost::crc_32_type result;
    result.process_bytes(trimmed_words.data(), trimmed_words.length());
    return result.checksum() % word_list_length;
  }

  /*!
   * \brief Does the checksum test on the seed passed.
   * \param seed                  Vector of seed words
   * \param unique_prefix_length  the prefix length of each word to use for checksum
   * \return                      True if the test passed false if not.
   */
  bool checksum_test(std::vector<std::string> seed, uint32_t unique_prefix_length)
  {
    // The last word is the checksum.
    std::string last_word = seed.back();
    seed.pop_back();

    std::string checksum = seed[create_checksum_index(seed, unique_prefix_length)];

    std::string trimmed_checksum = checksum.length() > unique_prefix_length ? Language::utf8prefix(checksum, unique_prefix_length) :
      checksum;
    std::string trimmed_last_word = last_word.length() > unique_prefix_length ? Language::utf8prefix(last_word, unique_prefix_length) :
      last_word;
    return trimmed_checksum == trimmed_last_word;
  }


  /*!
  * \brief Does the checksum test on the seed + prev checksum passed.
  * \param seed                  Vector of seed words
  * \param unique_prefix_length  the prefix length of each word to use for checksum
  * \param word_list             Vector of word list
  * \return                      True if the test passed false if not.
  */
  bool checksum_test2(std::vector<std::string> seed, uint32_t unique_prefix_length, const std::vector<std::string> &word_list)
  {
    // The last word is the checksum.
    std::string last_word = seed.back();
    seed.pop_back();

    std::string checksum = word_list[create_checksum_index2(seed, unique_prefix_length, word_list.size())];

    std::string trimmed_checksum = checksum.length() > unique_prefix_length ? Language::utf8prefix(checksum, unique_prefix_length) :
      checksum;
    std::string trimmed_last_word = last_word.length() > unique_prefix_length ? Language::utf8prefix(last_word, unique_prefix_length) :
      last_word;
    return trimmed_checksum == trimmed_last_word;
  }
}

/*!
 * \namespace crypto
 *
 * \brief crypto namespace.
 */
namespace crypto
{
  /*!
   * \namespace crypto::ElectrumWords
   *
   * \brief Mnemonic seed word generation and wallet restoration helper functions.
   */
  namespace ElectrumWords
  {
    /*!
     * \brief Converts seed words to bytes (secret key).
     * \param  words           String containing the words separated by spaces.
     * \param  dst             To put the secret key restored from the words.
     * \param  language_name   Language of the seed as found gets written here.
     * \return                 false if not a multiple of 3 words, or if word is not in the words list
     */
    bool words_to_bytes(std::string words, crypto::secret_key& dst,
      std::string &language_name)
    {
      std::vector<std::string> seed;

      boost::algorithm::trim(words);
      boost::split(seed, words, boost::is_any_of(" "));

      // error on non-compliant word list
      if (seed.size() != seed_length + 2)
      {
        return false;
      }

      std::vector<uint32_t> matched_indices;
      Language::Base *language;
      if (!find_seed_language(seed, true, matched_indices, &language))
      {
        return false;
      }
      const std::vector<std::string> &word_list = language->get_word_list();
      language_name = language->get_language_name();

      uint32_t word_list_length = word_list.size();

      if (!checksum_test2(seed, language->get_unique_prefix_length(), word_list))
      {
        // Checksum fail
        return false;
      }
      seed.pop_back();

      if (!checksum_test(seed, language->get_unique_prefix_length()))
      {
        // Checksum fail
        return false;
      }
      seed.pop_back();

      for (unsigned int i=0; i < seed.size() / 3; i++)
      {
        uint32_t val;
        uint32_t w1, w2, w3;
        w1 = matched_indices[i*3];
        w2 = matched_indices[i*3 + 1];
        w3 = matched_indices[i*3 + 2];

        val = w1 + word_list_length * (((word_list_length - w1) + w2) % word_list_length) +
          word_list_length * word_list_length * (((word_list_length - w2) + w3) % word_list_length);

        if (!(val % word_list_length == w1)) return false;

        memcpy(dst.data + i * 4, &val, 4);  // copy 4 bytes to position
      }

      return true;
    }

    /*!
     * \brief Converts bytes (secret key) to seed words.
     * \param  src           Secret key
     * \param  words         Space delimited concatenated words get written here.
     * \param  language_name Seed language name
     * \return               true if successful false if not. Unsuccessful if wrong key size.
     */
    bool bytes_to_words(const crypto::secret_key& src, std::string& words,
      const std::string &language_name)
    {

      if (sizeof(src.data) % 4 != 0 || sizeof(src.data) == 0) return false;

      Language::Base *language;
      if (language_name == "English")
      {
        language = Language::Singleton<Language::English>::instance();
      }
      else if (language_name == "Spanish")
      {
        language = Language::Singleton<Language::Spanish>::instance();
      }
      else if (language_name == "Portuguese")
      {
        language = Language::Singleton<Language::Portuguese>::instance();
      }
      else if (language_name == "Japanese")
      {
        language = Language::Singleton<Language::Japanese>::instance();
      }
      else if (language_name == "Italian")
      {
        language = Language::Singleton<Language::Italian>::instance();
      }
      else if (language_name == "German")
      {
        language = Language::Singleton<Language::German>::instance();
      }
      else if (language_name == "Russian")
      {
        language = Language::Singleton<Language::Russian>::instance();
      }
      else
      {
        return false;
      }
      const std::vector<std::string> &word_list = language->get_word_list();
      // To store the words for random access to add the checksum word later.
      std::vector<std::string> words_store;

      uint32_t word_list_length = word_list.size();
      // 8 bytes -> 3 words.  8 digits base 16 -> 3 digits base 1626
      for (unsigned int i=0; i < sizeof(src.data)/4; i++, words += ' ')
      {
        uint32_t w1, w2, w3;

        uint32_t val;

        memcpy(&val, (src.data) + (i * 4), 4);

        w1 = val % word_list_length;
        w2 = ((val / word_list_length) + w1) % word_list_length;
        w3 = (((val / word_list_length) / word_list_length) + w2) % word_list_length;

        words += word_list[w1];
        words += ' ';
        words += word_list[w2];
        words += ' ';
        words += word_list[w3];

        words_store.push_back(word_list[w1]);
        words_store.push_back(word_list[w2]);
        words_store.push_back(word_list[w3]);
      }

      words.pop_back();
      std::string checksum_word = words_store[create_checksum_index(words_store, language->get_unique_prefix_length())];
      words += (' ' + checksum_word);
      words_store.push_back(checksum_word);
      words += (' ' + word_list[create_checksum_index2(words_store, language->get_unique_prefix_length(), word_list_length)]);
      return true;
    }

    /*!
     * \brief Gets a list of seed languages that are supported.
     * \param languages The vector is set to the list of languages.
     */
    void get_language_list(std::vector<std::string> &languages)
    {
      std::vector<Language::Base*> language_instances({
        Language::Singleton<Language::English>::instance(),
        Language::Singleton<Language::Spanish>::instance(),
        Language::Singleton<Language::German>::instance(),
        Language::Singleton<Language::Italian>::instance(),
        Language::Singleton<Language::Portuguese>::instance(),
        Language::Singleton<Language::Russian>::instance(),
        Language::Singleton<Language::Japanese>::instance()
      });
      for (std::vector<Language::Base*>::iterator it = language_instances.begin();
        it != language_instances.end(); it++)
      {
        languages.push_back((*it)->get_language_name());
      }
    }

  }

}

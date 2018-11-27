// Copyright (c) 2018, The CitiCash Project
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
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers//
// Created by jakub on 3.10.18.
//
#ifndef CITICASH_DEVNET_H
#define CITICASH_DEVNET_H

#define DEVNET_ID_GENTX_AND_PORT \
uint16_t const P2P_DEFAULT_PORT = 65535; \
uint16_t const RPC_DEFAULT_PORT = 65535; \
boost::uuids::uuid const NETWORK_ID = { { \
                                                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff \
                                        } }; \
   std::string const GENESIS_TX = "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"; \

#define DEVNET_CHECKPOINTS { \
 crypto::hash h; bool r = true; \
 r &= epee::string_tools::parse_tpod_from_hex_string("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", h); m_points[0] = h; \
 if(!r) \
     throw("Inconsistent checkpoints in devnet"); \
}
#define TEXT_P2P_PORT ":39833"

#define DEVNET_SEEDS \
                "node1.devciticash.mydomain", \
                "node2.devciticash.mydomain", \
                "node3.devciticash.mydomain", \
                "node4.devciticash.mydomain", \
                "node5.devciticash.mydomain"

#define DEVNET_FALLBACK_SEEDS \
    full_addrs.insert("192.168.255.255:65535"); \
    full_addrs.insert("192.168.255.255:65535"); \
    full_addrs.insert("192.168.255.255:65535"); \
    full_addrs.insert("192.168.255.255:65535"); \
    full_addrs.insert("192.168.255.255:65535");

#define DEVNET_FORKS \
    { 1, 1, 0, 1482806500 }

#define DEVCRYPTONOTENAME const std::string CRYPTONOTE_NAME {"devciticash"};

#endif //CITICASH_DEVNET_H
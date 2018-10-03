//
// Created by jakub on 1.10.18.
//
#ifndef CITICASH_DEVNET_H
#define CITICASH_DEVNET_H

//



#define DEVNET_ID_GENTX_AND_PORT \
uint16_t const P2P_DEFAULT_PORT = 39833; \
uint16_t const RPC_DEFAULT_PORT = 39834; \
boost::uuids::uuid const NETWORK_ID = { { \
                                                0x5a, 0x1d, 0xa0, 0x7d, 0x27, 0x4a, 0x24, 0xd4, 0xf3, 0x8d, 0x42, 0x44, 0x42, 0xa8, 0x04, 0x66 \
                                        } }; \
   std::string const GENESIS_TX = "020a01ff00018080b6a58eb9e7ef01029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd08807121010a5323532a4e4b43bc329f376ffc544acc70fce3ce11a1689d3592bf1a30f02c00"; \

#define INIT_DEVNET_CHECKPOINTS { \
 crypto::hash h; bool r = true; \
 r  = epee::string_tools::parse_tpod_from_hex_string("1a5813655714c6be2251806463b4b43bf52e0f0762551cd5e3cafb394cbdaf0c", h); m_points[1] = h; \
 r &= epee::string_tools::parse_tpod_from_hex_string("1df7850fda5aec7bb0d83dfac5826f85236fe930389657c975919d89e28cff8e", h); m_points[100] = h; \
 r &= epee::string_tools::parse_tpod_from_hex_string("027632cafb4279c7bb0afa4babc3efe1b85a0920b458d921a6a6da0784a36fae", h); m_points[10000] = h; \
 r &= epee::string_tools::parse_tpod_from_hex_string("e16c4651fba2b3d39ca3ad4b4a8a93a4c96f7ace03f51cede77492bd7c13110e", h); m_points[29636] = h; \
 if(!r) \
     throw(333); \
}
#define TEXT_P2P_PORT ":39833"

#define DEVNET_SEEDS \
                "node1.citicash.loc", \
                "node2.citicash.loc", \
                "node3.citicash.loc", \
                "node4.citicash.loc", \
                "node5.citicash.loc"

#define DEVNET_FALLBACK_SEEDS \
    full_addrs.insert("192.168.25.121:39833"); \
    full_addrs.insert("192.168.25.111:39833"); \
    full_addrs.insert("192.168.25.154:39833"); \
    full_addrs.insert("192.168.25.193:39833"); \
    full_addrs.insert("192.168.25.244:39833");

#define DEVCRYPTONOTENAME const std::string CRYPTONOTE_NAME {"citicashdev"};

#endif //CITICASH_DEVNET_H



// Copyright (c) 2011-2017 The Cryptonote Developers
// Copyright (c) 2018 The Circle Foundation
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <initializer_list>

namespace CryptoNote {
namespace parameters {

const uint64_t CRYPTONOTE_MAX_BLOCK_NUMBER                   = 500000000;
const size_t   CRYPTONOTE_MAX_BLOCK_BLOB_SIZE                = 500000000;
const size_t   CRYPTONOTE_MAX_TX_SIZE                        = 1000000000;
const uint64_t CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX       = 0x7ad4; // addresses start with "ccx7"
const size_t   CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW          = 10; // 20m unlock
const uint64_t CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT            = 60 * 60 * 2; // 2 hours
const uint64_t CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE           = 10; // 20m unlock

const size_t   BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW             = 30;

const uint64_t MONEY_SUPPLY                                  = UINT64_C(200000000000000); // max supply: 200M (Consensus II)

const uint32_t ZAWY_DIFFICULTY_BLOCK_INDEX                   = 0;
const size_t   ZAWY_DIFFICULTY_FIX                           = 1;
const uint8_t  ZAWY_DIFFICULTY_BLOCK_VERSION                 = 0;

const size_t   CRYPTONOTE_REWARD_BLOCKS_WINDOW               = 100;
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE     = 100000; // size of block (bytes): after which reward is calculated using block size
const size_t   CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE        = 600;
const size_t   CRYPTONOTE_DISPLAY_DECIMAL_POINT              = 6;

// COIN - number of smallest units in one coin
const uint64_t POINT                                         = UINT64_C(1000);     // pow(10, 3)
const uint64_t COIN                                          = UINT64_C(1000000);  // pow(10, 6)
const uint64_t MINIMUM_FEE                                   = UINT64_C(10);       // pow(10, 1)
const uint64_t DEFAULT_DUST_THRESHOLD                        = UINT64_C(10);       // pow(10, 1)

const uint64_t DIFFICULTY_TARGET                             = 120; // seconds = 2m
const uint64_t EXPECTED_NUMBER_OF_BLOCKS_PER_DAY             = 24 * 60 * 60 / DIFFICULTY_TARGET;
const size_t   DIFFICULTY_WINDOW                             = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY; // blocks
const size_t   DIFFICULTY_WINDOW_V1                          = DIFFICULTY_WINDOW;
const size_t   DIFFICULTY_WINDOW_V2                          = DIFFICULTY_WINDOW;
const size_t   DIFFICULTY_CUT                                = 60; // timestamps to cut after sorting
const size_t   DIFFICULTY_CUT_V1                             = DIFFICULTY_CUT;
const size_t   DIFFICULTY_CUT_V2                             = DIFFICULTY_CUT;
const size_t   DIFFICULTY_LAG                                = 15; // !!!
const size_t   DIFFICULTY_LAG_V1                             = DIFFICULTY_LAG;
const size_t   DIFFICULTY_LAG_V2                             = DIFFICULTY_LAG;

static_assert(2 * DIFFICULTY_CUT <= DIFFICULTY_WINDOW - 2, "Bad DIFFICULTY_WINDOW or DIFFICULTY_CUT");

const uint64_t DEPOSIT_MIN_AMOUNT                            = 1 * COIN; // minimun mmount for a valid deposit
const uint32_t DEPOSIT_MIN_TERM                              = 21900; // ~1 month
const uint32_t DEPOSIT_MAX_TERM                              = 1 * 12 * 21900; // ~1 year
const uint64_t DEPOSIT_MIN_TOTAL_RATE_FACTOR                 = 0; // rate is constant
const uint64_t DEPOSIT_MAX_TOTAL_RATE                        = 4; // percentage rate for DEPOSIT_MAX_TERM

static_assert(DEPOSIT_MIN_TERM > 0, "Bad DEPOSIT_MIN_TERM");
static_assert(DEPOSIT_MIN_TERM <= DEPOSIT_MAX_TERM, "Bad DEPOSIT_MAX_TERM");
static_assert(DEPOSIT_MIN_TERM * DEPOSIT_MAX_TOTAL_RATE > DEPOSIT_MIN_TOTAL_RATE_FACTOR, "Bad DEPOSIT_MIN_TOTAL_RATE_FACTOR or DEPOSIT_MAX_TOTAL_RATE");

const uint64_t MULTIPLIER_FACTOR                             = 100;
const uint32_t END_MULTIPLIER_BLOCK                          = 12750;

const size_t   MAX_BLOCK_SIZE_INITIAL                        = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE * 10;
const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_NUMERATOR         = 100 * 1024;
const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_DENOMINATOR       = 365 * 24 * 60 * 60 / DIFFICULTY_TARGET;

const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS     = 1;
const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS    = DIFFICULTY_TARGET * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS;

const size_t   CRYPTONOTE_MAX_TX_SIZE_LIMIT                  = (CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE / 4) - CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE;
const size_t   CRYPTONOTE_OPTIMIZE_SIZE                   = 100; // proportional to CRYPTONOTE_MAX_TX_SIZE_LIMIT

const uint64_t CRYPTONOTE_MEMPOOL_TX_LIVETIME                = (60 * 60 * 12); // seconds, 12 hours
const uint64_t CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME = (60 * 60 * 24); // seconds, 1 day
const uint64_t CRYPTONOTE_NUMBER_OF_PERIODS_TO_FORGET_TX_DELETED_FROM_POOL = 7; // CRYPTONOTE_NUMBER_OF_PERIODS_TO_FORGET_TX_DELETED_FROM_POOL * CRYPTONOTE_MEMPOOL_TX_LIVETIME = time to forget tx

const size_t   FUSION_TX_MAX_SIZE                            = CRYPTONOTE_MAX_TX_SIZE_LIMIT * 2;
const size_t   FUSION_TX_MIN_INPUT_COUNT                     = 12;
const size_t   FUSION_TX_MIN_IN_OUT_COUNT_RATIO              = 4;

const uint64_t UPGRADE_HEIGHT                                = 1;
const uint64_t UPGRADE_HEIGHT_V2                             = 1;
const uint64_t UPGRADE_HEIGHT_V3                             = 12750; // Consensus III - block for CN Orion fork
const uint64_t UPGRADE_HEIGHT_V4                             = 45000; // block for minimum mixin fork
const unsigned UPGRADE_VOTING_THRESHOLD                      = 90; // percent
const size_t   UPGRADE_VOTING_WINDOW                         = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY; // blocks
const size_t   UPGRADE_WINDOW                                = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY; // blocks

static_assert(0 < UPGRADE_VOTING_THRESHOLD && UPGRADE_VOTING_THRESHOLD <= 100, "Bad UPGRADE_VOTING_THRESHOLD");
static_assert(UPGRADE_VOTING_WINDOW > 1, "Bad UPGRADE_VOTING_WINDOW");

const char     CRYPTONOTE_BLOCKS_FILENAME[]                  = "blocks.dat";
const char     CRYPTONOTE_BLOCKINDEXES_FILENAME[]            = "blockindexes.dat";
const char     CRYPTONOTE_BLOCKSCACHE_FILENAME[]             = "blockscache.dat";
const char     CRYPTONOTE_POOLDATA_FILENAME[]                = "poolstate.bin";
const char     P2P_NET_DATA_FILENAME[]                       = "p2pstate.bin";
const char     CRYPTONOTE_BLOCKCHAIN_INDICES_FILENAME[]      = "blockchainindices.dat";
const char     MINER_CONFIG_FILE_NAME[]                      = "miner_conf.json";

} // parameters

const uint64_t START_BLOCK_REWARD                            = (UINT64_C(5000) * parameters::POINT); // start reward (Consensus I)
const uint64_t FOUNDATION_TRUST	                             = (UINT64_C(12000000) * parameters::COIN); // locked funds to secure network  (Consensus II)
const uint64_t MAX_BLOCK_REWARD                              = (UINT64_C(20) * parameters::COIN); // max reward (Consensus I)
const uint64_t REWARD_INCREASE_INTERVAL                      = (UINT64_C(21900)); // aprox. 1 month (+ 0.25 CCX increment per month)

const char     CRYPTONOTE_NAME[]                             = "conceal";
const char     GENESIS_COINBASE_TX_HEX[]                     = "010a01ff0001c096b102029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd08807121017d6775185749e95ac2d70cae3f29e0e46f430ab648abbe9fdc61d8e7437c60f8";
const uint32_t GENESIS_NONCE                                 = 10000;
const uint64_t GENESIS_TIMESTAMP                             = 1527078920;

const uint8_t  TRANSACTION_VERSION_1                         =  1;
const uint8_t  TRANSACTION_VERSION_2                         =  2;
const uint8_t  BLOCK_MAJOR_VERSION_1                         =  1; // (Consensus I)
const uint8_t  BLOCK_MAJOR_VERSION_2                         =  2; // (Consensus II)
const uint8_t  BLOCK_MAJOR_VERSION_3                         =  3; // (Consensus III)
const uint8_t  BLOCK_MINOR_VERSION_0                         =  0;
const uint8_t  BLOCK_MINOR_VERSION_1                         =  1;

const size_t   BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT        = 10000; // by default, blocks ids count in synchronizing
const size_t   BLOCKS_SYNCHRONIZING_DEFAULT_COUNT            = 128; // by default, blocks count in blocks downloading
const size_t   COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT         = 1000;

const int      P2P_DEFAULT_PORT                              = 15000;
const int      RPC_DEFAULT_PORT                              = 16000;

const size_t   P2P_LOCAL_WHITE_PEERLIST_LIMIT                = 1000;
const size_t   P2P_LOCAL_GRAY_PEERLIST_LIMIT                 = 5000;

const size_t   P2P_CONNECTION_MAX_WRITE_BUFFER_SIZE          = 16 * 1024 * 1024; // 16MB
const uint32_t P2P_DEFAULT_CONNECTIONS_COUNT                 = 8;
const size_t   P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT     = 70; // percent
const uint32_t P2P_DEFAULT_HANDSHAKE_INTERVAL                = 60; // seconds
const uint32_t P2P_DEFAULT_PACKET_MAX_SIZE                   = 50000000; // 50000000 bytes maximum packet size
const uint32_t P2P_DEFAULT_PEERS_IN_HANDSHAKE                = 250;
const uint32_t P2P_DEFAULT_CONNECTION_TIMEOUT                = 5000; // 5 seconds
const uint32_t P2P_DEFAULT_PING_CONNECTION_TIMEOUT           = 2000; // 2 seconds
const uint64_t P2P_DEFAULT_INVOKE_TIMEOUT                    = 60 * 2 * 1000; // 2 minutes
const size_t   P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT          = 5000; // 5 seconds
const char     P2P_STAT_TRUSTED_PUB_KEY[]                    = "f7061e9a5f0d30549afde49c9bfbaa52ac60afdc46304642b460a9ea34bf7a4e";

// Seed Nodes
const std::initializer_list<const char*> SEED_NODES = {
		"212.237.59.97:15000", // gamma
		"94.177.171.102:15000", // aurora
		"188.213.165.210:15000", // omega
		"93.186.254.77:15000" // chaos
};

struct CheckpointData {
  uint32_t height;
  const char* blockId;
};

#ifdef __GNUC__
__attribute__((unused))
#endif

// Blockchain Checkpoints:
// {<block height>, "<block hash>"},
const std::initializer_list<CheckpointData> CHECKPOINTS = {
		{0, "b9dc432e56e37b52771970ce014dd23fda517cfd4fc5a9b296f1954b7d4505de"},
		{1000, "52ba463c6b6fbfd88765f50bb79761313091b585ed4a182a91fcf209c55ccb9f"},
		{2000, "b2f2d356cfc4acb3a5a9bb643a0d28059919d26101f46178262fc9c466321c4c"},
		{3000, "48013304f58d8b10882f7a1c265e5f307e453b44ab33ba5b9ea2a6565e993377"},
		{4000, "af92aac58e3ecf03ccd5118f920893fa4ff9207f650282d5fb439d18d53fe928"},
		{5000, "5fdf72e7c106d429231fb980bc336e736464403dee77c7c5ef5e87305644965d"},
		{6000, "acb8e16f4117763bccf966ad2a3af2f522246437c8be8956d1f145693a245b04"},
		{7000, "072a845b3113be159c2a79b8bb1206612b58a6e5395f2e449ddaf6941b660416"},
		{8000, "34cb847e7313e3a316d4eb4c017389f967da5d85573365ae0dec0e5bacddcfba"},
		{9000, "70f4fe7652d504b77bd459d76cc86ff78b15aab69f81567c70cafb5e25e43ffb"},
		{10000, "55cf271a5c97785fb35fea7ed177cb75f47c18688bd86fc01ae66508878029d6"},
		{11000, "eb4b3b4e3711ff132710392703d18eaffdfc044567447dae67ecd11a9799da68"},
		{12000, "b294a435ef3b39859a66ba90684c09c5c7ebf2bf647aaeabbd0be981b6d1f7a0"},
		{13000, "c3b566eb6e9f8e2a839d3f44b8351704a1f9a89b1b00b05155027217f094c787"},
		{14000, "72a7ba07f083af608cd19e20b5abd9dfc7d1fcc9a939efbd1586aa6ef51580d7"},
		{15000, "bbd63d8be2087567cefa44ff50a7d240a2b734be3efb26bfccc5391cbcb9e5ec"},
		{16000, "34d2aaecaa7ee8aae611df7e7fca44fd714cce91bd4b921eeccd8102ccb2f8ef"},
		{17000, "77c4f05c8f4269e3f0c631c3906751a96f33c9ed172929fdd7eba9446237a7ea"},
		{18000, "f752de14adeaaf230908582ab16657390c712902ba8436c8b8d2975695f03f99"},
		{19000, "9b61c4678ed84f4116bd0213efb6c84e24e67ae9c69c66430a8dd4f1fedc41ec"},
		{20000, "52533de7f1596154c6954530ae8331fe4f92e92d476f097c6d7d20ebab1c2748"},
		{21000, "8d3b8b2bce9a9d835fc4fa8c3502ddfaaca7ce0db90670eda49598b221ece835"},
		{22000, "f9fe323b236900c3917dd9b90e5df0aa36d43aa498a9d3f2558607f5cd7bf6b8"},
		{23000, "af81476fee85d2c608f78de6f8090be8d0438000492ed45df019648305539afd"},
		{25000, "ce97a9151180cbbfc52d184b6b60b9039a8a113ddd741bf520758e8b4bc21d98"},
		{27000, "1113e3e642a0166a2d2eb1c21c9d81fda84e36b0c20a8835b5d45f33ad87e7c5"},
		{29000, "92fb8d6f50a3a8bccdbbc2c034798d1c3e3f601451cafcee2212644d4823f187"},
		{31000, "c4e7a29d3240d72febd4b3d29d9cc573eaa2168166e7de87f82abe6dd3cf177b"},
		{33000, "651980d3e6fa7d29ce41e060d2cc9069f11af58b19099e0f04bed7b7ce141de5"},
		{35000, "aae3a729d2b74eebd8b2c41496c9170fff60966f5429fffce0eb6703e0092a9f"},
		{37000, "7fd74bf494758b9f85560400b59bdb975d44342cb8ebfd5d4e5934c0724a9b35"},
		{39000, "31109511629b414f0a198d35595d630eea32a258302a00dfaa192194502970b8"},
		{41000, "8ed39283321e27b41d0a19d4a0b1969b84f033397bd5c5bb549c33ada79a1190"},
		{43000, "652977e94353f36f8aa804506eee1d8cc1cbd5f7853f77e36cdf75ba6c638a64"},
		{45000, "4b822f1395996129be1140e591925a849da7e44b0ef350f29e12779eb9010f5e"},
		{47000, "385b7016c3785b0492fba6d99c5fb4575c2069b257e68656740ab1c7aaab73e1"},
		{49000, "40f2474bf4029ef7184316bc22c3ff62e116f8fb22e5f32a2c5e55a59462c638"},
		{51000, "3a3aa0d806f8ea523d5dc6bfdabfa4b01173da494c1e5bc9ca366ffb980f882d"},
		{53000, "7c03e3bdfd2e250dd059df3439addf8624dc5f4ce1c5f0928ce5812a364c234d"},
		{55000, "5180f25d32fa5f43c4b38d03b4826c0dfaedd737334d4a482b62026388524474"},
		{57000, "fd3817e6d368f255a53e352e402f0a0b9ea27e1b02c06247a9df09fbbce4ba17"},
		{59000, "957b9bb1cd74cd530143a63ecc3fba7ad5ee22bb54c829e9565c18dfe2308707"},
		{61000, "80ea9d3d1d93108a1748a3fa05b140a23f8afee4eac780627f4751f947cb0bad"},
		{63000, "b620f5c26de795e7ba875426f48c9c2380a17dbd56f23f77884a98ded2407141"},						
		{67000, "33f2d96861bad5ee69240875e2aec6075c134b0a81a97fc4e033319921cce797"},
		{69000, "1e0e4f43e63038f0077a9a358e354c9aeadb53396a75457cc9e7b2c9da8fcf6f"}
};


} // CryptoNote

#define ALLOW_DEBUG_COMMANDS

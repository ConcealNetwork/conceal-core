// Copyright (c) 2011-2017 The Cryptonote Developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2019 Conceal Network & Conceal Devs
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <initializer_list>

namespace CryptoNote {
namespace parameters {

const uint64_t CRYPTONOTE_MAX_BLOCK_NUMBER = 500000000;
const size_t   CRYPTONOTE_MAX_BLOCK_BLOB_SIZE = 500000000;
const size_t   CRYPTONOTE_MAX_TX_SIZE = 1000000000;
const uint64_t CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 0x7ad4; /* ccx7 address prefix */
const size_t   CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW = 10; /* 20 minutes */
const uint64_t CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT = 60 * 60 * 2; /* two hours */
const uint64_t CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V1 = 360; /* changed for LWMA3 */
const uint64_t CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE = 10; /* 20 minutes */

const size_t   BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW = 30;
const size_t   BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V1 = 11; /* changed for LWMA3 */

const uint64_t MONEY_SUPPLY = UINT64_C(200000000000000); /* max supply: 200M (Consensus II) */

const uint32_t ZAWY_DIFFICULTY_BLOCK_INDEX = 0;
const size_t   ZAWY_DIFFICULTY_FIX = 1;
const uint8_t  ZAWY_DIFFICULTY_BLOCK_VERSION = 0;

const size_t   CRYPTONOTE_REWARD_BLOCKS_WINDOW = 100;
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE = 100000; /* size of block in bytes, after which reward is calculated using block size */
const size_t   CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE = 600;
const size_t   CRYPTONOTE_DISPLAY_DECIMAL_POINT = 6;

const uint64_t POINT = UINT64_C(1000); 
const uint64_t COIN = UINT64_C(1000000); /* smallest atomic unit */
const uint64_t MINIMUM_FEE = UINT64_C(10); /* 0.000010 CCX */
const uint64_t MINIMUM_FEE_V1 = UINT64_C(100); /* 0.000100 CCX */
const uint64_t MINIMUM_FEE_BANKING = UINT64_C(1000); /* 0.001000 CCX */
const uint64_t DEFAULT_DUST_THRESHOLD = UINT64_C(10); /* 0.000010 CCX */  

const uint64_t DIFFICULTY_TARGET = 120; /* two minutes */
const uint64_t EXPECTED_NUMBER_OF_BLOCKS_PER_DAY = 24 * 60 * 60 / DIFFICULTY_TARGET; /* 720 blocks */
const size_t   DIFFICULTY_WINDOW = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY; 
const size_t   DIFFICULTY_WINDOW_V1 = DIFFICULTY_WINDOW;
const size_t   DIFFICULTY_WINDOW_V2 = DIFFICULTY_WINDOW;
const size_t   DIFFICULTY_WINDOW_V3 = 60; /* changed for LWMA3 */
const size_t   DIFFICULTY_BLOCKS_COUNT = DIFFICULTY_WINDOW_V3 + 1; /* added for LWMA3 */
const size_t   DIFFICULTY_CUT = 60; /* timestamps to cut after sorting */
const size_t   DIFFICULTY_CUT_V1 = DIFFICULTY_CUT;
const size_t   DIFFICULTY_CUT_V2 = DIFFICULTY_CUT;
const size_t   DIFFICULTY_LAG = 15; 
const size_t   DIFFICULTY_LAG_V1 = DIFFICULTY_LAG;
const size_t   DIFFICULTY_LAG_V2 = DIFFICULTY_LAG;

static_assert(2 * DIFFICULTY_CUT <= DIFFICULTY_WINDOW - 2, "Bad DIFFICULTY_WINDOW or DIFFICULTY_CUT");

const uint64_t DEPOSIT_MIN_AMOUNT = 1 * COIN; 
const uint32_t DEPOSIT_MIN_TERM = 5040; /* one week */
const uint32_t DEPOSIT_MAX_TERM = 1 * 12 * 21900; /* legacy deposts - one year */
const uint32_t DEPOSIT_MAX_TERM_V1 = 64800 * 20; /* five years */
const uint64_t DEPOSIT_MIN_TOTAL_RATE_FACTOR = 0; /* constant rate */
const uint64_t DEPOSIT_MAX_TOTAL_RATE = 4; /* legacy deposits */

static_assert(DEPOSIT_MIN_TERM > 0, "Bad DEPOSIT_MIN_TERM");
static_assert(DEPOSIT_MIN_TERM <= DEPOSIT_MAX_TERM, "Bad DEPOSIT_MAX_TERM");
static_assert(DEPOSIT_MIN_TERM * DEPOSIT_MAX_TOTAL_RATE > DEPOSIT_MIN_TOTAL_RATE_FACTOR, "Bad DEPOSIT_MIN_TOTAL_RATE_FACTOR or DEPOSIT_MAX_TOTAL_RATE");

const uint64_t MULTIPLIER_FACTOR = 100; /* legacy deposits */
const uint32_t END_MULTIPLIER_BLOCK = 12750; /* legacy deposits */

const size_t   MAX_BLOCK_SIZE_INITIAL = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE * 10;
const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_NUMERATOR = 100 * 1024;
const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_DENOMINATOR = 365 * 24 * 60 * 60 / DIFFICULTY_TARGET;

const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS = 1;
const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS = DIFFICULTY_TARGET * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS;

const size_t   CRYPTONOTE_MAX_TX_SIZE_LIMIT = (CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE / 4) - CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE; /* maximum transaction size */
const size_t   CRYPTONOTE_OPTIMIZE_SIZE = 100; /* proportional to CRYPTONOTE_MAX_TX_SIZE_LIMIT */

const uint64_t CRYPTONOTE_MEMPOOL_TX_LIVETIME = (60 * 60 * 12); /* 12 hours in seconds */
const uint64_t CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME = (60 * 60 * 24); /* 23 hours in seconds */
const uint64_t CRYPTONOTE_NUMBER_OF_PERIODS_TO_FORGET_TX_DELETED_FROM_POOL  = 7; /* CRYPTONOTE_NUMBER_OF_PERIODS_TO_FORGET_TX_DELETED_FROM_POOL * CRYPTONOTE_MEMPOOL_TX_LIVETIME  = time to forget tx */

const size_t   FUSION_TX_MAX_SIZE = CRYPTONOTE_MAX_TX_SIZE_LIMIT * 2;
const size_t   FUSION_TX_MIN_INPUT_COUNT = 12;
const size_t   FUSION_TX_MIN_IN_OUT_COUNT_RATIO = 4;

const uint64_t UPGRADE_HEIGHT = 1;
const uint64_t UPGRADE_HEIGHT_V2 = 1;
const uint64_t UPGRADE_HEIGHT_V3 = 12750; /* Cryptonight-Fast */
const uint64_t UPGRADE_HEIGHT_V4 = 45000; /* MixIn 2 */
const uint64_t UPGRADE_HEIGHT_V5 = 98160; /* Deposits 2.0, Investments 1.0 */
const uint64_t UPGRADE_HEIGHT_V6 = 104200; /* LWMA3 */
const uint64_t UPGRADE_HEIGHT_V7 = 195765; /* Cryptoight Conceal */
const unsigned UPGRADE_VOTING_THRESHOLD = 90; // percent
const size_t   UPGRADE_VOTING_WINDOW = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY; 
const size_t   UPGRADE_WINDOW = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY; 

static_assert(0 < UPGRADE_VOTING_THRESHOLD && UPGRADE_VOTING_THRESHOLD <= 100, "Bad UPGRADE_VOTING_THRESHOLD");
static_assert(UPGRADE_VOTING_WINDOW > 1, "Bad UPGRADE_VOTING_WINDOW");

const char     CRYPTONOTE_BLOCKS_FILENAME[] = "blocks.dat";
const char     CRYPTONOTE_BLOCKINDEXES_FILENAME[] = "blockindexes.dat";
const char     CRYPTONOTE_BLOCKSCACHE_FILENAME[] = "blockscache.dat";
const char     CRYPTONOTE_POOLDATA_FILENAME[] = "poolstate.bin";
const char     P2P_NET_DATA_FILENAME[] = "p2pstate.bin";
const char     CRYPTONOTE_BLOCKCHAIN_INDICES_FILENAME[]       = "blockchainindices.dat";
const char     MINER_CONFIG_FILE_NAME[]                       = "miner_conf.json";

} // parameters

const uint64_t START_BLOCK_REWARD = (UINT64_C(5000) * parameters::POINT); // start reward (Consensus I)
const uint64_t FOUNDATION_TRUST = (UINT64_C(12000000) * parameters::COIN); // locked funds to secure network  (Consensus II)
const uint64_t MAX_BLOCK_REWARD = (UINT64_C(20) * parameters::COIN); // max reward (Consensus I)
const uint64_t REWARD_INCREASE_INTERVAL = (UINT64_C(21900)); // aprox. 1 month (+ 0.25 CCX increment per month)

const char     CRYPTONOTE_NAME[] = "conceal";
const char     GENESIS_COINBASE_TX_HEX[] = "010a01ff0001c096b102029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd08807121017d6775185749e95ac2d70cae3f29e0e46f430ab648abbe9fdc61d8e7437c60f8";
const uint32_t GENESIS_NONCE = 10000;
const uint64_t GENESIS_TIMESTAMP = 1527078920;

const uint8_t  TRANSACTION_VERSION_1 = 1;
const uint8_t  TRANSACTION_VERSION_2 = 2;
const uint8_t  BLOCK_MAJOR_VERSION_1 = 1; // (Consensus I)
const uint8_t  BLOCK_MAJOR_VERSION_2 = 2; // (Consensus II)
const uint8_t  BLOCK_MAJOR_VERSION_3 = 3; // (Consensus III)
const uint8_t  BLOCK_MAJOR_VERSION_4 = 4; // LWMA3
const uint8_t  BLOCK_MAJOR_VERSION_7 = 7; /* Cryptonight Conceal */
const uint8_t  BLOCK_MINOR_VERSION_0 = 0;
const uint8_t  BLOCK_MINOR_VERSION_1 = 1;

const size_t   BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT = 10000; // by default, blocks ids count in synchronizing
const size_t   BLOCKS_SYNCHRONIZING_DEFAULT_COUNT = 128; // by default, blocks count in blocks downloading
const size_t   COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT = 1000;

const int      P2P_DEFAULT_PORT = 15000;
const int      RPC_DEFAULT_PORT = 16000;


/* P2P Network Configuration Section - This defines our current P2P network version
and the minimum version for communication between nodes */
const uint8_t  P2P_CURRENT_VERSION = 1;
const uint8_t  P2P_MINIMUM_VERSION = 1;
const uint8_t  P2P_UPGRADE_WINDOW = 2;

const size_t   P2P_LOCAL_WHITE_PEERLIST_LIMIT = 1000;
const size_t   P2P_LOCAL_GRAY_PEERLIST_LIMIT = 5000;

const size_t   P2P_CONNECTION_MAX_WRITE_BUFFER_SIZE = 64 * 1024 * 1024; // 64MB
const uint32_t P2P_DEFAULT_CONNECTIONS_COUNT = 8;
const size_t   P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT = 70; // percent
const uint32_t P2P_DEFAULT_HANDSHAKE_INTERVAL = 60; // seconds
const uint32_t P2P_DEFAULT_PACKET_MAX_SIZE = 50000000; // 50000000 bytes maximum packet size
const uint32_t P2P_DEFAULT_PEERS_IN_HANDSHAKE = 250;
const uint32_t P2P_DEFAULT_CONNECTION_TIMEOUT = 5000; // 5 seconds
const uint32_t P2P_DEFAULT_PING_CONNECTION_TIMEOUT = 2000; // 2 seconds
const uint64_t P2P_DEFAULT_INVOKE_TIMEOUT = 60 * 2 * 1000; // 2 minutes
const size_t   P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT = 5000; // 5 seconds
const char     P2P_STAT_TRUSTED_PUB_KEY[] = "f7061e9a5f0d30549afde49c9bfbaa52ac60afdc46304642b460a9ea34bf7a4e";

// Seed Nodes
const std::initializer_list<const char*> SEED_NODES  = {
	"212.237.59.97:15000", // Gamma
	"188.213.165.210:15000", // Omega
	"89.40.118.85:15000", // Delta
	"94.177.245.107:15000", // Lambda
	"199.175.54.195:15000", // Godspeed
	"173.212.196.43:15000", // Katz
	"192.3.114.99:15000", // Katz, US
	"139.99.42.182:15000" // Katz, Singapore
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
const std::initializer_list<CheckpointData> CHECKPOINTS  = {
	{0, "b9dc432e56e37b52771970ce014dd23fda517cfd4fc5a9b296f1954b7d4505de"},
	{10000, "55cf271a5c97785fb35fea7ed177cb75f47c18688bd86fc01ae66508878029d6"},
	{20000, "52533de7f1596154c6954530ae8331fe4f92e92d476f097c6d7d20ebab1c2748"},
	{30000, "50b5d84ac0b8abfe25669aac8514505c4c5f7ffd8e2bba0b52ab64f600d90796"},
	{40000, "ae2ed29163a57396f11c743400e55fba3f6b8e6bb6473f421c48ff8c87447ad0"},
	{50000, "8ad7969ca5d3cf48f784d33b60d1ea00bfb35b632447584e5181b194f3bb9cd6"},
	{60000, "22b1a161de2318b1a83ae0e3d1d04a2c420accccadd861aa8ad6365ec630ce04"},
	{70000, "4ef8a3c59b04ad8ae335fee0b5df0c1b114dda57d13232741d82c4984bf22bed"},
	{80000, "a60bd6b446c5b09997b5b70f31c56f35358657a673dcf56213a163fb6516750d"},
	{90000, "9985f631d4b2c15388e8c3797a1384b4610b13ff3852bc6d8f125ea4e13fdd22"},
	{100000, "1ccef60fb31646fc1745ccb42167f2e2efcf953a83b99ff4b6a39c99eb37d0e5"},
	{110000, "1b80bf8355ea023de7ed3367881ef111dfdb3aaeb25db3a0d6cad4c3cc0bb4bd"},
	{120000, "f621bd615716b75716eb89714d60481077a16b1df4046bf829f6d09b1c8e58a6"},
	{130000, "deb2514d03e2faf1c63b55f707b1524665ca4bd71cace3f4e8f0de58f32ecc41"},
	{140000, "c439524c13187bb6008acb1e9999317aa44e8d1cd75c96faae78831f6b961bac"},
	{150000, "90cc70379ea81d47df998e8b9928ba9191968035ae79ec1cb429c64a55497e03"},
	{160000, "4176bdff06416934d7766a6c2f6279d048cfdc516019a0580ea19c1d003038cc"},
	{170000, "50e3af756e96115011c8e4d138852e1f4835da805ca5ccd826f81593a53f4bd3"},
	{180000, "e1672173a2794245830a742d1df38b5fe5006fe6f00707e1b776bf29316ab18b"},
	{190000, "763eaa3c049ef46479144924b41cc9cb37346da88b0a3ae32a10e026c6f7984c"},
	{200000, "2ef304bec067c3a94f04440a593a13903a1487890493d15f74ec79c0ae585109"},
	{210000, "90dd7aca026ec5f9fdfd2fa9cd0c114c1c6c6bfc0536fb6490c804ac7ef72425"},
	{220000, "8de5278fc6703933e32e062b14496b0e1562c941e7e3c5b93147a3b39491fac5"},
	{230000, "f8ed2680d912a7f3aeb452d4eb8023f93f6387ff4c6927615691f66701d05d32"},
	{240000, "4445874d16b3dd8d5b0f9dee287e47219022c2b214c459e03be2bb71e4a12e3d"},
	{250000, "c579d2ad4f95a6c34180a89b32aa9fbe6ab2ecba9f3714ddde90fd5d9f85f6e9"},
	{260000, "ce63d00de7546f1dee417b2391692b367dc5c2cfe19ea43c98cf932d3838c5ec"},
	{270000, "f16000fefb54ad1f0f927f634c5b6f44fcfa201adc5ee093850301bd773c18fa"},
	{280000, "aba16466e085b2c7a792ba449f025bd1e37d6a1d44fa957a1ad4df78f41f6478"},
	{290000, "9fd5f13ac51df7ce2b8d78c45fbb864b231d6275bf7495118b4cc415301e6fe1"},
	{290665, "4e0082f3e66b0fe4176a850ff9560f1d8d2f2e11dc3a2045904209d11478f779"},
	{290674, "ac89a1f4c20674a8d735681b1ded3a1242252bb23341bc9b79bc06b310b490f4"},
	{290675, "6782c5e7436f77f4466253d6a70466cc6bfc66c6c51b675864c4543250c09e8b"},
	{290676, "0b25026f8c7fb194776c081f2bb32874b82f4298bd0d71c2d0a986117b97fa1e"},
	{290720, "36572a88fbed4654f4291f6d7a35a732b81f61e87ec27ce58f38047981b84e09"},
	{300000, "2a984212cc42ef62cd2229b624e05aa72926f0e89006e976c88b52d99ea14225"},
	{305000, "46104ab66387ab6ca6a3889e81c7b9810e27f547a8684659aeb62c438a3b6cf0"},
	{310000, "4a896f5de4f782c59f1f4691505aba0df87a20f2e06499b59496b8d7ffb025fe"},
	{320000, "c68d15c181bdfc6c5b7fe5c46c6432a03b95d640caa425a5cb3aa675c1d8f8fd"},	
	{330000, "af9e972f98bed57579a6691c3d21443d3cbff35005e984044bc99cee82d93922"},
	{340000, "6fce13dd473f3673cd08b28171902e281d7fdbbd8b8ba34e0019ae18f597d22f"},
	{350000, "f08aad1562ceee3a6c8147846bb3e5dd15b3168007f588ab68bd8ee816eb386d"}		
};

} // CryptoNote

#define ALLOW_DEBUG_COMMANDS

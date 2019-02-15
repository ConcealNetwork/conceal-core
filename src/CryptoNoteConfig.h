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

const uint64_t CRYPTONOTE_MAX_BLOCK_NUMBER													= 500000000;
const size_t   CRYPTONOTE_MAX_BLOCK_BLOB_SIZE												= 500000000;
const size_t   CRYPTONOTE_MAX_TX_SIZE														= 1000000000;
const uint64_t CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX										= 0x7ad4; /* ccx7 address prefix */
const size_t   CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW											= 10; /* 20 minutes */
const uint64_t CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT											= 60 * 60 * 2; /* two hours */
const uint64_t CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V1										= 360; /* changed for LWMA3 */
const uint64_t CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE											= 10; /* 20 minutes */

const size_t   BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW											= 30;
const size_t   BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V1											= 11; /* changed for LWMA3 */

const uint64_t MONEY_SUPPLY																	= UINT64_C(200000000000000); /* max supply: 200M (Consensus II) */

const uint32_t ZAWY_DIFFICULTY_BLOCK_INDEX													= 0;
const size_t   ZAWY_DIFFICULTY_FIX															= 1;
const uint8_t  ZAWY_DIFFICULTY_BLOCK_VERSION												= 0;

const size_t   CRYPTONOTE_REWARD_BLOCKS_WINDOW												= 100;
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE									= 100000; /* size of block in bytes, after which reward is calculated using block size */
const size_t   CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE										= 600;
const size_t   CRYPTONOTE_DISPLAY_DECIMAL_POINT												= 6;

const uint64_t POINT																		= UINT64_C(1000); 
const uint64_t COIN																			= UINT64_C(1000000); /* smallest atomic unit */
const uint64_t MINIMUM_FEE																	= UINT64_C(10); /* 0.000010 CCX */
const uint64_t MINIMUM_FEE_V1																= UINT64_C(100); /* 0.000100 CCX */
const uint64_t MINIMUM_FEE_BANKING															= UINT64_C(1000); /* 0.001000 CCX */
const uint64_t DEFAULT_DUST_THRESHOLD														= UINT64_C(10); /* 0.000010 CCX */  

const uint64_t DIFFICULTY_TARGET															= 120; /* two minutes */
const uint64_t EXPECTED_NUMBER_OF_BLOCKS_PER_DAY											= 24 * 60 * 60 / DIFFICULTY_TARGET; /* 720 blocks */
const size_t   DIFFICULTY_WINDOW															= EXPECTED_NUMBER_OF_BLOCKS_PER_DAY; 	
const size_t   DIFFICULTY_WINDOW_V1															= DIFFICULTY_WINDOW;
const size_t   DIFFICULTY_WINDOW_V2															= DIFFICULTY_WINDOW;
const size_t   DIFFICULTY_WINDOW_V3															= 60; /* changed for LWMA3 */
const size_t   DIFFICULTY_BLOCKS_COUNT														= DIFFICULTY_WINDOW_V3 + 1; /* added for LWMA3 */
const size_t   DIFFICULTY_CUT																= 60; /* timestamps to cut after sorting */
const size_t   DIFFICULTY_CUT_V1															= DIFFICULTY_CUT;
const size_t   DIFFICULTY_CUT_V2															= DIFFICULTY_CUT;
const size_t   DIFFICULTY_LAG																= 15; 
const size_t   DIFFICULTY_LAG_V1															= DIFFICULTY_LAG;
const size_t   DIFFICULTY_LAG_V2															= DIFFICULTY_LAG;

static_assert(2 * DIFFICULTY_CUT <= DIFFICULTY_WINDOW - 2, "Bad DIFFICULTY_WINDOW or DIFFICULTY_CUT");

const uint64_t DEPOSIT_MIN_AMOUNT															= 1 * COIN; 
const uint32_t DEPOSIT_MIN_TERM																= 5040; /* one week */	
const uint32_t DEPOSIT_MAX_TERM																= 1 * 12 * 21900; /* legacy deposts - one year */
const uint32_t DEPOSIT_MAX_TERM_V1															= 64800 * 20; /* five years */
const uint64_t DEPOSIT_MIN_TOTAL_RATE_FACTOR												= 0; /* constant rate */
const uint64_t DEPOSIT_MAX_TOTAL_RATE														= 4; /* legacy deposits */

static_assert(DEPOSIT_MIN_TERM > 0, "Bad DEPOSIT_MIN_TERM");
static_assert(DEPOSIT_MIN_TERM <= DEPOSIT_MAX_TERM, "Bad DEPOSIT_MAX_TERM");
static_assert(DEPOSIT_MIN_TERM * DEPOSIT_MAX_TOTAL_RATE > DEPOSIT_MIN_TOTAL_RATE_FACTOR, "Bad DEPOSIT_MIN_TOTAL_RATE_FACTOR or DEPOSIT_MAX_TOTAL_RATE");

const uint64_t MULTIPLIER_FACTOR															= 100; /* legacy deposits */
const uint32_t END_MULTIPLIER_BLOCK															= 12750; /* legacy deposits */

const size_t   MAX_BLOCK_SIZE_INITIAL														= CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE * 10;
const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_NUMERATOR										= 100 * 1024;
const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_DENOMINATOR										= 365 * 24 * 60 * 60 / DIFFICULTY_TARGET;

const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS									= 1;
const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS									= DIFFICULTY_TARGET * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS;

const size_t   CRYPTONOTE_MAX_TX_SIZE_LIMIT													= (CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE / 4) - CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE; /* maximum transaction size */
const size_t   CRYPTONOTE_OPTIMIZE_SIZE														= 100; /* proportional to CRYPTONOTE_MAX_TX_SIZE_LIMIT */

const uint64_t CRYPTONOTE_MEMPOOL_TX_LIVETIME												= (60 * 60 * 12); /* 12 hours in seconds */
const uint64_t CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME								= (60 * 60 * 24); /* 23 hours in seconds */
const uint64_t CRYPTONOTE_NUMBER_OF_PERIODS_TO_FORGET_TX_DELETED_FROM_POOL 					= 7; /* CRYPTONOTE_NUMBER_OF_PERIODS_TO_FORGET_TX_DELETED_FROM_POOL * CRYPTONOTE_MEMPOOL_TX_LIVETIME = time to forget tx */

const size_t   FUSION_TX_MAX_SIZE															= CRYPTONOTE_MAX_TX_SIZE_LIMIT * 2;
const size_t   FUSION_TX_MIN_INPUT_COUNT													= 12;
const size_t   FUSION_TX_MIN_IN_OUT_COUNT_RATIO												= 4;

const uint64_t UPGRADE_HEIGHT																= 1;
const uint64_t UPGRADE_HEIGHT_V2															= 1;
const uint64_t UPGRADE_HEIGHT_V3															= 12750; /* Cryptonight-Fast */
const uint64_t UPGRADE_HEIGHT_V4															= 45000; /* MixIn 2 */
const uint64_t UPGRADE_HEIGHT_V5															= 98160; /* Deposits 2.0, Investments 1.0 */
const uint64_t UPGRADE_HEIGHT_V6															= 104200; /* LWMA3 */
const uint64_t UPGRADE_HEIGHT_V7															= 195500; /* Cryptoight Conceal */
const unsigned UPGRADE_VOTING_THRESHOLD														= 90; // percent
const size_t   UPGRADE_VOTING_WINDOW														= EXPECTED_NUMBER_OF_BLOCKS_PER_DAY; 
const size_t   UPGRADE_WINDOW																= EXPECTED_NUMBER_OF_BLOCKS_PER_DAY; 

static_assert(0 < UPGRADE_VOTING_THRESHOLD && UPGRADE_VOTING_THRESHOLD <= 100, "Bad UPGRADE_VOTING_THRESHOLD");
static_assert(UPGRADE_VOTING_WINDOW > 1, "Bad UPGRADE_VOTING_WINDOW");

const char     CRYPTONOTE_BLOCKS_FILENAME[]													= "blocks.dat";
const char     CRYPTONOTE_BLOCKINDEXES_FILENAME[]											= "blockindexes.dat";
const char     CRYPTONOTE_BLOCKSCACHE_FILENAME[]											= "blockscache.dat";
const char     CRYPTONOTE_POOLDATA_FILENAME[]												= "poolstate.bin";
const char     P2P_NET_DATA_FILENAME[]														= "p2pstate.bin";
const char     CRYPTONOTE_BLOCKCHAIN_INDICES_FILENAME[]      								= "blockchainindices.dat";
const char     MINER_CONFIG_FILE_NAME[]                      								= "miner_conf.json";

} // parameters

const uint64_t START_BLOCK_REWARD															= (UINT64_C(5000) * parameters::POINT); // start reward (Consensus I)
const uint64_t FOUNDATION_TRUST																= (UINT64_C(12000000) * parameters::COIN); // locked funds to secure network  (Consensus II)
const uint64_t MAX_BLOCK_REWARD					= (UINT64_C(20) * parameters::COIN); // max reward (Consensus I)
const uint64_t REWARD_INCREASE_INTERVAL				= (UINT64_C(21900)); // aprox. 1 month (+ 0.25 CCX increment per month)

const char     CRYPTONOTE_NAME[]                             	= "conceal";
const char     GENESIS_COINBASE_TX_HEX[]			= "010a01ff0001c096b102029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd08807121017d6775185749e95ac2d70cae3f29e0e46f430ab648abbe9fdc61d8e7437c60f8";
const uint32_t GENESIS_NONCE                         	        = 10000;
const uint64_t GENESIS_TIMESTAMP				= 1527078920;

const uint8_t  TRANSACTION_VERSION_1				=  1;
const uint8_t  TRANSACTION_VERSION_2				=  2;
const uint8_t  BLOCK_MAJOR_VERSION_1				=  1; // (Consensus I)
const uint8_t  BLOCK_MAJOR_VERSION_2				=  2; // (Consensus II)
const uint8_t  BLOCK_MAJOR_VERSION_3				=  3; // (Consensus III)
const uint8_t  BLOCK_MAJOR_VERSION_4				=  4; // LWMA3
const uint8_t  BLOCK_MAJOR_VERSION_7				=  7; /* Cryptonight Conceal */
const uint8_t  BLOCK_MINOR_VERSION_0				=  0;
const uint8_t  BLOCK_MINOR_VERSION_1				=  1;

const size_t   BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT		= 10000; // by default, blocks ids count in synchronizing
const size_t   BLOCKS_SYNCHRONIZING_DEFAULT_COUNT		= 128; // by default, blocks count in blocks downloading
const size_t   COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT		= 1000;

const int      P2P_DEFAULT_PORT					= 15000;
const int      RPC_DEFAULT_PORT					= 16000;


/* P2P Network Configuration Section - This defines our current P2P network version
and the minimum version for communication between nodes */
const uint8_t  P2P_CURRENT_VERSION                           = 1;
const uint8_t  P2P_MINIMUM_VERSION                           = 1;
const uint8_t  P2P_UPGRADE_WINDOW                            = 2;

const size_t   P2P_LOCAL_WHITE_PEERLIST_LIMIT			= 1000;
const size_t   P2P_LOCAL_GRAY_PEERLIST_LIMIT			= 5000;

const size_t   P2P_CONNECTION_MAX_WRITE_BUFFER_SIZE		= 64 * 1024 * 1024; // 64MB
const uint32_t P2P_DEFAULT_CONNECTIONS_COUNT			= 8;
const size_t   P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT	= 70; // percent
const uint32_t P2P_DEFAULT_HANDSHAKE_INTERVAL			= 60; // seconds
const uint32_t P2P_DEFAULT_PACKET_MAX_SIZE			= 50000000; // 50000000 bytes maximum packet size
const uint32_t P2P_DEFAULT_PEERS_IN_HANDSHAKE			= 250;
const uint32_t P2P_DEFAULT_CONNECTION_TIMEOUT			= 5000; // 5 seconds
const uint32_t P2P_DEFAULT_PING_CONNECTION_TIMEOUT		= 2000; // 2 seconds
const uint64_t P2P_DEFAULT_INVOKE_TIMEOUT			= 60 * 2 * 1000; // 2 minutes
const size_t   P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT		= 5000; // 5 seconds
const char     P2P_STAT_TRUSTED_PUB_KEY[]			= "f7061e9a5f0d30549afde49c9bfbaa52ac60afdc46304642b460a9ea34bf7a4e";

// Seed Nodes
const std::initializer_list<const char*> SEED_NODES = {
		"212.237.59.97:15000", // Gamma
		"188.213.165.210:15000", // Omega
		"89.40.118.85:15000", // Delta
		"94.177.245.107:15000", // Lambda
		"199.175.54.195:15000" // Godspeed
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
		{5000, "5fdf72e7c106d429231fb980bc336e736464403dee77c7c5ef5e87305644965d"},
		{10000, "55cf271a5c97785fb35fea7ed177cb75f47c18688bd86fc01ae66508878029d6"},
		{15000, "bbd63d8be2087567cefa44ff50a7d240a2b734be3efb26bfccc5391cbcb9e5ec"},
		{20000, "52533de7f1596154c6954530ae8331fe4f92e92d476f097c6d7d20ebab1c2748"},
		{25000, "ce97a9151180cbbfc52d184b6b60b9039a8a113ddd741bf520758e8b4bc21d98"},
		{30000, "50b5d84ac0b8abfe25669aac8514505c4c5f7ffd8e2bba0b52ab64f600d90796"},
		{35000, "aae3a729d2b74eebd8b2c41496c9170fff60966f5429fffce0eb6703e0092a9f"},
		{40000, "ae2ed29163a57396f11c743400e55fba3f6b8e6bb6473f421c48ff8c87447ad0"},
		{45000, "4b822f1395996129be1140e591925a849da7e44b0ef350f29e12779eb9010f5e"},
		{50000, "8ad7969ca5d3cf48f784d33b60d1ea00bfb35b632447584e5181b194f3bb9cd6"},		
		{55000, "5180f25d32fa5f43c4b38d03b4826c0dfaedd737334d4a482b62026388524474"},
		{60000, "22b1a161de2318b1a83ae0e3d1d04a2c420accccadd861aa8ad6365ec630ce04"},
		{65000, "83c868f27ba7589211b74e1bbd8b06fea920e620f7a5e6615d9c0fd8d09bc7b2"},
		{70000, "4ef8a3c59b04ad8ae335fee0b5df0c1b114dda57d13232741d82c4984bf22bed"},
		{75000, "d8e0ce611c36b439721c88aa79c005cce0f20b9d91a7e8499c26d4f8b84bb9d1"},
		{80000, "a60bd6b446c5b09997b5b70f31c56f35358657a673dcf56213a163fb6516750d"},
		{85000, "3f9e64ee60f878b2bb3700d01d287092eb7da93ca76250b96a8c2c3e6e3048fd"},
		{90000, "9985f631d4b2c15388e8c3797a1384b4610b13ff3852bc6d8f125ea4e13fdd22"},
		{95000, "aae10001900f721b1ad90b377cedefeb5561c96e36987e549ddcbeebd9c54b21"},
		{100000, "1ccef60fb31646fc1745ccb42167f2e2efcf953a83b99ff4b6a39c99eb37d0e5"},
		{105000, "697ad17c6f3de29181ef380271cc3bf6df15181b3e68997517cf657194f0c6d4"},
		{110000, "1b80bf8355ea023de7ed3367881ef111dfdb3aaeb25db3a0d6cad4c3cc0bb4bd"},
		{115000, "6000e495db0cdf043f474949e3e708a8c670f99f1232711be4789fecffbfb4d8"},
		{120000, "f621bd615716b75716eb89714d60481077a16b1df4046bf829f6d09b1c8e58a6"},
		{125000, "0a783373ed67adbd3bc3904496e813bc481018e9f5a91681ff74416de0f805e4"},
		{130000, "deb2514d03e2faf1c63b55f707b1524665ca4bd71cace3f4e8f0de58f32ecc41"},
		{135000, "466d509486fe791b7af6d222c026eba29848b66302c6fd44dd26386d98acaa5a"},
		{140000, "c439524c13187bb6008acb1e9999317aa44e8d1cd75c96faae78831f6b961bac"},
		{145000, "9e1c4319e40a2c61abc5d405399f2add93a1394d77eb8eeaf4115f41a06e1d7b"},
		{150000, "90cc70379ea81d47df998e8b9928ba9191968035ae79ec1cb429c64a55497e03"},
		{155000, "c2f913d25d0367927419c85d28c934bdc56cd8f951e638f1dc50c3a125def561"},
		{160000, "4176bdff06416934d7766a6c2f6279d048cfdc516019a0580ea19c1d003038cc"},
		{165000, "ed5070b18095a4903509045a478d9314aa4fffb0df6d1e475a4b2b33258db290"},		
		{170000, "50e3af756e96115011c8e4d138852e1f4835da805ca5ccd826f81593a53f4bd3"}	
};

} // CryptoNote

#define ALLOW_DEBUG_COMMANDS

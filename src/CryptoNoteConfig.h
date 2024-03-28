// Copyright (c) 2012-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2023 Conceal Network & Conceal Devs
//
//
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <initializer_list>

namespace cn
{
	namespace parameters
	{

		const uint64_t CRYPTONOTE_MAX_BLOCK_NUMBER = 500000000;
		const size_t CRYPTONOTE_MAX_BLOCK_BLOB_SIZE = 500000000;
		const size_t CRYPTONOTE_MAX_TX_SIZE = 1000000000;
		const uint64_t CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 0x7ad4; /* ccx7 address prefix */
		const size_t CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW = 10;			 /* 20 minutes */
		const uint64_t CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT = 60 * 60 * 2; /* two hours */
		const uint64_t CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V1 = 360;		 /* changed for LWMA3 */
		const uint64_t CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE = 10;		 /* 20 minutes */

		const size_t BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW = 30;
		const size_t BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V1 = 11; /* changed for LWMA3 */

		const uint64_t MONEY_SUPPLY = UINT64_C(200000000000000); /* max supply: 200M (Consensus II) */

		const uint32_t ZAWY_DIFFICULTY_BLOCK_INDEX = 0;
		const size_t ZAWY_DIFFICULTY_FIX = 1;
		const uint8_t ZAWY_DIFFICULTY_BLOCK_VERSION = 0;

		const size_t CRYPTONOTE_REWARD_BLOCKS_WINDOW = 100;
		const size_t CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE = 100000; /* size of block in bytes, after which reward is calculated using block size */
		const size_t CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE = 600;
		const size_t CRYPTONOTE_DISPLAY_DECIMAL_POINT = 6;

		const uint64_t POINT = UINT64_C(1000);
		const uint64_t COIN = UINT64_C(1000000);			  /* smallest atomic unit */
		const uint64_t MINIMUM_FEE = UINT64_C(10);			  /* 0.000010 CCX */
		const uint64_t MINIMUM_FEE_V1 = UINT64_C(100);		  /* 0.000100 CCX */
		const uint64_t MINIMUM_FEE_V2 = UINT64_C(1000);		  /* 0.001000 CCX */
		const uint64_t MINIMUM_FEE_BANKING = UINT64_C(1000);  /* 0.001000 CCX */
		const uint64_t DEFAULT_DUST_THRESHOLD = UINT64_C(10); /* 0.000010 CCX */

		const uint64_t DIFFICULTY_TARGET = 120;												 /* two minutes */
		const uint64_t EXPECTED_NUMBER_OF_BLOCKS_PER_DAY = 24 * 60 * 60 / DIFFICULTY_TARGET; /* 720 blocks */
		const size_t DIFFICULTY_WINDOW = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;
		const size_t DIFFICULTY_WINDOW_V1 = DIFFICULTY_WINDOW;
		const size_t DIFFICULTY_WINDOW_V2 = DIFFICULTY_WINDOW;
		const size_t DIFFICULTY_WINDOW_V3 = 60; /* changed for LWMA3 */
		const size_t DIFFICULTY_WINDOW_V4 = 60;
		const size_t DIFFICULTY_BLOCKS_COUNT = DIFFICULTY_WINDOW_V3 + 1;	/* added for LWMA3 */
		const size_t DIFFICULTY_BLOCKS_COUNT_V1 = DIFFICULTY_WINDOW_V4 + 1; /* added for LWMA1 */
		const size_t DIFFICULTY_CUT = 60;									/* timestamps to cut after sorting */
		const size_t DIFFICULTY_CUT_V1 = DIFFICULTY_CUT;
		const size_t DIFFICULTY_CUT_V2 = DIFFICULTY_CUT;
		const size_t DIFFICULTY_LAG = 15;
		const size_t DIFFICULTY_LAG_V1 = DIFFICULTY_LAG;
		const size_t DIFFICULTY_LAG_V2 = DIFFICULTY_LAG;
		const size_t MINIMUM_MIXIN = 5;

		static_assert(2 * DIFFICULTY_CUT <= DIFFICULTY_WINDOW - 2, "Bad DIFFICULTY_WINDOW or DIFFICULTY_CUT");

		const uint64_t DEPOSIT_MIN_AMOUNT = 1 * COIN;
		const uint32_t DEPOSIT_MIN_TERM = 5040;				 /* one week */
		const uint32_t DEPOSIT_MAX_TERM = 1 * 12 * 21900;	 /* legacy deposts - one year */
		const uint32_t DEPOSIT_MAX_TERM_V1 = 64800 * 20;	 /* five years */
		const uint32_t DEPOSIT_MIN_TERM_V3 = 21900;			 /* consensus 2019 - one month */
		const uint32_t DEPOSIT_MAX_TERM_V3 = 1 * 12 * 21900; /* consensus 2019 - one year */
		const uint32_t DEPOSIT_HEIGHT_V3 = 413400;			 /* consensus 2019 - deposts v3.0 */
		const uint64_t DEPOSIT_MIN_TOTAL_RATE_FACTOR = 0;	 /* constant rate */
		const uint64_t DEPOSIT_MAX_TOTAL_RATE = 4;			 /* legacy deposits */
		const uint32_t DEPOSIT_HEIGHT_V4 = 1162162;			 /* enforce deposit terms */
		const uint32_t BLOCK_WITH_MISSING_INTEREST = 425799; /*  */

		static_assert(DEPOSIT_MIN_TERM > 0, "Bad DEPOSIT_MIN_TERM");
		static_assert(DEPOSIT_MIN_TERM <= DEPOSIT_MAX_TERM, "Bad DEPOSIT_MAX_TERM");
		static_assert(DEPOSIT_MIN_TERM * DEPOSIT_MAX_TOTAL_RATE > DEPOSIT_MIN_TOTAL_RATE_FACTOR, "Bad DEPOSIT_MIN_TOTAL_RATE_FACTOR or DEPOSIT_MAX_TOTAL_RATE");

		const uint64_t MULTIPLIER_FACTOR = 100;		 /* legacy deposits */
		const uint32_t END_MULTIPLIER_BLOCK = 12750; /* legacy deposits */

		const size_t MAX_BLOCK_SIZE_INITIAL = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE * 10;
		const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_NUMERATOR = 100 * 1024;
		const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_DENOMINATOR = 365 * 24 * 60 * 60 / DIFFICULTY_TARGET;

		const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS = 1;
		const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS = DIFFICULTY_TARGET * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS;

		const size_t CRYPTONOTE_MAX_TX_SIZE_LIMIT = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE - CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE; /* maximum transaction size */
		const size_t CRYPTONOTE_OPTIMIZE_SIZE = 100;																					/* proportional to CRYPTONOTE_MAX_TX_SIZE_LIMIT */

		const uint64_t CRYPTONOTE_MEMPOOL_TX_LIVETIME = (60 * 60 * 12);					/* 1 hour in seconds */
		const uint64_t CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME = (60 * 60 * 12);	/* 24 hours in seconds */
		const uint64_t CRYPTONOTE_NUMBER_OF_PERIODS_TO_FORGET_TX_DELETED_FROM_POOL = 7; /* CRYPTONOTE_NUMBER_OF_PERIODS_TO_FORGET_TX_DELETED_FROM_POOL * CRYPTONOTE_MEMPOOL_TX_LIVETIME  = time to forget tx */

		const size_t FUSION_TX_MAX_SIZE = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE * 30 / 100;
		const size_t FUSION_TX_MIN_INPUT_COUNT = 12;
		const size_t FUSION_TX_MIN_IN_OUT_COUNT_RATIO = 4;

		const uint64_t UPGRADE_HEIGHT = 1;
		const uint64_t UPGRADE_HEIGHT_V2 = 1;
		const uint64_t UPGRADE_HEIGHT_V3 = 12750;	  /* Cryptonight-Fast */
		const uint64_t UPGRADE_HEIGHT_V4 = 45000;	  /* MixIn 2 */
		const uint64_t UPGRADE_HEIGHT_V5 = 98160;	  /* Deposits 2.0, Investments 1.0 */
		const uint64_t UPGRADE_HEIGHT_V6 = 104200;	  /* LWMA3 */
		const uint64_t UPGRADE_HEIGHT_V7 = 195765;	  /* Cryptoight Conceal */
		const uint64_t UPGRADE_HEIGHT_V8 = 661300;	  /* LWMA1, CN-GPU, Halving */
		const unsigned UPGRADE_VOTING_THRESHOLD = 90; // percent
		const size_t UPGRADE_VOTING_WINDOW = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;
		const size_t UPGRADE_WINDOW = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;


		const uint64_t TESTNET_UPGRADE_HEIGHT = 1;
		const uint64_t TESTNET_UPGRADE_HEIGHT_V2 = 1;
		const uint64_t TESTNET_UPGRADE_HEIGHT_V3 = 12;	  /* Cryptonight-Fast */
		const uint64_t TESTNET_UPGRADE_HEIGHT_V4 = 24;	  /* MixIn 2 */
		const uint64_t TESTNET_UPGRADE_HEIGHT_V5 = 36;	  /* Deposits 2.0, Investments 1.0 */
		const uint64_t TESTNET_UPGRADE_HEIGHT_V6 = 48;	  /* LWMA3 */
		const uint64_t TESTNET_UPGRADE_HEIGHT_V7 = 60;	  /* Cryptoight Conceal */
		const uint64_t TESTNET_UPGRADE_HEIGHT_V8 = 72;	  /* LWMA1, CN-GPU, Halving */

		const uint32_t TESTNET_DEPOSIT_MIN_TERM_V3 = 30;		/* testnet deposits 1 month -> 1 hour */
		const uint32_t TESTNET_DEPOSIT_MAX_TERM_V3 = 12 * 30;	/* testnet deposits 1 year -> 12 hour */
		const uint32_t TESTNET_DEPOSIT_HEIGHT_V3 = 60;		
		const uint32_t TESTNET_DEPOSIT_HEIGHT_V4 = 300000;
		const uint32_t TESTNET_BLOCK_WITH_MISSING_INTEREST = 0; /* testnet is not impacted */

		static_assert(0 < UPGRADE_VOTING_THRESHOLD && UPGRADE_VOTING_THRESHOLD <= 100, "Bad UPGRADE_VOTING_THRESHOLD");
		static_assert(UPGRADE_VOTING_WINDOW > 1, "Bad UPGRADE_VOTING_WINDOW");

		const char CRYPTONOTE_BLOCKS_FILENAME[] = "blocks.dat";
		const char CRYPTONOTE_BLOCKINDEXES_FILENAME[] = "blockindexes.dat";
		const char CRYPTONOTE_BLOCKSCACHE_FILENAME[] = "blockscache.dat";
		const char CRYPTONOTE_POOLDATA_FILENAME[] = "poolstate.bin";
		const char P2P_NET_DATA_FILENAME[] = "p2pstate.bin";
		const char CRYPTONOTE_BLOCKCHAIN_INDICES_FILENAME[] = "blockchainindices.dat";
		const char MINER_CONFIG_FILE_NAME[] = "miner_conf.json";
		const char CRYPTONOTE_CHECKPOINT_FILENAME[] = "checkpoint.dat";

	} // namespace parameters

	const uint64_t START_BLOCK_REWARD = (UINT64_C(5000) * parameters::POINT);  // start reward (Consensus I)
	const uint64_t FOUNDATION_TRUST = (UINT64_C(12000000) * parameters::COIN); // locked funds to secure network  (Consensus II)
	const uint64_t MAX_BLOCK_REWARD = (UINT64_C(15) * parameters::COIN);	   // max reward (Consensus I)
	const uint64_t MAX_BLOCK_REWARD_V1 = (UINT64_C(6) * parameters::COIN);
	const uint64_t REWARD_INCREASE_INTERVAL = (UINT64_C(21900));			   // aprox. 1 month (+ 0.25 CCX increment per month)

	const char BLOCKCHAIN_DIR[] = "conceal";
	const char GENESIS_COINBASE_TX_HEX[] = "010a01ff0001c096b102029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd08807121017d6775185749e95ac2d70cae3f29e0e46f430ab648abbe9fdc61d8e7437c60f8";
	const uint32_t GENESIS_NONCE = 10000;
	const uint64_t GENESIS_TIMESTAMP = 1527078920;

	const uint64_t TESTNET_GENESIS_TIMESTAMP = 1632048808;

	const uint8_t TRANSACTION_VERSION_1 = 1;
	const uint8_t TRANSACTION_VERSION_2 = 2;
	const uint8_t BLOCK_MAJOR_VERSION_1 = 1; // (Consensus I)
	const uint8_t BLOCK_MAJOR_VERSION_2 = 2; // (Consensus II)
	const uint8_t BLOCK_MAJOR_VERSION_3 = 3; // (Consensus III)
	const uint8_t BLOCK_MAJOR_VERSION_4 = 4; // LWMA3
	const uint8_t BLOCK_MAJOR_VERSION_7 = 7; /* Cryptonight Conceal */
	const uint8_t BLOCK_MAJOR_VERSION_8 = 8; /* LWMA1, CN-GPU, Halving */
	const uint8_t BLOCK_MINOR_VERSION_0 = 0;
	const uint8_t BLOCK_MINOR_VERSION_1 = 1;

	const size_t BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT = 10000; // by default, blocks ids count in synchronizing
	const size_t BLOCKS_SYNCHRONIZING_DEFAULT_COUNT = 128;		 // by default, blocks count in blocks downloading
	const size_t COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT = 1000;
    const size_t COMMAND_RPC_GET_OBJECTS_MAX_COUNT = 1000;

	const int P2P_DEFAULT_PORT = 15000;
	const int RPC_DEFAULT_PORT = 16000;
    const int PAYMENT_GATE_DEFAULT_PORT = 8070;

	const int TESTNET_P2P_DEFAULT_PORT = 15500;
	const int TESTNET_RPC_DEFAULT_PORT = 16600;
    const int TESTNET_PAYMENT_GATE_DEFAULT_PORT = 8770;

	/* P2P Network Configuration Section - This defines our current P2P network version
	and the minimum version for communication between nodes */
	const uint8_t P2P_VERSION_1 = 1;
	const uint8_t P2P_VERSION_2 = 2;
	const uint8_t P2P_CURRENT_VERSION = 2;
	const uint8_t P2P_MINIMUM_VERSION = 1;
	const uint8_t P2P_UPGRADE_WINDOW = 2;

	// This defines the minimum P2P version required for lite blocks propogation
	const uint8_t P2P_LITE_BLOCKS_PROPOGATION_VERSION = 2;
	const uint8_t P2P_CHECKPOINT_LIST_VERSION = 2;

	const size_t P2P_LOCAL_WHITE_PEERLIST_LIMIT = 1000;
	const size_t P2P_LOCAL_GRAY_PEERLIST_LIMIT = 5000;

	const size_t P2P_CONNECTION_MAX_WRITE_BUFFER_SIZE = 64 * 1024 * 1024; // 64MB
	const uint32_t P2P_DEFAULT_CONNECTIONS_COUNT = 8;
	const size_t P2P_DEFAULT_ANCHOR_CONNECTIONS_COUNT = 2;
	const size_t P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT = 70; // percent
	const uint32_t P2P_DEFAULT_HANDSHAKE_INTERVAL = 60;			 // seconds
	const uint32_t P2P_DEFAULT_PACKET_MAX_SIZE = 50000000;		 // 50000000 bytes maximum packet size
	const uint32_t P2P_DEFAULT_PEERS_IN_HANDSHAKE = 250;
	const uint32_t P2P_DEFAULT_CONNECTION_TIMEOUT = 5000;	   // 5 seconds
	const uint32_t P2P_DEFAULT_PING_CONNECTION_TIMEOUT = 2000; // 2 seconds
	const uint64_t P2P_DEFAULT_INVOKE_TIMEOUT = 60 * 2 * 1000; // 2 minutes
	const size_t P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT = 5000;  // 5 seconds
	const size_t P2P_CHECKPOINT_LIST_RE_REQUEST = 300;  // 5 minutes
	const char P2P_STAT_TRUSTED_PUB_KEY[] = "f7061e9a5f0d30549afde49c9bfbaa52ac60afdc46304642b460a9ea34bf7a4e";

	// Seed Nodes
	const std::initializer_list<const char *> SEED_NODES = {
		"185.58.227.32:15000", // UK
		"185.35.64.209:15000",	 // France 
		"94.177.245.107:15000"	 // Germany
	};

	const std::initializer_list<const char *> TESTNET_SEED_NODES = {
		"161.97.145.65:15500",
		"161.97.145.65:15501"
	};

	struct CheckpointData
	{
		uint32_t height;
		const char *blockId;
	};

#ifdef __GNUC__
	__attribute__((unused))
#endif

	const char DNS_CHECKPOINT_DOMAIN[] = "checkpoints.conceal.id";
	const char TESTNET_DNS_CHECKPOINT_DOMAIN[] = "testpoints.conceal.gq";    

	// Blockchain Checkpoints:
	// {<block height>, "<block hash>"},
	const std::initializer_list<CheckpointData>
		CHECKPOINTS = {
			{ 100000, "9b58762e759cd02cef493b310f95d73c36a907ea5c6ab3953b6b304651d3f291"},
			{ 200000, "07a7d796f64309b84558b7fc44902dd65dbf1bfd4b727b9f48b9f358b9c7e4f5"},
			{ 300000, "d723e8964fd416d1fbb5c8d616b0f7aa2f61cd7dbf1c7f6654daf751915f7967"},
			{ 400000, "1b4fbfd19b8502af420b8a38a6ac610a6b9e30a164af4da742092bdf9887d086"},
			{ 500000, "47cce8323f661b07048be52a2c6cca29f9f49aa5cc3282253f6e0a54dd3f1d56"},
			{ 600000, "086035e107dd22b8be63a86c28f750124306914cd47f46d0c69208580a4d5f9e"},
			{ 700000, "b499559416d7198a01bfe6c02d94f0e0c4ff785158ffa6f690e736317372acc3"},
			{ 800000, "3c5207312c528b80df4392f5f6a99cb23457d018c08747b072e7ab86a83025b4"},
			{ 900000, "4bb3c7c5b7bd24dac9440f3f5797348716b1b1a6335a0de0769f0ea0f409e447"},
			{1000000, "3148b677584d71f5e75b5c7431d525aa6b8d8a7d5e4d01ea4adadc75adcc64d5"},
			{1100000, "480f890918c2c550696109391b55ca465fdc04a4302b403a90ccf00c37a282d9"},
			{1200000, "f730a854601d55c774755a6d8f561ab2454dfb763340f23b7218c24ae14163f4"},
			{1300000, "d130491c88398a2812849d27fa09b884085d7f2feffe5d5da0f56cd8039469f5"},
			{1400000, "570682ba6a7614081071a9f111b7e737789e1c7d445635bcd1f830211a311a19"},
			{1500000, "43f173aba14a6b2d023c07796683789470395d7f15ebd24991617b4cc81c4f8c"}
	};

	const std::initializer_list<CheckpointData> TESTNET_CHECKPOINTS = {
		{0, "850ac16022f4dddab624fad3f9049dba80592c8ea51a5dff19fefeb386e536b1"},
		{25000, "bdde29c10211c911947e1e0d602309e95fb915372f3317690c7860ef451a78e7"},
		{50000, "ceb6c3e01ccd832779060a65a348ef38cab067bc951df11aa38332b8c2b7d299"},
		{75000, "cf2e2c171107e05aa9932e7f66a3df5117e5c1a4b084bb9b0c9c79300ebb82ce"},
		{100000, "a2c7179f5e7ea541ba8ed929044600b9cbf2aa20e5cf1d29cb93439fda4431ba"},
		{125000, "8a1737f2eee125776dff01288ada59ced95f9ab3a3fcbbed9c32a3dc20cad033"},
		{150000, "ffacefc66ca2a8e8f86cb857682568f1ba6280cd7d0363d4c322947ec32ccfe7"},
		{175000, "a053a4d65eeabafdb2766d1d70969c81849d6f00d9bd2bc88913c578bc462336"},
		{200000, "64fdf3fc7cb0cf0596624370324c898dc70f6792f654490fad2a2211ad07fd5f"},
		{225000, "a39f3d7a0891f9f58b6419d61cd954411fbff93e122205d7f46ecec9948470eb"},
		{250000, "8cdecb3de4ba342e4a817cbd02e81238574391864182711b193c05403eb1c87d"},
		{275000, "61cab9e0d8133e58ae8544c320e43b433945ae32d9599df8eb41c0398c1377b8"},
		{300000, "48ee76f5c16deb56f5d4e5acd632c78f8d5b71b1371514459066c224e4cd82dc"},
		{325000, "2c2566bdabcd1ae1fc328bdeb6b55da56414847101e767a808cf9d9e2e4a1358"},
		{350000, "14aaebe52ec107fac28179ae1056821c112932e74bfa971e9040730b9a797b09"},
		{375000, "11858ed98655337da5f959194e96f4405a5811bb96afdb8ad1e450a62face3c8"},
		{400000, "1de31d8d458e0b0355fd7ff14ab757e6991b1b66c5e63156849891118843bc26"},
		{425000, "63c2ff4cb122d76acb4c8939996d2486fcf6d347f801ad2b3b7356ffab0f1b85"},
		{450000, "aa34be98d020af37faf6b78ffe51561a3c001b0322f726da75574e9c8c86dc7b"},
		{475000, "d6224b5f69024e101487da252b3efc75a8b035810eae32d51656e2dee42cc757"},
		{500000, "f0aa507fdd0cc13698381002ee962e5943db1ade6d20c27eaf8544a3d0dc8230"},
		{525000, "14c6a1f432d577861d0c6a510bffa18bf8c09629f3553d9d475d5a46421ec8fa"},
		{550000, "29699db2cc98d5e9aee8c3abcc0d64604d121692aae50f8bb7e642083ba8f7fc"},
		{575000, "f32d55598a3df5e83a28a1c0f67ecc836c7fc2d710be7656fddd3d07b0f3e88f"},
		{600000, "b44f8e1a1bcc9841a174f76ff04832e7090851dfd0a2e7ac9ce68990c5b21ca3"},
		{625000, "774e1129050fe4d09661ddf335f9b44922b1e44637bbfd30f44a89142e7bf8f9"}
	};

} // namespace cn


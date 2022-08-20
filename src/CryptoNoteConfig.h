// Copyright (c) 2011-2017 The Cryptonote Developers
// Copyright (c) 2017-2018 The Circle Foundation & Conceal Devs
// Copyright (c) 2018-2022 Conceal Network & Conceal Devs
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

		static_assert(0 < UPGRADE_VOTING_THRESHOLD && UPGRADE_VOTING_THRESHOLD <= 100, "Bad UPGRADE_VOTING_THRESHOLD");
		static_assert(UPGRADE_VOTING_WINDOW > 1, "Bad UPGRADE_VOTING_WINDOW");

		const char CRYPTONOTE_BLOCKS_FILENAME[] = "blocks.dat";
		const char CRYPTONOTE_BLOCKINDEXES_FILENAME[] = "blockindexes.dat";
		const char CRYPTONOTE_BLOCKSCACHE_FILENAME[] = "blockscache.dat";
		const char CRYPTONOTE_POOLDATA_FILENAME[] = "poolstate.bin";
		const char P2P_NET_DATA_FILENAME[] = "p2pstate.bin";
		const char CRYPTONOTE_BLOCKCHAIN_INDICES_FILENAME[] = "blockchainindices.dat";
		const char MINER_CONFIG_FILE_NAME[] = "miner_conf.json";

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
	const uint8_t P2P_CURRENT_VERSION = 1;
	const uint8_t P2P_MINIMUM_VERSION = 1;
	const uint8_t P2P_UPGRADE_WINDOW = 2;

	// This defines the minimum P2P version required for lite blocks propogation
	const uint8_t P2P_LITE_BLOCKS_PROPOGATION_VERSION = 3;

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

	// Blockchain Checkpoints:
	// {<block height>, "<block hash>"},
	const std::initializer_list<CheckpointData>
		CHECKPOINTS = {
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
			{350000, "f08aad1562ceee3a6c8147846bb3e5dd15b3168007f588ab68bd8ee816eb386d"},
			{360000, "cd910715be7dccc155ad3e8a6311f1bbcfaffe3ee25186c454ed27ee61faa977"},
			{370000, "6c4a86be9a1f697cadc38d21718803c43f49bf60c71ae253293e29ebac6efe31"},
			{380000, "620709892437c28deb72a56e6a91960f481aa682d8dd8652f792fb33e6683ef5"},
			{390000, "d2ff4c39b4aed7ef08a99a00b9823bed44581e866180ae3daa8b8e990b57ec63"},
			{400000, "9b7302daf5e5933b9a3e75a12651eaad83bea7d0058191cf65eb20985fe281c5"},
			{410000, "4f343219e57f78c1063f4b4c5be6cb5a10599d64d36e9f686f7046469a6c7e73"},
			{420000, "56b2fec8f7a55c9e2960d7224999c2e8c83a77f051931ba1673e071e7bcd6851"},
			{430000, "6d6e24f6c518c9cc24a05967fd1bbb3aeffb670fd7329d0a24053662a2305d9e"},
			{440000, "6a0138801d48150985045bc671c752f8209d084adad3624a57edd22f9edbef78"},
			{460000, "dec1da5df01c3cdf5d25a577816c93de58dfb6dd6b073619c5cbd50aedefceb7"},
			{470000, "1d07fd8995e17429143202da00138f0bfcbdd20aa5ddbba18ac762bc473ffd77"},
			{480000, "c896df9146e8f09f6205496dfa1e28037c8223f531546d2d64119068a6d1db1d"},
			{490000, "faa86e0b546f7655e829dcd8e967a52d9fa933c832863a648df30cc0e8771fa8"},
			{500000, "df5b2b47960ecd7809f037de44c6817640283e13323a36fe3dd894f3b2b3c5e1"},
			{510000, "db784d782ac463fbfbf221b417166a80ca1451f8895a1e3027bd19de2952c9bc"},
			{520000, "70b9c6945d8156d97d5f337b22ec8a4f77fa8af3b89d63e3fe6b834a03f7a613"},
			{530000, "f1b6f4018201e9c498e2b441f8e20f6e562e5d45c69008fe74caa7baa0a16611"},
			{550000, "1b922d13de891cd9f7224bd1a3c879a1d7634505f5f562623d7a487d44211327"},
			{570000, "9efe8868099afd1f6b17de773da0f5baebf2ace666bf5e599188c64d27cd429f"},
			{580000, "39ecee8d292c4e0440467b28ead6ed96c480ac85bec4fdba1e4c14b49b08077e"},
			{590000, "d6201b072cfed013b0e1091517624ca72bdd1ef147143356a1f951dd3241dd88"},
			{600000, "9f87dd161e37e9dbbcd86a3fafe8e1dec8c54194251ca0c36c646173db12c115"},
			{610000, "9c95678a27c5bde2b53efdae5c20a5528f134c4ff75737dee3e3d63b4d79c7ba"},
			{620000, "e5de278b0ea676855873663a32a2d21bc6d98cffcb133e249c8219fb0fcdc3eb"},
			{630000, "762c8269af35d53408d806d453b8ca6f19fc9e83048bb8d985502344f1d5e08a"},
			{640000, "24e1ac8aff3e1e7850c06a377c68b2ea3afe53477b710b988b6b456383a50081"},
			{650000, "4587f3196487cdf12e701bebe30340669374e39b6e0ca7a3c32d6b522be44570"},
			{660000, "8d8338dab606e4010f1fa53bc0ef268c98f63bf727150184bfedbea37c40026d"},
			{670000, "26350d735576a40e4d4e628b57186f4c7f85b3bea6c15f28554706f4c78c3837"},
			{680000, "6774c21beb0f4e2383069da967654ce4d26743f313aa7c705f222c055fcf0e05"},
			{690000, "33e1ddd732edfb8e850cdca304ae398a2eb495fd2a6876ff759725788f5b1135"},
			{700000, "a6b8e9707cd5ac93931b3fcc6bb516d11e7cb840bf49c8d3712bdeba605557be"},
			{710000, "922f1ca029163e58a24d6573e7de6bf9bcecc16ae164ebfd0285c0eda57d4eec"},
			{750000, "0e22dabd4379040815f078525ed02ae95e26ae92bc9eb35628a5d588e176b900"},
			{760000, "ecc64815b44b4c0c67340ff7e0d9fefee2cfbfbed10d61260c49bbe98aeb6ea2"},
			{770000, "7c97512a8ffdefa3e97809779841e48ffa6b68bd8a5bf90fcb59c1547f5ad90f"},
			{780000, "7acd1d5c843245bf1cc12a966f388111e2258c029595d31539b9257bb1217e61"},
			{790000, "580580b3b628b68532d9b141cc5e3299ebdc6e421c58ed155916c418144704ff"},
			{800000, "bb0dc113b1bfde3f06bfa341ba7d6de6ea82cebc86b98fc3d122b695e0bfdebe"},
			{810000, "c8a8e81032d66b137a99087ddd6ca6289040cb336012d4248ff616e1e7abb5b3"},
			{820000, "98a8373323adca6f09048177c774783150bcf25b62e26c804fd33461fc1af09b"},
			{830000, "7007d4331703233f48a3f1a2e824734d02ec81e7da46b7e00a2c354edb8de357"},
			{840000, "eae669f44a964ce5501b971139ee04657b62dc2535e9305857a1c9e2e839790a"},
			{850000, "e354bf503e8273a90d5d338ce78966ee820b69490b3a3183f0765281429d2f77"},
			{860000, "11b35a3f0c78686d75991d5bf65e868e565ea4ba15e96c9b0f643a4d9983eedf"},
			{870000, "21fef5a9cbb6ebe8ba271bdb55ed4d3ccd0468ab31a77f634cabc893e2a9661e"},
			{880000, "a861367503fec46c12f8ae957438a389c4de7b3c267cfdb21a7be29337885e10"},
			{890000, "092b29ab3369d0227239f0604d57ab91a3e1794ca3abe0c75fd5e69acb611a66"},
			{900000, "4cb49bba6abe10464db1075ed7125172e639ec9ee03f08ddb4dbed318d9dbfb9"},
			{910000, "2b3064cc74a3780e55c6eba250ec1e6ad6deb7ea6188430c07ba6fb3b60b63f0"},
			{920000, "961be71463b51c41fa5fbd43213b43b4b66173c26c1cd29d55881aae29a8ce07"},
			{930000, "511d2784b65e9ff0da55358834b88319b653aa1b5bf5a0f76c25c0467c16c536"},
			{940000, "781dba46e6a2d8a7ba4129e32e7764c519e011d8967672f7873599eb5449f760"},
			{950000, "5404a8e358ddf55c018bff01f4c112fc5ea291ac4429524d8b5c496ede697246"},
			{960000, "5133cf16ece3cba43a199c11dc80ecd81a8dbda4ce0c517f78a3e400efa6a730"},
			{970000, "e9ae491f24acd824dbcd43178c3070878e0ec32b494fe293b153fd19b2e8428a"},
			{980000, "c99c74ae4e3ff43ba2c93cd8a6d3ed52598c7bda6b42fac18d9569e29c5753aa"},
			{990000, "0412793a1650aeba2cf9ab7c32bee26668e8e997a55e97d65c609e903f9e04ec"},
			{1000000, "6ad9d4ccc9666b31481079374e573c20ebdf2d63862da8fcc2c45d13093b93ba"},
			{1010000, "f341d678cfbd5d488bbc179bc54fc92587dad7fb29823facc95f3e26158a722d"},
			{1020000, "e3dae82d451358ac300e3960695784efb7d76833e620d75196cd0af9db0568bb"},
			{1030000, "4d1d4f3174e684c93cb3dc0e261776224b02f6c0bd2c7ea91b6f923b42e7b321"},
			{1040000, "c894d5f5a6637d7ec50c9a09722059aa5e878bf1eda7210a1f6c4e61eca770ef"},
			{1050000, "8a0f5df47ce13a082423743651f22209cef78f46b933ced7642d1f6b8d8d80be"},
			{1060000, "59d6ee913234a03eefa023a3a12487c5244973e17a9c9dcc61bd7cb3c5dcc426"},
			{1070000, "0895514f95977bdbb220550f00ec38962a6e672a125dad115f2408600fd9b593"},
			{1080000, "58a71c6f06c16e3337f5dd43a018d2768b2f6ca2d97a7bee9e81b2b2bae866c3"}
	};

    const std::initializer_list<CheckpointData> TESTNET_CHECKPOINTS = {
        {0, "850ac16022f4dddab624fad3f9049dba80592c8ea51a5dff19fefeb386e536b1"},
        {5000, "e232d411f2264e185bba87cad56053bd35596d629faf9d6e6cddc410d3fdf3de"},
        {10000, "ad40d09ed6194709da7aabc893a71a7d28745386b765ff29cbab34fc1df83696"},
        {15000, "a5874d60032c365150acaf528e06b403471560a7bee1faea2b8ac3d09b4e06c5"},
        {20000, "41b738d741339a9609eceade3c5adffd54b89a5c70274d7edeade1ebdfd483a7"},
        {25000, "bdde29c10211c911947e1e0d602309e95fb915372f3317690c7860ef451a78e7"},
        {30000, "2bfd5dcd511b836755e413db911301a7e0bbb324c43fe38c4e2ec696996cb557"},
        {35000, "dc0f5be53085ffe347c92ff551a4e8757759ef30bdd4589a636d976c580ce4c3"},
        {40000, "724886ca65c2c90f3507cabd13e14ed998222193f2d4a1cfd043749632a0a0a7"},
        {45000, "200c0a0090f5f31893ab7cb9853d596cb4bdef33f680de7f772dc83d615f554c"},
        {50000, "ceb6c3e01ccd832779060a65a348ef38cab067bc951df11aa38332b8c2b7d299"},
        {55000, "306d852d99c6bb7ac3070d93c4495ae6964f5d5ec3f01d2657afe55ebb34e5e9"},
        {60000, "cfb9869db7c336b56f2eeffab109dfe8eb9adb79defc70c9d0024526a7eeb787"},
        {65000, "191d92f5c7cbe6f4482c6c7ea935dfebbaf929e8a918a668a99152ae0b3eb3c6"},
        {70000, "075590cc76603d59bc72a91564b5cfb5431034adac9cc0ceac9b88b70508238d"},
        {75000, "cf2e2c171107e05aa9932e7f66a3df5117e5c1a4b084bb9b0c9c79300ebb82ce"},
        {80000, "59b24deb207138ab7b15d38d9daf387d507e0fed33aae16175a30a1e7fa6ab33"},
        {85000, "1b03c9547f1650763e675439d1882df0bde91c7df8bd490036d61c7591a73677"},
        {90000, "991e40e508f420994afdc94398353508164a24af697402fb9a5f65164166a856"},
        {95000, "c861cf7d0ea8b1f59c1220c4141de5725ed3cc03e41a7fd6a9ca457b5606c666"},
        {100000, "a2c7179f5e7ea541ba8ed929044600b9cbf2aa20e5cf1d29cb93439fda4431ba"},
        {105000, "5afc590c15c8f115350c8095f45f4a9efc3debca2de6bac09a8d694ff6f22a00"},
        {110000, "15d37aadf963f32167a916a73f1e8ede7ccfee181f6cbe149c95d7b536f4be91"},
        {115000, "dcdddd9412dd93d1b7231bd070c3ba5a9b3931f0b850fa58f02a2c5e4ea8b8fe"},
        {120000, "56d0352b187a17d7523c10c85df98e8b8860af2c34f05c109b7d4966337191c5"},
        {125000, "8a1737f2eee125776dff01288ada59ced95f9ab3a3fcbbed9c32a3dc20cad033"},
        {130000, "30be866410a6a2bb4d7fdf69dc8e8256af116c5a8fdcf5169096a211c01bf160"},
        {135000, "eaf75463b54076c4db11afb5d4917cded75b329b84b33fc8f455b7275867449c"},
        {140000, "645512f82ab6e20fd8531716ea1f735fcaf5dcadeaec06bb4c238b36057b4934"}
    };

} // namespace cn


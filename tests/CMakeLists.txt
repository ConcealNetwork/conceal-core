add_definitions(-DMINIUPNP_STATICLIB)

include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR} ../version)

file(GLOB_RECURSE CoreTests CoreTests/*)
file(GLOB_RECURSE CryptoTests crypto/*)
file(GLOB_RECURSE IntegrationTestLibrary IntegrationTestLib/*)
file(GLOB_RECURSE IntegrationTests IntegrationTests/*)
file(GLOB_RECURSE FunctionalTests FunctionalTests/*)
file(GLOB_RECURSE NodeRpcProxyTests NodeRpcProxyTests/*)
file(GLOB_RECURSE PerformanceTests PerformanceTests/*)
file(GLOB_RECURSE SystemTests System/*)
file(GLOB_RECURSE TestGenerator TestGenerator/*)
file(GLOB_RECURSE TransfersTests TransfersTests/*)
file(GLOB_RECURSE UnitTests UnitTests/*)

file(GLOB_RECURSE CryptoNoteProtocol ../src/CryptoNoteProtocol/*)
file(GLOB_RECURSE P2p ../src/P2p/*)

source_group("" FILES ${CoreTests} ${CryptoTests} ${CryptoTests} ${FunctionalTests} ${NodeRpcProxyTests} ${PerformanceTests} ${SystemTests} ${TestGenerator}  ${UnitTests})
source_group("" FILES ${CryptoNoteProtocol} ${P2p})

add_library(IntegrationTestLibrary ${IntegrationTestLibrary})
add_library(TestGenerator ${TestGenerator})

add_executable(CoreTests ${CoreTests})
add_executable(CryptoTests ${CryptoTests})
add_executable(IntegrationTests ${IntegrationTests})
add_executable(NodeRpcProxyTests ${NodeRpcProxyTests})
add_executable(PerformanceTests ${PerformanceTests})
add_executable(SystemTests ${SystemTests})
add_executable(TransfersTests ${TransfersTests})
add_executable(UnitTests ${UnitTests})

add_executable(DifficultyTests Difficulty/Difficulty.cpp)
add_executable(HashTargetTests HashTarget.cpp)
add_executable(HashTests Hash/main.cpp)

target_link_libraries(CoreTests TestGenerator CryptoNoteCore Serialization System Logging Common crypto BlockchainExplorer ${Boost_LIBRARIES})
target_link_libraries(IntegrationTests IntegrationTestLibrary Wallet NodeRpcProxy InProcessNode P2P Rpc Http Transfers Serialization System CryptoNoteCore Logging Common crypto BlockchainExplorer gtest libminiupnpc-static ${Boost_LIBRARIES})
target_link_libraries(NodeRpcProxyTests NodeRpcProxy CryptoNoteCore Rpc Http Serialization System Logging Common crypto ${Boost_LIBRARIES})
target_link_libraries(PerformanceTests CryptoNoteCore Serialization Logging Common crypto ${Boost_LIBRARIES})
target_link_libraries(SystemTests System gtest_main)
if (MSVC)
  target_link_libraries(SystemTests ws2_32)
  target_link_libraries(NodeRpcProxyTests ws2_32)
  target_link_libraries(CoreTests ws2_32)
endif ()
target_link_libraries(TransfersTests IntegrationTestLibrary Wallet gtest_main InProcessNode NodeRpcProxy P2P Rpc Http BlockchainExplorer CryptoNoteCore Serialization System Logging Transfers Common crypto libminiupnpc-static ${Boost_LIBRARIES})
target_link_libraries(UnitTests TestGenerator PaymentGate Wallet NodeRpcProxy InProcessNode P2P Rpc Http Transfers Serialization System CryptoNoteCore Logging Common crypto BlockchainExplorer gtest ${Boost_LIBRARIES})

target_link_libraries(DifficultyTests CryptoNoteCore Serialization crypto Logging Common ${Boost_LIBRARIES})
target_link_libraries(HashTargetTests CryptoNoteCore crypto)
target_link_libraries(HashTests crypto)


add_custom_target(tests DEPENDS NodeRpcProxyTests PerformanceTests SystemTests UnitTests DifficultyTests HashTargetTests)

set_property(TARGET
  tests
  CoreTests
  CryptoTests
  IntegrationTestLibrary
  TestGenerator
  IntegrationTests
  NodeRpcProxyTests
  PerformanceTests
  SystemTests
  TransfersTests
  UnitTests

  DifficultyTests
  HashTargetTests
  HashTests

PROPERTY FOLDER "tests")

set_property(TARGET CoreTests PROPERTY OUTPUT_NAME "core_tests")
set_property(TARGET CryptoTests PROPERTY OUTPUT_NAME "crypto_tests")
set_property(TARGET IntegrationTests PROPERTY OUTPUT_NAME "integration_tests")
set_property(TARGET NodeRpcProxyTests PROPERTY OUTPUT_NAME "node_rpc_proxy_tests")
set_property(TARGET PerformanceTests PROPERTY OUTPUT_NAME "performance_tests")
set_property(TARGET SystemTests PROPERTY OUTPUT_NAME "system_tests")
set_property(TARGET TransfersTests PROPERTY OUTPUT_NAME "transfers_tests")
set_property(TARGET UnitTests PROPERTY OUTPUT_NAME "unit_tests")

set_property(TARGET DifficultyTests PROPERTY OUTPUT_NAME "difficulty_tests")
set_property(TARGET HashTargetTests PROPERTY OUTPUT_NAME "hash_target_tests")
set_property(TARGET HashTests PROPERTY OUTPUT_NAME "hash_tests")


if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR APPLE AND NOT ANDROID)
  target_link_libraries(CoreTests -lresolv)
  target_link_libraries(IntegrationTests -lresolv)
  target_link_libraries(TransfersTests -lresolv)
  target_link_libraries(UnitTests -lresolv)
endif()

include(CTest)

add_test(CoreTests core_tests --generate_and_play_test_data)
add_test(CryptoTests crypto_tests ${CMAKE_CURRENT_SOURCE_DIR}/crypto/tests.txt)
add_test(IntegrationTests integration_tests)
add_test(NodeRpcProxyTests node_rpc_proxy_tests)
add_test(PerformanceTests performance_tests)
add_test(SystemTests system_tests)
add_test(TransfersTests transfers_tests)
add_test(UnitTests unit_tests --gtest_filter=-WalletApi.*:WalletApi_makeTransaction.*:WalletApi_commitTransaction.*:WalletApi_rollbackUncommitedTransaction.*:tx_pool.TxPoolDoesNotAcceptInvalidFusionTransaction:TxPool_FillBlockTemplate.TxPoolAddsFusionTransactionsToBlockTemplateNoMoreThanLimit:TxPool_FillBlockTemplate.TxPoolContinuesToAddOrdinaryTransactionsUpTo125PerCentOfMedianAfterAddingFusionTransactions:WalletLegacyApi.sendSeveralTransactions)

add_test(DifficultyTests difficulty_tests ${CMAKE_CURRENT_SOURCE_DIR}/Difficulty/data.txt)
add_test(HashTargetTests hash_target_tests)
foreach(hash IN ITEMS fast slow tree)
  add_test(hash-${hash} hash_tests ${hash} ${CMAKE_CURRENT_SOURCE_DIR}/Hash/tests-${hash}.txt)
endforeach(hash)

if(MINGW AND STATIC)
  set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive")
endif()
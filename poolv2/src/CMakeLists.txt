project(crlpool)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( crlpool crlpool crlpool.cpp )
target_include_directories( crlpool PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( crlpool ${CMAKE_SOURCE_DIR}/../ricardian )
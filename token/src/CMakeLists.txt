project(token)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( token token token.cpp )
target_include_directories( token PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( token ${CMAKE_SOURCE_DIR}/../ricardian )
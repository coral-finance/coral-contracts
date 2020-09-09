project(wram)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( wram wram wram.cpp )
target_include_directories( wram PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( wram ${CMAKE_SOURCE_DIR}/../ricardian )
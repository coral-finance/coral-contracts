#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <eosio/asset.hpp>

using namespace eosio;
using namespace std;

struct transfer_args {
    name from;
    name to;
    asset quantity;
    string memo;
};
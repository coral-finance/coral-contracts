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

 struct [[eosio::table]] box_reward {
    name owner;
    uint64_t cumulative;
    uint64_t unclaimed;
    uint64_t primary_key() const { return owner.value; }
};

typedef eosio::multi_index<"rewards"_n, box_reward> boxrewards;
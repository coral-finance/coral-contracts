#include <structs.hpp>

namespace utils {

    void inline_transfer(name contract, name from, name to, asset quantity, string memo) {
        auto data = make_tuple(from, to, quantity, memo);
        action(permission_level{from, "active"_n}, contract, "transfer"_n, data).send();
    }

    // uint128_t get_token_key(name contract, symbol sym) {
    //     return ((uint128_t)(contract.value) << 64) + sym.raw();
    // }

}
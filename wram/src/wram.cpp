#include <wram.hpp>

void wram::create(const name &issuer, const asset &maximum_supply) {
    require_auth(get_self());

    auto sym = maximum_supply.symbol;
    check(sym.is_valid(), "invalid symbol name");
    check(maximum_supply.is_valid(), "invalid supply");
    check(maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable(get_self(), sym.code().raw());
    auto existing = statstable.find(sym.code().raw());
    check(existing == statstable.end(), "token with symbol already exists");

    statstable.emplace(get_self(), [&](auto &s) {
        s.supply.symbol = maximum_supply.symbol;
        s.max_supply = maximum_supply;
        s.issuer = issuer;
    });
}

void wram::issue(const name &to, const asset &quantity, const string &memo) {
    auto sym = quantity.symbol;
    check(sym.is_valid(), "invalid symbol name");
    check(memo.size() <= 256, "memo has more than 256 bytes");

    stats statstable(_self, sym.code().raw());
    auto existing = statstable.find(sym.code().raw());
    check(existing != statstable.end(), "token with symbol does not exist, create token before issue");
    const auto &st = *existing;

    require_auth(st.issuer);
    check(quantity.is_valid(), "invalid quantity");
    check(quantity.amount > 0, "must issue positive quantity");

    check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
    check(quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify(st, same_payer, [&](auto &s) {
        s.supply += quantity;
    });

    add_balance(st.issuer, quantity, st.issuer);

    if (to != st.issuer) {
        SEND_INLINE_ACTION(*this, transfer, {{st.issuer, "active"_n}}, {st.issuer, to, quantity, memo});
    }
}

void wram::retire(const asset &quantity, const string &memo) {
    auto sym = quantity.symbol;
    check(sym.is_valid(), "invalid symbol name");
    check(memo.size() <= 256, "memo has more than 256 bytes");

    stats statstable(get_self(), sym.code().raw());
    auto existing = statstable.find(sym.code().raw());
    check(existing != statstable.end(), "token with symbol does not exist");
    const auto &st = *existing;

    require_auth(st.issuer);
    check(quantity.is_valid(), "invalid quantity");
    check(quantity.amount > 0, "must retire positive quantity");

    check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");

    statstable.modify(st, same_payer, [&](auto &s) {
        s.supply -= quantity;
    });

    sub_balance(st.issuer, quantity);
}

void wram::transfer(const name &from, const name &to, const asset &quantity, const string &memo) {
    check(from != to, "cannot transfer to self");
    require_auth(from);
    check(is_account(to), "to account does not exist");
    auto sym = quantity.symbol.code();
    stats statstable(get_self(), sym.raw());
    const auto &st = statstable.get(sym.raw());

    require_recipient(from);
    require_recipient(to);

    check(quantity.is_valid(), "invalid quantity");
    check(quantity.amount > 0, "must transfer positive quantity");
    check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
    check(memo.size() <= 256, "memo has more than 256 bytes");

    auto payer = has_auth(to) ? to : from;

    sub_balance(from, quantity);
    add_balance(to, quantity, payer);

    if (to == _self) {
        sellram(from, to, quantity);
    }
}

void wram::sub_balance(const name &owner, const asset &value) {
    accounts from_acnts(get_self(), owner.value);

    const auto &from = from_acnts.get(value.symbol.code().raw(), "no balance object found");

    auto balance = from.balance.amount;
    check(balance >= value.amount, "overdrawn balance");

    from_acnts.modify(from, owner, [&](auto &a) {
        a.balance -= value;
    });
}

void wram::add_balance(const name &owner, const asset &value, const name &ram_payer) {
    accounts to_acnts(get_self(), owner.value);
    auto to = to_acnts.find(value.symbol.code().raw());
    if (to == to_acnts.end()) {
        to_acnts.emplace(ram_payer, [&](auto &a) {
            a.balance = value;
        });
    } else {
        to_acnts.modify(to, same_payer, [&](auto &a) {
            a.balance += value;
        });
    }
}

void wram::open(const name &owner, const symbol &symbol, const name &ram_payer) {
    require_auth(ram_payer);

    check(is_account(owner), "owner account does not exist");

    auto sym_code_raw = symbol.code().raw();
    stats statstable(get_self(), sym_code_raw);
    const auto &st = statstable.get(sym_code_raw, "symbol does not exist");
    check(st.supply.symbol == symbol, "symbol precision mismatch");

    accounts acnts(get_self(), owner.value);
    auto it = acnts.find(sym_code_raw);
    if (it == acnts.end()) {
        acnts.emplace(ram_payer, [&](auto &a) {
            a.balance = asset{0, symbol};
        });
    }
}

void wram::close(const name &owner, const symbol &symbol) {
    require_auth(owner);
    accounts acnts(get_self(), owner.value);
    auto it = acnts.find(symbol.code().raw());
    check(it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect.");
    check(it->balance.amount == 0, "Cannot close because the balance is not zero.");
    acnts.erase(it);
}

// new methods
void wram::receive_eos(name from, name to, asset quantity, std::string memo) {
    if (from == _self || to != _self) {
        return;
    }
    if (from == "eosio.ram"_n) {
        return;
    }
    check(quantity.symbol == EOS_SYMBOL, "wrong token");
    buyram(from, to, quantity);
}

void wram::buyram(name from, name to, asset quantity) {
    auto fee_amount = quantity.amount * 5 / 1000;
    if (fee_amount == 0) {
        fee_amount = 1;
    }
    check(quantity.amount > fee_amount, "too small amount");
    quantity.amount -= fee_amount;

    auto fees = asset(fee_amount, quantity.symbol);
    action{
        permission_level{_self, "active"_n},
        "eosio.token"_n,
        "transfer"_n,
        std::make_tuple(_self, WRAM_FEE_ACCOUNT, fees, std::string("mint fee"))
    }.send();

    rams ramstable("eosio"_n, _self.value);
    auto itr = ramstable.find(_self.value);
    auto bytes = itr->ram_bytes;
    action{
        permission_level{_self, "active"_n},
        "eosio"_n,
        "buyram"_n,
        std::make_tuple(_self, _self, quantity)}
        .send();

    action{
        permission_level{_self, "active"_n},
        _self,
        "mint"_n,
        std::make_tuple(from, bytes)
    }.send();
}

void wram::mint(name from, uint64_t prev_bytes) {
    require_auth(_self);

    rams ramstable("eosio"_n, _self.value);
    auto itr = ramstable.find(_self.value);
    auto now_bytes = itr->ram_bytes;
    auto amount = now_bytes - prev_bytes;
    auto quantity = asset(amount, symbol("WRAM", 4));

    action{
        permission_level{_self, "active"_n},
        _self,
        "issue"_n,
        std::make_tuple(from, quantity, std::string("mint wRAM"))
    }.send();
}

void wram::sellram(name from, name to, asset quantity) {
    if (from == _self || to != _self) {
        return;
    }

    action{
        permission_level{_self, "active"_n},
        _self,
        "retire"_n,
        std::make_tuple(quantity, std::string("retire"))
    }.send();

    action{
        permission_level{_self, "active"_n},
        "eosio"_n,
        "sellram"_n,
        std::make_tuple(_self, quantity.amount)
    }.send();

    action{
        permission_level{_self, "active"_n},
        _self,
        "repay"_n,
        std::make_tuple(from)
    }.send();
}

void wram::repay(name from) {
    require_auth(_self);

    auto quantity = get_balance("eosio.token"_n, _self, EOS_SYMBOL.code());
    action{
        permission_level{_self, "active"_n},
        "eosio.token"_n,
        "transfer"_n,
        std::make_tuple(_self, from, quantity, std::string("withdraw EOS"))
    }.send();
}

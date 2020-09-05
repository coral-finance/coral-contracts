#include <crlpool.hpp>
#include <math.h>

extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        if (code == receiver) {
            switch (action) {
                EOSIO_DISPATCH_HELPER(crlpool, (create)(claim)(withdraw)(harvest))
            }
        } else {
            if (action == name("transfer").value) {
                crlpool inst(name(receiver), name(code), datastream<const char *>(nullptr, 0));
                const auto t = unpack_action_data<transfer_args>();
                inst.handle_transfer(t.from, t.to, t.quantity, t.memo, name(code));
            }
        }
    }
}

void crlpool::create(name contract, symbol sym, asset reward, uint32_t epoch_time, uint32_t duration, asset min_staked) {
    require_auth("coralmanager"_n);

    pools_mi pools_tbl(_self, _self.value);
    auto itr = pools_tbl.begin();
    
    while (itr != pools_tbl.end()) {
        auto exists = itr->contract == contract && itr->sym == sym;
        check(!exists, "Token exists");
        itr++;
    }

    check(reward.symbol == symbol("CRL", 10), "Reward symbol error");
    check(min_staked.symbol == sym, "Min-staked symbol error");

    auto total = reward;
    itr = pools_tbl.begin();
    while (itr != pools_tbl.end()) {
        total += itr->total_reward;
        itr++;
    }
    check(total.amount <= 300000000000000, "Reach the max circulation");

    auto pool_id = pools_tbl.available_primary_key();
    if (pool_id == 0) {
        pool_id = 1;
    }
    pools_tbl.emplace(_self, [&]( auto& a ) {
        a.id = pool_id;
        a.contract = contract;
        a.sym = sym;
        a.total_staked = asset(0, sym);
        a.total_reward = reward;
        a.released_reward = asset(0, reward.symbol);
        a.epoch_time = epoch_time;
        a.duration = duration;
        a.min_staked = min_staked;
        a.last_harvest_time = epoch_time;
    });
}

void crlpool::claim(name owner, uint64_t pool_id) {
    require_auth(owner);

    pools_mi pools_tbl(_self, _self.value);
    auto p_itr = pools_tbl.find(pool_id);  
    check(p_itr != pools_tbl.end(), "Pool not exists");

    miners_mi miners_tbl(_self, pool_id);
    auto m_itr = miners_tbl.find(owner.value);
    check(m_itr != miners_tbl.end(), "No this miner");
    check(m_itr->unclaimed.amount > 0, "No unclaimed");

    auto quantity = m_itr->unclaimed;
    miners_tbl.modify(m_itr, same_payer, [&]( auto& s) {
        s.claimed += quantity;
        s.unclaimed = asset(0, quantity.symbol);
    });
    
    utils::inline_transfer(CRL_CONTRACT, _self, owner, quantity, string("Minner claimed"));
}

void crlpool::withdraw(name owner, uint64_t pool_id) {
    require_auth(owner);

    pools_mi pools_tbl(_self, _self.value);
    auto p_itr = pools_tbl.find(pool_id);  
    check(p_itr != pools_tbl.end(), "Pool not exists");

    miners_mi miners_tbl(_self, pool_id);
    auto m_itr = miners_tbl.find(owner.value);
    check(m_itr != miners_tbl.end(), "No this miner");
    auto unclaimed = m_itr->unclaimed;

    auto quantity = m_itr->staked;
    pools_tbl.modify(p_itr, same_payer, [&]( auto& s) {
        s.total_staked -= quantity;
    });
    miners_tbl.erase(m_itr);

    utils::inline_transfer(p_itr->contract, _self, owner, quantity, string("Minner withdraw"));
    if (unclaimed.amount > 0) {
        utils::inline_transfer(CRL_CONTRACT, _self, owner, unclaimed, string("Minner claimed"));
    }
}

void crlpool::harvest(uint64_t pool_id, uint32_t nonce) {
    require_auth("coralmanager"_n);

    pools_mi pools_tbl(_self, _self.value);
    auto itr = pools_tbl.find(pool_id);  
    check(itr != pools_tbl.end(), "Pool not exists");

    auto now_time = current_time_point().sec_since_epoch();
    check(now_time >= itr->epoch_time, "Mining hasn't started yet");
    check(now_time <= itr->epoch_time + itr->duration, "Mining is over");
    
    auto period = itr->duration / 4;
    auto supply_per_second_init = itr->total_reward.amount / 2 / period;
    auto exp = (now_time - itr->epoch_time) / period;
    if (exp > 3) {
        exp = 3;
    }
    auto supply_per_second_now = supply_per_second_init * (uint32_t)(pow(0.5, exp) * 1000) / 1000;
    auto time_elapsed = now_time - itr->last_harvest_time;
    if (time_elapsed == 0) {
        return;
    }
    
    auto token_issued = asset(time_elapsed * supply_per_second_now, itr->released_reward.symbol);
    pools_tbl.modify(itr, same_payer, [&]( auto& s) {
        s.released_reward += token_issued;
        s.last_harvest_time = now_time;
    });

    // issue
    auto data = make_tuple(_self, token_issued, string("Issue CRL"));
    action(permission_level{_self, "active"_n}, CRL_CONTRACT, "issue"_n, data).send();

    // update every miner
    miners_mi miners_tbl(_self, itr->id);
    auto m_itr = miners_tbl.begin();
    check(m_itr != miners_tbl.end(), "No miners");
    while (m_itr != miners_tbl.end()) {
        double radio = (double)(m_itr->staked.amount) / itr->total_staked.amount;
        uint64_t amount = (uint64_t)(token_issued.amount * radio);
        miners_tbl.modify(m_itr, same_payer, [&]( auto& a) {
            a.unclaimed.amount += amount;
        });
        m_itr++;
    }

}

void crlpool::handle_transfer(name from, name to, asset quantity, string memo, name code) {
    if (from == _self || to != _self) {
        return;
    }
    require_auth(from);
    auto sym = quantity.symbol;
    pools_mi pools_tbl(_self, _self.value);
    auto itr = pools_tbl.begin();
    while (itr != pools_tbl.end()) {
        if (itr->contract == code && itr->sym == sym) {
            break;
        }
        itr++;
    }
    check(itr != pools_tbl.end(), "Pool not found");
    check(itr->contract == code && itr->sym == sym, "Error token"); // recheck, actually don’t need to do this.
    check(quantity >= itr->min_staked, "The amount of staked is too small");
    auto now_time = current_time_point().sec_since_epoch();
    check(now_time <= itr->epoch_time + itr->duration, "Mining is over");

    pools_tbl.modify(itr, same_payer, [&]( auto& s) {
        s.total_staked += quantity;
    });

    miners_mi miners_tbl(_self, itr->id);
    auto m_itr = miners_tbl.find(from.value);
    if (m_itr == miners_tbl.end()) {
        auto zero_crl = asset(0, symbol("CRL", 10));
        miners_tbl.emplace(_self, [&]( auto& a) {
            a.owner = from;
            a.staked = quantity;
            a.claimed = zero_crl;
            a.unclaimed = zero_crl;
        });
    } else {
        miners_tbl.modify(m_itr, same_payer, [&]( auto& a) {
            a.staked += quantity;
        });
    }
}
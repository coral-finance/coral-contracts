#include <utils.hpp>

#define CRL_CONTRACT  name("coralfitoken")

CONTRACT crlpool : public contract {
   public:
      using contract::contract;

      ACTION create(name contract, symbol sym, asset reward, uint32_t epoch_time, uint32_t duration, asset min_staked);
      ACTION claim(name owner, uint64_t pool_id);
      ACTION withdraw(name owner, uint64_t pool_id);
      ACTION harvest(uint64_t pool_id, uint32_t nonce);

      void handle_transfer(name from, name to, asset quantity, string memo, name code);

   private:
      TABLE pool {
         uint64_t id;
         name contract;
         symbol sym;
         asset total_staked;
         asset total_reward;
         asset released_reward;
         uint32_t epoch_time;
         uint32_t duration;
         asset min_staked;
         uint32_t last_harvest_time;
         uint64_t primary_key() const { return id; }
         // uint128_t get_key() const { return utils::get_token_key(contract, sym); }
      };

      TABLE miner {
         name owner;
         asset staked;
         asset claimed;
         asset unclaimed;
         uint64_t primary_key() const { return owner.value; }
      };

      typedef eosio::multi_index<"pools"_n, pool> pools_mi;
      typedef eosio::multi_index<"miners"_n, miner> miners_mi;

      
};
#include <utils.hpp>

#define CRL_CONTRACT  name("coralfitoken")
#define BOX_LP_CONTRACT  name("lptoken.defi")
#define BOX_TOKEN_CONTRACT  name("token.defi")
#define FEES_ACCOUNT  name("coralpoolfee")

CONTRACT crlpool : public contract {
   public:
      using contract::contract;

      ACTION create(name contract, symbol sym, asset reward, uint32_t epoch_time, uint32_t duration, asset min_staked, uint8_t box_enable, symbol_code box_code);
      ACTION claim(name owner, uint64_t pool_id);
      ACTION withdraw(name owner, uint64_t pool_id);
      ACTION harvest(uint64_t pool_id, uint64_t round_no, uint32_t limit);

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
         uint8_t box_enable;
         symbol_code box_code;
         asset box_reward;
         uint64_t primary_key() const { return id; }
      };

      TABLE miner {
         name owner;
         asset staked;
         asset claimed_crl;
         asset unclaimed_crl;
         asset claimed_box;
         asset unclaimed_box;
         uint64_t primary_key() const { return owner.value; }
      };

      TABLE round {
         uint64_t pool_id;
         uint64_t no;
         name offset;
         uint64_t crl_amount;
         uint64_t box_amount;
         bool completed;
         uint64_t primary_key() const { return pool_id; }
      };
      
      typedef eosio::multi_index<"pools"_n, pool> pools_mi;
      typedef eosio::multi_index<"miners"_n, miner> miners_mi;
      typedef eosio::multi_index<"rounds"_n, round> rounds_mi;
      
};
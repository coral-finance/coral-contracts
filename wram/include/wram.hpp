#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>

using std::string;
using namespace eosio;

#define WRAM_FEE_ACCOUNT  name("coralwramfee")
#define EOS_SYMBOL  symbol("EOS", 4)

class [[eosio::contract("wram")]] wram : public contract {
public:
   using contract::contract;

   [[eosio::action]] 
   void create(const name &issuer, const asset &maximum_supply);

   [[eosio::action]] 
   void issue(const name &to, const asset &quantity, const string &memo);

   [[eosio::action]] 
   void retire(const asset &quantity, const string &memo);

   [[eosio::action]] 
   void transfer(const name &from, const name &to, const asset &quantity, const string &memo);

   [[eosio::action]] 
   void open(const name &owner, const symbol &symbol, const name &ram_payer);

   [[eosio::action]] 
   void close(const name &owner, const symbol &symbol);

   // some new methods
   [[eosio::on_notify("eosio.token::transfer")]]
   void receive_eos(name from, name to, asset quantity, std::string memo);

   [[eosio::action]]
	void mint(name from, uint64_t prev_bytes);

   [[eosio::action]]
	void repay(name from);

private:

   struct [[eosio::table]] account {
      asset balance;
      uint64_t primary_key() const { return balance.symbol.code().raw(); }
   };

   struct [[eosio::table]] currency_stats {
      asset supply;
      asset max_supply;
      name issuer;
      uint64_t primary_key() const { return supply.symbol.code().raw(); }
   };

   struct userres {
      name 	owner;
      asset 	net_weight;
      asset 	cpu_weight;
      uint64_t ram_bytes;
      uint64_t primary_key() const { return owner.value; }
   };

   typedef eosio::multi_index<"accounts"_n, account> accounts;
   typedef eosio::multi_index<"stat"_n, currency_stats> stats;
   typedef eosio::multi_index<"userres"_n, userres> rams;

   void sub_balance(const name &owner, const asset &value);
   void add_balance(const name &owner, const asset &value, const name &ram_payer);

   static asset get_balance(const name& token_contract_account, const name& owner, const symbol_code& sym_code) {
		accounts accountstable(token_contract_account, owner.value);
		const auto& ac = accountstable.get(sym_code.raw());
		return ac.balance;
	}

   void buyram(name from, name to, asset quantity);
   void sellram(name from, name to, asset quantity);

};
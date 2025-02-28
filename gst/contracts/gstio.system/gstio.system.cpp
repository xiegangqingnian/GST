#include "gstio.system.hpp"
#include <gstiolib/dispatcher.hpp>

#include "producer_pay.cpp"
#include "delegate_bandwidth.cpp"
#include "voting.cpp"
#include "exchange_state.cpp"


namespace gstiosystem {

	system_contract::system_contract(account_name s)
		:native(s),
		_voters(_self, _self),
		_producers(_self, _self),
		_global(_self, _self),
		_rammarket(_self, _self)
	{
		//print( "construct system\n" );
		_gstate = _global.exists() ? _global.get() : get_default_parameters();

		auto itr = _rammarket.find(S(4, RAMCORE));

		if (itr == _rammarket.end()) {
			auto system_token_supply = gstio::token(N(gstio.token)).get_supply(gstio::symbol_type(system_token_symbol).name()).amount;
			if (system_token_supply > 0) {
				itr = _rammarket.emplace(_self, [&](auto& m) {
					m.supply.amount = 100000000000000ll;
					m.supply.symbol = S(4, RAMCORE);
					m.base.balance.amount = int64_t(_gstate.free_ram());
					m.base.balance.symbol = S(0, RAM);
					m.quote.balance.amount = system_token_supply / 1000;
					m.quote.balance.symbol = CORE_SYMBOL;
				});
			}
		}
		else {
			//print( "ram market already created" );
		}
	}

	gstio_global_state system_contract::get_default_parameters() {
		gstio_global_state dp;
		get_blockchain_parameters(dp);
		return dp;
	}


	system_contract::~system_contract() {
		//print( "destruct system\n" );
		_global.set(_gstate, _self);
		//gstio_exit(0);
	}

	void system_contract::setram(uint64_t max_ram_size) {
		require_auth(_self);

		gstio_assert(_gstate.max_ram_size < max_ram_size, "ram may only be increased"); /// decreasing ram might result market maker issues
		gstio_assert(max_ram_size < 1024ll * 1024 * 1024 * 1024 * 1024, "ram size is unrealistic");
		gstio_assert(max_ram_size > _gstate.total_ram_bytes_reserved, "attempt to set max below reserved");

		auto delta = int64_t(max_ram_size) - int64_t(_gstate.max_ram_size);
		auto itr = _rammarket.find(S(4, RAMCORE));

		/**
		 *  Increase or decrease the amount of ram for sale based upon the change in max
		 *  ram size.
		 */
		_rammarket.modify(itr, 0, [&](auto& m) {
			m.base.balance.amount += delta;
		});

		_gstate.max_ram_size = max_ram_size;
		_global.set(_gstate, _self);
	}

	void system_contract::setparams(const gstio::blockchain_parameters& params) {
		require_auth(N(gstio));
		(gstio::blockchain_parameters&)(_gstate) = params;
		gstio_assert(3 <= _gstate.max_authority_depth, "max_authority_depth should be at least 3");
		set_blockchain_parameters(params);
	}

	void system_contract::setpriv(account_name account, uint8_t ispriv) {
		require_auth(_self);
		set_privileged(account, ispriv);
	}

	void system_contract::rmvproducer(account_name producer) {
		require_auth(_self);
		auto prod = _producers.find(producer);
		gstio_assert(prod != _producers.end(), "producer not found");
		_producers.modify(prod, 0, [&](auto& p) {
			p.deactivate();
		});
	}

	void system_contract::bidname(account_name bidder, account_name newname, asset bid) {
		require_auth(bidder);
		gstio_assert(gstio::name_suffix(newname) == newname, "you can only bid on top-level suffix");
		gstio_assert(newname != 0, "the empty name is not a valid account name to bid on");
		gstio_assert((newname & 0xFull) == 0, "13 character names are not valid account names to bid on");
		gstio_assert((newname & 0x1F0ull) == 0, "accounts with 12 character names and no dots can be created without bidding required");
		gstio_assert(!is_account(newname), "account already exists");
		gstio_assert(bid.symbol == asset().symbol, "asset must be system token");
		gstio_assert(bid.amount > 0, "insufficient bid");

		INLINE_ACTION_SENDER(gstio::token, transfer)(N(gstio.token), { bidder,N(active) },
			{ bidder, N(gstio.names), bid, std::string("bid name ") + (name{newname}).to_string() });

		name_bid_table bids(_self, _self);
		print(name{ bidder }, " bid ", bid, " on ", name{ newname }, "\n");
		auto current = bids.find(newname);
		if (current == bids.end()) {
			bids.emplace(bidder, [&](auto& b) {
				b.newname = newname;
				b.high_bidder = bidder;
				b.high_bid = bid.amount;
				b.last_bid_time = current_time();
			});
		}
		else {
			gstio_assert(current->high_bid > 0, "this auction has already closed");
			gstio_assert(bid.amount - current->high_bid > (current->high_bid / 10), "must increase bid by 10%");
			gstio_assert(current->high_bidder != bidder, "account is already highest bidder");

			INLINE_ACTION_SENDER(gstio::token, transfer)(N(gstio.token), { N(gstio.names),N(active) },
				{ N(gstio.names), current->high_bidder, asset(current->high_bid),
				std::string("refund bid on name ") + (name{newname}).to_string() });

			bids.modify(current, bidder, [&](auto& b) {
				b.high_bidder = bidder;
				b.high_bid = bid.amount;
				b.last_bid_time = current_time();
			});
		}
	}

	/**
	 *  Called after a new account is created. This code enforces resource-limits rules
	 *  for new accounts as well as new account naming conventions.
	 *
	 *  Account names containing '.' symbols must have a suffix equal to the name of the creator.
	 *  This allows users who buy a premium name (shorter than 12 characters with no dots) to be the only ones
	 *  who can create accounts with the creator's name as a suffix.
	 *
	 */
	void native::newaccount(account_name     creator,
		account_name     newact
		/*  no need to parse authorities
		const authority& owner,
		const authority& active*/) {

		if (creator != _self) {
			auto tmp = newact >> 4;
			bool has_dot = false;

			for (uint32_t i = 0; i < 12; ++i) {
				has_dot |= !(tmp & 0x1f);
				tmp >>= 5;
			}
			if (has_dot) { // or is less than 12 characters
				auto suffix = gstio::name_suffix(newact);
				if (suffix == newact) {
					name_bid_table bids(_self, _self);
					auto current = bids.find(newact);
					gstio_assert(current != bids.end(), "no active bid for name");
					gstio_assert(current->high_bidder == creator, "only highest bidder can claim");
					gstio_assert(current->high_bid < 0, "auction for name is not closed yet");
					bids.erase(current);
				}
				else {
					gstio_assert(creator == suffix, "only suffix may create this account");
				}
			}
		}

		user_resources_table  userres(_self, newact);

		userres.emplace(newact, [&](auto& res) {
			res.owner = newact;
		});

		set_resource_limits(newact, -1, 0, 0);
	}

} /// gstio.system


GSTIO_ABI(gstiosystem::system_contract,
	// native.hpp (newaccount definition is actually in gstio.system.cpp)
	(newaccount)(updateauth)(deleteauth)(linkauth)(unlinkauth)(canceldelay)(onerror)
	// gstio.system.cpp
	(setram)(setparams)(setpriv)(rmvproducer)(bidname)
	// delegate_bandwidth.cpp
	(buyrambytes)(buyram)(buymem)(buymembytes)(sellram)(delegatebw)(undelegatebw)(refund)(voterewards)
	// voting.cpp
	(regproducer)(unregprod)(voteproducer)(regproxy)(claimvrew)		//2019/03/11
	// producer_pay.cpp
	(onblock)(claimrewards)(prodrewards)	//2019/03/12
)

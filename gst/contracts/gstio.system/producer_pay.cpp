#include "gstio.system.hpp"

#include <gstio.token/gstio.token.hpp>

namespace gstiosystem
{

const int64_t min_pervote_daily_pay = 100'0000;
const int64_t min_activated_stake = 150'000'000'0000;
const double continuous_rate = 0.04879;					 // 5% annual rate
const double perblock_rate = 0.0025;					 // 0.25%
const double standby_rate = 0.0075;						 // 0.75%
const uint32_t blocks_per_year = 52 * 7 * 24 * 2 * 3600; // half seconds per year
const uint32_t seconds_per_year = 52 * 7 * 24 * 3600;
const uint32_t blocks_per_day = 2 * 24 * 3600;
const uint32_t blocks_per_hour = 2 * 3600;
const uint64_t useconds_per_day = 24 * 3600 * uint64_t(1000000);
const uint64_t useconds_per_year = seconds_per_year * 1000000ll;

void system_contract::onblock(block_timestamp timestamp, account_name producer)
{
	using namespace gstio;

	require_auth(N(gstio));

	/** until activated stake crosses this threshold no new rewards are paid */
	if (_gstate.total_activated_stake < min_activated_stake)
		return;

	if (_gstate.last_pervote_bucket_fill == 0) /// start the presses
		_gstate.last_pervote_bucket_fill = current_time();

	/**
		 * At startup the initial producer may not be one that is registered / elected
		 * and therefore there may be no producer object for them.
		 */
	auto prod = _producers.find(producer);
	if (prod != _producers.end())
	{
		_gstate.total_unpaid_blocks++;
		_producers.modify(prod, 0, [&](auto &p) {
			p.unpaid_blocks++;
		});
	}

	/// only update block producers once every minute, block_timestamp is in half seconds
	if (timestamp.slot - _gstate.last_producer_schedule_update.slot > 120)
	{
		update_elected_producers(timestamp);

		if ((timestamp.slot - _gstate.last_name_close.slot) > blocks_per_day)
		{
			name_bid_table bids(_self, _self);
			auto idx = bids.get_index<N(highbid)>();
			auto highest = idx.begin();
			if (highest != idx.end() &&
				highest->high_bid > 0 &&
				highest->last_bid_time < (current_time() - useconds_per_day) &&
				_gstate.thresh_activated_stake_time > 0 &&
				(current_time() - _gstate.thresh_activated_stake_time) > 14 * useconds_per_day)
			{
				_gstate.last_name_close = timestamp;
				idx.modify(highest, 0, [&](auto &b) {
					b.high_bid = -b.high_bid;
				});
			}
		}
	}
}

using namespace gstio;
void system_contract::claimrewards(const account_name &owner)
{
	require_auth(owner);

	const auto &prod = _producers.get(owner);
	gstio_assert(prod.active(), "producer does not have an active key");

	gstio_assert(_gstate.total_activated_stake >= min_activated_stake,
				 "cannot claim rewards until the chain is activated (at least 15% of all tokens participate in voting)");

	auto ct = current_time();

	gstio_assert(ct - prod.last_claim_time > useconds_per_day, "already claimed rewards within past day");

	const asset token_supply = token(N(gstio.token)).get_supply(symbol_type(system_token_symbol).name());
	const auto usecs_since_last_fill = ct - _gstate.last_pervote_bucket_fill;

	if (usecs_since_last_fill > 0 && _gstate.last_pervote_bucket_fill > 0)
	{
		auto new_tokens = static_cast<int64_t>((continuous_rate * double(token_supply.amount) * double(usecs_since_last_fill)) / double(useconds_per_year));

		auto to_producers = new_tokens / 5;							 //用于给超级节点的奖励，总增发量的1/5
		auto to_voters_pay = new_tokens / 5;						 //2019/03/11
		auto to_savings = new_tokens - to_producers - to_voters_pay; //2019/03/11	存起来给社区提案的奖励，总增发量的3/5
		auto to_per_block_pay = to_producers / 4;					 //用于出块的奖励，给超级节点奖励的1/4
		auto to_per_vote_pay = to_producers - to_per_block_pay;		 //用于得票奖励，给超级节点奖励的3/4

		INLINE_ACTION_SENDER(gstio::token, issue)
		(N(gstio.token), {{N(gstio), N(active)}},
		 {N(gstio), asset(new_tokens), std::string("issue tokens for producer pay and savings")});

		INLINE_ACTION_SENDER(gstio::token, transfer)
		(N(gstio.token), {N(gstio), N(active)},
		 {N(gstio), N(gstio.saving), asset(to_savings), "unallocated inflation"});

		INLINE_ACTION_SENDER(gstio::token, transfer)
		(N(gstio.token), {N(gstio), N(active)},
		 {N(gstio), N(gstio.bpay), asset(to_per_block_pay), "fund per-block bucket"});

		INLINE_ACTION_SENDER(gstio::token, transfer)
		(N(gstio.token), {N(gstio), N(active)},
		 {N(gstio), N(gstio.vpay), asset(to_per_vote_pay), "fund per-vote bucket"});

		INLINE_ACTION_SENDER(gstio::token, transfer)
		(N(gstio.token), {N(gstio), N(active)}, //2019/03/11
		 {N(gstio), N(gstio.vote), asset(to_voters_pay), "fund per-vote bucket"});

		_gstate.pervote_bucket += to_per_vote_pay;   //更新可用于得票奖励的总token量
		_gstate.perblock_bucket += to_per_block_pay; //更新可用于出块奖励的总token量
		_gstate.pertovote_bucket += to_voters_pay;   //2019/03/11	更新可用于投票奖励的总token量

		_gstate.last_pervote_bucket_fill = ct; //更新增发时间点
	}

	int64_t producer_per_block_pay = 0;
	if (_gstate.total_unpaid_blocks > 0)
	{
		producer_per_block_pay = (_gstate.perblock_bucket * prod.unpaid_blocks) / _gstate.total_unpaid_blocks;
	}
	int64_t producer_per_vote_pay = 0;
	if (_gstate.total_producer_vote_weight > 0)
	{
		producer_per_vote_pay = int64_t((_gstate.pervote_bucket * prod.total_votes) / _gstate.total_producer_vote_weight);
	}
	if (producer_per_vote_pay < min_pervote_daily_pay)
	{
		producer_per_vote_pay = 0;
	}
	_gstate.pervote_bucket -= producer_per_vote_pay;
	_gstate.perblock_bucket -= producer_per_block_pay;
	_gstate.total_unpaid_blocks -= prod.unpaid_blocks;

	_producers.modify(prod, 0, [&](auto &p) {
		p.last_claim_time = ct;
		p.unpaid_blocks = 0;
	});

	if (producer_per_block_pay > 0)
	{
		INLINE_ACTION_SENDER(gstio::token, transfer)
		(N(gstio.token), {N(gstio.bpay), N(active)},
		 {N(gstio.bpay), owner, asset(producer_per_block_pay), std::string("producer block pay")});

		user_resources_table totals_tbl(_self, owner);
		auto tot_itr = totals_tbl.find(owner);

		totals_tbl.modify(tot_itr, 0, [&](auto &tot) {
			if(asset(0) > (tot.prodreward -= asset(producer_per_block_pay)))
				tot.prodreward = asset(0);
		});
	}
	if (producer_per_vote_pay > 0)
	{
		INLINE_ACTION_SENDER(gstio::token, transfer)
		(N(gstio.token), {N(gstio.vpay), N(active)},
		 {N(gstio.vpay), owner, asset(producer_per_vote_pay), std::string("producer vote pay")});

		user_resources_table totals_tbl(_self, owner);
		auto tot_itr = totals_tbl.find(owner);

		totals_tbl.modify(tot_itr, 0, [&](auto &tot) {
			tot.prodreward = asset(0);
		});
	}
}

//2019/03/12 以下
using namespace gstio;
void system_contract::prodrewards(const account_name &owner)
{
	require_auth(owner);

	const auto &prod = _producers.get(owner);
	gstio_assert(prod.active(), "producer does not have an active key");

	gstio_assert(_gstate.total_activated_stake >= min_activated_stake,
				 "cannot claim rewards until the chain is activated (at least 15% of all tokens participate in voting)");

	auto ct = current_time();

	gstio_assert(ct - prod.last_claim_time > useconds_per_day, "already claimed rewards within past day");

	const asset token_supply = token(N(gstio.token)).get_supply(symbol_type(system_token_symbol).name());
	const auto usecs_since_last_fill = ct - _gstate.last_pervote_bucket_fill;

	int64_t to_per_block_pay = 0;
	int64_t to_per_vote_pay = 0;
	if (usecs_since_last_fill > 0 && _gstate.last_pervote_bucket_fill > 0)
	{
		auto new_tokens = static_cast<int64_t>((continuous_rate * double(token_supply.amount) * double(usecs_since_last_fill)) / double(useconds_per_year));

		auto to_producers = new_tokens / 5;				   //用于给超级节点的奖励，总增发量的1/5
		to_per_block_pay = to_producers / 4;			   //用于出块的奖励，给超级节点奖励的1/4
		to_per_vote_pay = to_producers - to_per_block_pay; //用于得票奖励，给超级节点奖励的3/4

		to_per_vote_pay += _gstate.pervote_bucket;   //更新可用于得票奖励的总token量
		to_per_block_pay += _gstate.perblock_bucket; //更新可用于出块奖励的总token量
	}

	int64_t producer_per_block_pay = 0;
	if (_gstate.total_unpaid_blocks > 0)
	{
		producer_per_block_pay = (to_per_block_pay * prod.unpaid_blocks) / _gstate.total_unpaid_blocks;
	}
	int64_t producer_per_vote_pay = 0;
	if (_gstate.total_producer_vote_weight > 0)
	{
		producer_per_vote_pay = int64_t((to_per_vote_pay * prod.total_votes) / _gstate.total_producer_vote_weight);
	}
	if (producer_per_vote_pay < min_pervote_daily_pay)
	{
		producer_per_vote_pay = 0;
	}

	int64_t prodpay = producer_per_block_pay + producer_per_vote_pay;
	user_resources_table totals_tbl(_self, owner);
	auto tot_itr = totals_tbl.find(owner);

	totals_tbl.modify(tot_itr, 0, [&](auto &tot) {
		tot.prodreward = asset(prodpay);
	});
}
//2019/03/12 以上

} //namespace gstiosystem

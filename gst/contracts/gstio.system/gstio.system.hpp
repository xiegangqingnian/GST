/**
 *  @file
 *  @copyright defined in gst/LICENSE.txt
 */
#pragma once

#include <gstio.system/native.hpp>
#include <gstiolib/asset.hpp>
#include <gstiolib/time.hpp>
#include <gstiolib/privileged.hpp>
#include <gstiolib/singleton.hpp>
#include <gstio.system/exchange_state.hpp>

#include <string>

namespace gstiosystem
{

using gstio::asset;
using gstio::block_timestamp;
using gstio::const_mem_fun;
using gstio::indexed_by;

struct name_bid
{
	account_name newname;
	account_name high_bidder;
	int64_t high_bid = 0; ///< negative high_bid == closed auction waiting to be claimed
	uint64_t last_bid_time = 0;

	auto primary_key() const { return newname; }
	uint64_t by_high_bid() const { return static_cast<uint64_t>(-high_bid); }
};

typedef gstio::multi_index<N(namebids), name_bid,
						   indexed_by<N(highbid), const_mem_fun<name_bid, uint64_t, &name_bid::by_high_bid>>>
	name_bid_table;

struct gstio_global_state : gstio::blockchain_parameters
{
	uint64_t free_ram() const { return max_ram_size - total_ram_bytes_reserved; }

	uint64_t max_ram_size = 64ll * 1024 * 1024 * 1024;
	uint64_t total_ram_bytes_reserved = 0;
	int64_t total_ram_stake = 0;

	block_timestamp last_producer_schedule_update;
	uint64_t last_pervote_bucket_fill = 0;
	int64_t pervote_bucket = 0;
	int64_t perblock_bucket = 0;
	int64_t pertovote_bucket = 0;
	uint32_t total_unpaid_blocks = 0; /// all blocks which have been produced but not paid
	int64_t total_activated_stake = 0;
	uint64_t thresh_activated_stake_time = 0;
	uint16_t last_producer_schedule_size = 0;
	double total_producer_vote_weight = 0; /// the sum of all producer votes
	block_timestamp last_name_close;

	// explicit serialization macro is not necessary, used here only to improve compilation time
	GSTLIB_SERIALIZE_DERIVED(gstio_global_state, gstio::blockchain_parameters,
							 (max_ram_size)(total_ram_bytes_reserved)(total_ram_stake)(last_producer_schedule_update)(last_pervote_bucket_fill)(pervote_bucket)(perblock_bucket)(total_unpaid_blocks)(total_activated_stake)(thresh_activated_stake_time)(last_producer_schedule_size)(total_producer_vote_weight)(last_name_close))
};

struct producer_info
{
	account_name owner;
	double total_votes = 0;
	gstio::public_key producer_key; /// a packed public key object
	bool is_active = true;
	std::string url;
	uint32_t unpaid_blocks = 0;
	uint64_t last_claim_time = 0;
	uint16_t location = 0;

	uint64_t primary_key() const { return owner; }
	double by_votes() const { return is_active ? -total_votes : total_votes; }
	bool active() const { return is_active; }
	void deactivate()
	{
		producer_key = public_key();
		is_active = false;
	}

	// explicit serialization macro is not necessary, used here only to improve compilation time
	GSTLIB_SERIALIZE(producer_info, (owner)(total_votes)(producer_key)(is_active)(url)(unpaid_blocks)(last_claim_time)(location))
};

struct voter_info
{
	account_name owner = 0;				 /// the voter
	account_name proxy = 0;				 /// the proxy set by the voter, if any
	std::vector<account_name> producers; /// the producers approved by this voter if no proxy set
	int64_t staked = 0;
	int64_t votereward = 0;
	uint64_t last_claim_time = 0;
	/**
		 *  Every time a vote is cast we must first "undo" the last vote weight, before casting the
		 *  new vote weight.  Vote weight is calculated as:
		 *
		 *  stated.amount * 2 ^ ( weeks_since_launch/weeks_per_year)
		 */
	double last_vote_weight = 0; /// the vote weight cast the last time the vote was updated

	/**
		 * Total vote weight delegated to this voter.
		 */
	double proxied_vote_weight = 0; /// the total vote weight delegated to this voter as a proxy
	bool is_proxy = 0;				/// whether the voter is a proxy for others

	uint32_t reserved1 = 0;
	time reserved2 = 0;
	gstio::asset reserved3;

	uint64_t primary_key() const { return owner; }

	// explicit serialization macro is not necessary, used here only to improve compilation time
	GSTLIB_SERIALIZE(voter_info, (owner)(proxy)(producers)(staked)(votereward)(last_claim_time)(last_vote_weight)(proxied_vote_weight)(is_proxy)(reserved1)(reserved2)(reserved3))
};

struct user_resources
{
	account_name owner;
	asset net_weight;
	asset cpu_weight;
	int64_t ram_bytes = 0;
	asset votereward = asset(0);
	asset prodreward = asset(0); //2019/03/12

	//asset staked;

	uint64_t primary_key() const { return owner; }

	// explicit serialization macro is not necessary, used here only to improve compilation time
	GSTLIB_SERIALIZE(user_resources, (owner)(net_weight)(cpu_weight)(ram_bytes)(votereward)(prodreward))
};

typedef gstio::multi_index<N(userres), user_resources> user_resources_table;

typedef gstio::multi_index<N(voters), voter_info> voters_table;

typedef gstio::multi_index<N(producers), producer_info,
						   indexed_by<N(prototalvote), const_mem_fun<producer_info, double, &producer_info::by_votes>>>
	producers_table;

typedef gstio::singleton<N(global), gstio_global_state> global_state_singleton;

//   static constexpr uint32_t     max_inflation_rate = 5;  // 5% annual inflation
static constexpr uint32_t seconds_per_day = 24 * 3600;
static constexpr uint64_t system_token_symbol = CORE_SYMBOL;

class system_contract : public native
{
  private:
	voters_table _voters;
	producers_table _producers;
	global_state_singleton _global;

	gstio_global_state _gstate;
	rammarket _rammarket;

  public:
	system_contract(account_name s);
	~system_contract();

	// Actions:
	void onblock(block_timestamp timestamp, account_name producer);
	// const block_header& header ); /// only parse first 3 fields of block header

	// functions defined in delegate_bandwidth.cpp

	/**
 *  Stakes SYS from the balance of 'from' for the benfit of 'receiver'.
 *  If transfer == true, then 'receiver' can unstake to their account
 *  Else 'from' can unstake at any time.
 */
	void delegatebw(account_name from, account_name receiver,
					asset stake_net_quantity, bool transfer);

	/**
		 *  Decreases the total tokens delegated by from to receiver and/or
		 *  frees the memory associated with the delegation if there is nothing
		 *  left to delegate.
		 *
		 *  This will cause an immediate reduction in net/cpu bandwidth of the
		 *  receiver.
		 *
		 *  A transaction is scheduled to send the tokens back to 'from' after
		 *  the staking period has passed. If existing transaction is scheduled, it
		 *  will be canceled and a new transaction issued that has the combined
		 *  undelegated amount.
		 *
		 *  The 'from' account loses voting power as a result of this call and
		 *  all producer tallies are updated.
		 */
	void undelegatebw(account_name from, account_name receiver,
					  asset unstake_net_quantity);

	/**
		 * Increases receiver's ram quota based upon current price and quantity of
		 * tokens provided. An inline transfer from receiver to system contract of
		 * tokens will be executed.
		 */

	void buymembytes(account_name buyer, account_name receiver, uint32_t bytes);
	void buymem(account_name buyer, account_name receiver, asset tokens);

	void buyram(account_name buyer, account_name receiver, asset tokens);
	void buyrambytes(account_name buyer, account_name receiver, uint32_t bytes);

	/**
		 *  Reduces quota my bytes and then performs an inline transfer of tokens
		 *  to receiver based upon the average purchase price of the original quota.
		 */
	void sellram(account_name receiver, int64_t bytes);

	/**
		 *  This action is called after the delegation-period to claim all pending
		 *  unstaked tokens belonging to owner
		 */
	void refund(account_name owner);

	// functions defined in voting.cpp

	void regproducer(const account_name producer, const public_key &producer_key, const std::string &url, uint16_t location);

	void unregprod(const account_name producer);

	void setram(uint64_t max_ram_size);

	void voteproducer(const account_name voter_name, const account_name proxy, const std::vector<account_name> &producers);
	//    	  void voteproducer(const account_name voter_name, const account_name proxy, asset vote_quantity, const std::vector<account_name>& producers);

	void regproxy(const account_name proxy, bool isproxy);

	void setparams(const gstio::blockchain_parameters &params);

	// functions defined in producer_pay.cpp
	void claimrewards(const account_name &owner);

	void prodrewards(const account_name &owner); //2019/03/12

	void claimvrew(const account_name &owner); //2019/03/11

	void voterewards(const account_name &owner);
	// asset floor ( asset x );

	void setpriv(account_name account, uint8_t ispriv);

	void rmvproducer(account_name producer);

	void bidname(account_name bidder, account_name newname, asset bid);

  private:
	void update_elected_producers(block_timestamp timestamp);

	// Implementation details:

	//defind in delegate_bandwidth.cpp
	void changebw(account_name from, account_name receiver,
				  asset stake_net_quantity, asset stake_cpu_quantity, bool transfer);

	void changebw_create_account(account_name from, account_name receiver,
								 asset stake_net_quantity, asset stake_cpu_quantity, bool transfer);

	void mydelegatebw(account_name from, account_name receiver,
					  asset stake_net_quantity, asset stake_cpu_quantity, bool transfer);

	//defined in voting.hpp
	static gstio_global_state get_default_parameters();

	void update_votes(const account_name voter, const account_name proxy, const std::vector<account_name> &producers, bool voting);

	// defined in voting.cpp
	void propagate_weight_change(const voter_info &voter);
};

} // namespace gstiosystem

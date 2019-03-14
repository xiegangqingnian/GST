/**
 *  @file
 *  @copyright defined in gst/LICENSE.txt
 */
#include "gstio.system.hpp"

#include <gstiolib/gstio.hpp>
#include <gstiolib/crypto.h>
#include <gstiolib/print.hpp>
#include <gstiolib/datastream.hpp>
#include <gstiolib/serialize.hpp>
#include <gstiolib/multi_index.hpp>
#include <gstiolib/privileged.hpp>
#include <gstiolib/singleton.hpp>
#include <gstiolib/transaction.hpp>
#include <gstio.token/gstio.token.hpp>

#include <algorithm>
#include <cmath>

namespace gstiosystem {
   using gstio::indexed_by;
   using gstio::const_mem_fun;
   using gstio::bytes;
   using gstio::print;
   using gstio::singleton;
   using gstio::transaction;

   /**
    *  This method will create a producer_config and producer_info object for 'producer'
    *
    *  @pre producer is not already registered
    *  @pre producer to register is an account
    *  @pre authority of producer to register
    *
    */
   void system_contract::regproducer( const account_name producer, const gstio::public_key& producer_key, const std::string& url, uint16_t location ) {
      gstio_assert( url.size() < 512, "url too long" );
      gstio_assert( producer_key != gstio::public_key(), "public key should not be the default value" );
      require_auth( producer );

      auto prod = _producers.find( producer );

      if ( prod != _producers.end() ) {
         _producers.modify( prod, producer, [&]( producer_info& info ){
               info.producer_key = producer_key;
               info.is_active    = true;
               info.url          = url;
               info.location     = location;
            });
      } else {
         _producers.emplace( producer, [&]( producer_info& info ){
               info.owner         = producer;
               info.total_votes   = 0;
               info.producer_key  = producer_key;
               info.is_active     = true;
               info.url           = url;
               info.location      = location;
         });
      }
   }

   void system_contract::unregprod( const account_name producer ) {
      require_auth( producer );

      const auto& prod = _producers.get( producer, "producer not found" );

      _producers.modify( prod, 0, [&]( producer_info& info ){
            info.deactivate();
      });
   }

   void system_contract::update_elected_producers( block_timestamp block_time ) {
      _gstate.last_producer_schedule_update = block_time;

      auto idx = _producers.get_index<N(prototalvote)>();

      std::vector< std::pair<gstio::producer_key,uint16_t> > top_producers;

	  int producer_cnt = 3;
      top_producers.reserve(producer_cnt);

      for ( auto it = idx.cbegin(); it != idx.cend() && top_producers.size() < producer_cnt && 0 < it->total_votes && it->active(); ++it ) {
         top_producers.emplace_back( std::pair<gstio::producer_key,uint16_t>({{it->owner, it->producer_key}, it->location}) );
      }

      if ( top_producers.size() < _gstate.last_producer_schedule_size ) {
         return;
      }

      /// sort by producer name
      std::sort( top_producers.begin(), top_producers.end() );

      std::vector<gstio::producer_key> producers;

      producers.reserve(top_producers.size());
      for( const auto& item : top_producers )
         producers.push_back(item.first);

      bytes packed_schedule = pack(producers);

      if( set_proposed_producers( packed_schedule.data(),  packed_schedule.size() ) >= 0 ) {
         _gstate.last_producer_schedule_size = static_cast<decltype(_gstate.last_producer_schedule_size)>( top_producers.size() );
      }
   }

   double stake2vote( int64_t staked ) {
      /// TODO subtract 2080 brings the large numbers closer to this decade
      double weight = int64_t( (now() - (block_timestamp::block_timestamp_epoch / 1000)) / (seconds_per_day * 7) )  / double( 52 );
      return double(staked) * std::pow( 2, weight );
   }
   /**
    *  @pre producers must be sorted from lowest to highest and must be registered and active
    *  @pre if proxy is set then no producers can be voted for
    *  @pre if proxy is set then proxy account must exist and be registered as a proxy
    *  @pre every listed producer or proxy must have been previously registered
    *  @pre voter must authorize this action
    *  @pre voter must have previously staked some GST for voting
    *  @pre voter->staked must be up to date
    *
    *  @post every producer previously voted for will have vote reduced by previous vote weight
    *  @post every producer newly voted for will have vote increased by new vote amount
    *  @post prior proxy will proxied_vote_weight decremented by previous vote weight
    *  @post new proxy will proxied_vote_weight incremented by new vote weight
    *
    *  If voting for a proxy, the producer votes will not change until the proxy updates their own vote.
    */
   void system_contract::voteproducer( const account_name voter_name, const account_name proxy, const std::vector<account_name>& producers ) {
      require_auth( voter_name );
      update_votes( voter_name, proxy, producers, true );
   }

   void system_contract::update_votes( const account_name voter_name, const account_name proxy, const std::vector<account_name>& producers, bool voting ) {
      //validate input
      if ( proxy ) {
         gstio_assert( producers.size() == 0, "cannot vote for producers and proxy at same time" );
         gstio_assert( voter_name != proxy, "cannot proxy to self" );
         require_recipient( proxy );
      } else {
         gstio_assert( producers.size() <= 30, "attempt to vote for too many producers" );
         for( size_t i = 1; i < producers.size(); ++i ) {
            gstio_assert( producers[i-1] < producers[i], "producer votes must be unique and sorted" );
         }
      }

      auto voter = _voters.find(voter_name);
      gstio_assert( voter != _voters.end(), "user must stake before they can vote" ); /// staking creates voter object
      gstio_assert( !proxy || !voter->is_proxy, "account registered as a proxy is not allowed to use a proxy" );

      /**
       * The first time someone votes we calculate and set last_vote_weight, since they cannot unstake until
       * after total_activated_stake hits threshold, we can use last_vote_weight to determine that this is
       * their first vote and should consider their stake activated.
       */
      if( voter->last_vote_weight <= 0.0 ) {
         _gstate.total_activated_stake += voter->staked;
         if( _gstate.total_activated_stake >= min_activated_stake && _gstate.thresh_activated_stake_time == 0 ) {
            _gstate.thresh_activated_stake_time = current_time();
         }
      }

      auto new_vote_weight = stake2vote( voter->staked );
      if( voter->is_proxy ) {
         new_vote_weight += voter->proxied_vote_weight;
      }

      boost::container::flat_map<account_name, pair<double, bool /*new*/> > producer_deltas;
      if ( voter->last_vote_weight > 0 ) {
         if( voter->proxy ) {
            auto old_proxy = _voters.find( voter->proxy );
            gstio_assert( old_proxy != _voters.end(), "old proxy not found" ); //data corruption
            _voters.modify( old_proxy, 0, [&]( auto& vp ) {
                  vp.proxied_vote_weight -= voter->last_vote_weight;
               });
            propagate_weight_change( *old_proxy );
         } else {
            for( const auto& p : voter->producers ) {
               auto& d = producer_deltas[p];
               d.first -= voter->last_vote_weight;
               d.second = false;
            }
         }
      }

      if( proxy ) {
         auto new_proxy = _voters.find( proxy );
         gstio_assert( new_proxy != _voters.end(), "invalid proxy specified" ); //if ( !voting ) { data corruption } else { wrong vote }
         gstio_assert( !voting || new_proxy->is_proxy, "proxy not found" );
         if ( new_vote_weight >= 0 ) {
            _voters.modify( new_proxy, 0, [&]( auto& vp ) {
                  vp.proxied_vote_weight += new_vote_weight;
               });
            propagate_weight_change( *new_proxy );
         }
      } else {
         if( new_vote_weight >= 0 ) {
            for( const auto& p : producers ) {
               auto& d = producer_deltas[p];
               d.first += new_vote_weight;
               d.second = true;
            }
         }
      }

      for( const auto& pd : producer_deltas ) {
         auto pitr = _producers.find( pd.first );
         if( pitr != _producers.end() ) {
            gstio_assert( !voting || pitr->active() || !pd.second.second /* not from new set */, "producer is not currently registered" );
            _producers.modify( pitr, 0, [&]( auto& p ) {
               p.total_votes += pd.second.first;
               if ( p.total_votes < 0 ) { // floating point arithmetics can give small negative numbers
                  p.total_votes = 0;
               }
               _gstate.total_producer_vote_weight += pd.second.first;
               //gstio_assert( p.total_votes >= 0, "something bad happened" );
            });
         } else {
            gstio_assert( !pd.second.second /* not from new set */, "producer is not registered" ); //data corruption
         }
      }

      _voters.modify( voter, 0, [&]( auto& av ) {
         av.last_vote_weight = new_vote_weight;
         av.producers = producers;
         av.proxy     = proxy;
      });
   }

   /**
    *  An account marked as a proxy can vote with the weight of other accounts which
    *  have selected it as a proxy. Other accounts must refresh their voteproducer to
    *  update the proxy's weight.
    *
    *  @param isproxy - true if proxy wishes to vote on behalf of others, false otherwise
    *  @pre proxy must have something staked (existing row in voters table)
    *  @pre new state must be different than current state
    */
   void system_contract::regproxy( const account_name proxy, bool isproxy ) {
      require_auth( proxy );

      auto pitr = _voters.find(proxy);
      if ( pitr != _voters.end() ) {
         gstio_assert( isproxy != pitr->is_proxy, "action has no effect" );
         gstio_assert( !isproxy || !pitr->proxy, "account that uses a proxy is not allowed to become a proxy" );
         _voters.modify( pitr, 0, [&]( auto& p ) {
               p.is_proxy = isproxy;
            });
         propagate_weight_change( *pitr );
      } else {
         _voters.emplace( proxy, [&]( auto& p ) {
               p.owner  = proxy;
               p.is_proxy = isproxy;
            });
      }
   }

   void system_contract::propagate_weight_change( const voter_info& voter ) {
      gstio_assert( voter.proxy == 0 || !voter.is_proxy, "account registered as a proxy is not allowed to use a proxy" );
      double new_weight = stake2vote( voter.staked );
      if ( voter.is_proxy ) {
         new_weight += voter.proxied_vote_weight;
      }

      /// don't propagate small changes (1 ~= epsilon)
      if ( fabs( new_weight - voter.last_vote_weight ) > 1 )  {
         if ( voter.proxy ) {
            auto& proxy = _voters.get( voter.proxy, "proxy not found" ); //data corruption
            _voters.modify( proxy, 0, [&]( auto& p ) {
                  p.proxied_vote_weight += new_weight - voter.last_vote_weight;
               }
            );
            propagate_weight_change( proxy );
         } else {
            auto delta = new_weight - voter.last_vote_weight;
            for ( auto acnt : voter.producers ) {
               auto& pitr = _producers.get( acnt, "producer not found" ); //data corruption
               _producers.modify( pitr, 0, [&]( auto& p ) {
                     p.total_votes += delta;
                     _gstate.total_producer_vote_weight += delta;
               });
            }
         }
      }
      _voters.modify( voter, 0, [&]( auto& v ) {
            v.last_vote_weight = new_weight;
         }
      );
   }

   //2019/03/11 以下
using namespace gstio;
void system_contract::claimvrew(const account_name &owner)
{
   require_auth(owner);

   const auto &vtr = _voters.get(owner);
   //gstio_assert(vtr.active(), "producer does not have an active key"); //2019/03/11

   gstio_assert(_gstate.total_activated_stake >= min_activated_stake,
                "cannot claim rewards until the chain is activated (at least 15% of all tokens participate in voting)");

   auto ct = current_time();

   gstio_assert(ct - vtr.last_claim_time > useconds_per_day, "already claimed rewards within past day");

   const asset token_supply = token(N(gstio.token)).get_supply(symbol_type(system_token_symbol).name());
   const auto usecs_since_last_fill = ct - _gstate.last_pervote_bucket_fill;

   if (usecs_since_last_fill > 0 && _gstate.last_pervote_bucket_fill > 0)
   {
      auto new_tokens = static_cast<int64_t>((continuous_rate * double(token_supply.amount) * double(usecs_since_last_fill)) / double(useconds_per_year));

      auto to_producers = new_tokens / 5;                          //用于给超级节点的奖励，总增发量的1/5
      auto to_voters_pay = new_tokens / 5;                         //2019/03/11
      auto to_savings = new_tokens - to_producers - to_voters_pay; //2019/03/11	存起来给社区提案的奖励，总增发量的3/5
      auto to_per_block_pay = to_producers / 4;                    //用于出块的奖励，给超级节点奖励的1/4
      auto to_per_vote_pay = to_producers - to_per_block_pay;      //用于得票奖励，给超级节点奖励的3/4

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
       {N(gstio), N(gstio.vote), asset(to_voters_pay), "fund per-tovote bucket"});

      _gstate.pervote_bucket += to_per_vote_pay;   //更新可用于得票奖励的总token量
      _gstate.perblock_bucket += to_per_block_pay; //更新可用于出块奖励的总token量
      _gstate.pertovote_bucket += to_voters_pay;   //2019/03/11	更新可用于投票奖励的总token量

      _gstate.last_pervote_bucket_fill = ct; //更新增发时间点
   }

   int64_t voter_per_vote_pay = 0;
   if (_gstate.total_producer_vote_weight > 0)
   {
      voter_per_vote_pay = int64_t((_gstate.pervote_bucket * vtr.last_vote_weight) / _gstate.total_producer_vote_weight);
   }
   if (voter_per_vote_pay < 0)
   {
      voter_per_vote_pay = 0;
   }
   _gstate.pervote_bucket -= voter_per_vote_pay;

   _voters.modify(vtr, 0, [&](auto &p) {
      p.last_claim_time = ct;
   });

   if (voter_per_vote_pay > 0)
   {
      INLINE_ACTION_SENDER(gstio::token, transfer)
      (N(gstio.token), {N(gstio.vote), N(active)},
       {N(gstio.vote), owner, asset(voter_per_vote_pay), std::string("voter vote pay")});

      user_resources_table totals_tbl(_self, owner);
      auto tot_itr = totals_tbl.find(owner);

      totals_tbl.modify(tot_itr, 0, [&](auto &tot) {
         tot.votereward = asset(0);
      });
   }
}
//2019/03/11 以上

//2019/03/12 以下
using namespace gstio;
void system_contract::voterewards(const account_name &owner)
{
   require_auth(owner);

   const auto &vtr = _voters.get(owner);
   //gstio_assert(vtr.active(), "producer does not have an active key"); //2019/03/11

   gstio_assert(_gstate.total_activated_stake >= min_activated_stake,
                "cannot claim rewards until the chain is activated (at least 15% of all tokens participate in voting)");

   auto ct = current_time();

   const asset token_supply = token(N(gstio.token)).get_supply(symbol_type(system_token_symbol).name());
   const auto usecs_since_last_fill = ct - _gstate.last_pervote_bucket_fill;

   int64_t to_voters_pay = 0;
   if (usecs_since_last_fill > 0 && _gstate.last_pervote_bucket_fill > 0)
   {
      auto new_tokens = static_cast<int64_t>((continuous_rate * double(token_supply.amount) * double(usecs_since_last_fill)) / double(useconds_per_year));
      to_voters_pay = new_tokens / 5; //2019/03/11

      to_voters_pay += _gstate.pertovote_bucket; //2019/03/11	更新可用于投票奖励的总token量
   }

   int64_t voter_per_vote_pay = 0;
   if (_gstate.total_producer_vote_weight > 0)
   {
      voter_per_vote_pay = int64_t((to_voters_pay * vtr.last_vote_weight) / _gstate.total_producer_vote_weight);
   }

   user_resources_table totals_tbl(_self, owner);
   auto tot_itr = totals_tbl.find(owner);

   totals_tbl.modify(tot_itr, 0, [&](auto &tot) {
      tot.votereward = asset(voter_per_vote_pay);
   });
}
// void system_contract::voterewards(const account_name &owner)
// {
//    require_auth(owner);

//    user_resources_table totals_tbl(_self, owner);
//    auto tot_itr = totals_tbl.find(owner);

//    totals_tbl.modify(tot_itr, 0, [&](auto &tot) {
//       //		gstio_assert(tot.reward.amount >= reward_max, "reward is not available yet");//��ϢֵҪ����0.0001  �ſ���ȡ
//       gstio::print("tot......reward :", tot.reward.amount, "\n");

//       //	gstio::print("max_available_reward.amount :", max_available_reward.amount, "\n");

//       INLINE_ACTION_SENDER(gstio::token, transfer)
//       (N(gstio.token), {N(gstio.vote), N(active)},
//        {N(gstio.vote), tot_itr->owner, asset(tot.reward), std::string("voter  reward")});

//       tot.reward = asset();
//    });
// }
//2019/03/12 以上

} /// namespace gstiosystem

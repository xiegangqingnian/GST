#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <gstio/testing/tester.hpp>
#include <gstio/chain/abi_serializer.hpp>
#include <gstio/chain/wasm_gstio_constraints.hpp>
#include <gstio/chain/resource_limits.hpp>
#include <gstio/chain/exceptions.hpp>
#include <gstio/chain/wast_to_wasm.hpp>
#include <gstio/chain_plugin/chain_plugin.hpp>

#include <asserter/asserter.wast.hpp>
#include <asserter/asserter.abi.hpp>

#include <stltest/stltest.wast.hpp>
#include <stltest/stltest.abi.hpp>

#include <gstio.system/gstio.system.wast.hpp>
#include <gstio.system/gstio.system.abi.hpp>

#include <gstio.token/gstio.token.wast.hpp>
#include <gstio.token/gstio.token.abi.hpp>

#include <fc/io/fstream.hpp>

#include <Runtime/Runtime.h>

#include <fc/variant_object.hpp>
#include <fc/io/json.hpp>

#include <array>
#include <utility>

#ifdef NON_VALIDATING_TEST
#define TESTER tester
#else
#define TESTER validating_tester
#endif

using namespace gstio;
using namespace gstio::chain;
using namespace gstio::testing;
using namespace fc;

BOOST_AUTO_TEST_SUITE(get_table_tests)

BOOST_FIXTURE_TEST_CASE( get_scope_test, TESTER ) try {
   produce_blocks(2);

   create_accounts({ N(gstio.token), N(gstio.ram), N(gstio.ramfee), N(gstio.stake),
      N(gstio.bpay), N(gstio.vpay), N(gstio.saving), N(gstio.names) });

   std::vector<account_name> accs{N(inita), N(initb), N(initc), N(initd)};
   create_accounts(accs);
   produce_block();

   set_code( N(gstio.token), gstio_token_wast );
   set_abi( N(gstio.token), gstio_token_abi );
   produce_blocks(1);

   // create currency 
   auto act = mutable_variant_object()
         ("issuer",       "gstio")
         ("maximum_supply", gstio::chain::asset::from_string("1000000000.0000 SYS"));
   push_action(N(gstio.token), N(create), N(gstio.token), act );

   // issue
   for (account_name a: accs) {
      push_action( N(gstio.token), N(issue), "gstio", mutable_variant_object()
                  ("to",      name(a) )
                  ("quantity", gstio::chain::asset::from_string("999.0000 SYS") )
                  ("memo", "")
                  );
   }
   produce_blocks(1);

   // iterate over scope
   gstio::chain_apis::read_only plugin(*(this->control), fc::microseconds(INT_MAX));
   gstio::chain_apis::read_only::get_table_by_scope_params param{N(gstio.token), N(accounts), "inita", "", 10};
   gstio::chain_apis::read_only::get_table_by_scope_result result = plugin.read_only::get_table_by_scope(param);

   BOOST_REQUIRE_EQUAL(4, result.rows.size());
   BOOST_REQUIRE_EQUAL("", result.more);
   if (result.rows.size() >= 4) {
      BOOST_REQUIRE_EQUAL(name(N(gstio.token)), result.rows[0].code);
      BOOST_REQUIRE_EQUAL(name(N(inita)), result.rows[0].scope);
      BOOST_REQUIRE_EQUAL(name(N(accounts)), result.rows[0].table);
      BOOST_REQUIRE_EQUAL(name(N(gstio)), result.rows[0].payer);
      BOOST_REQUIRE_EQUAL(1, result.rows[0].count);

      BOOST_REQUIRE_EQUAL(name(N(initb)), result.rows[1].scope);
      BOOST_REQUIRE_EQUAL(name(N(initc)), result.rows[2].scope);
      BOOST_REQUIRE_EQUAL(name(N(initd)), result.rows[3].scope);
   }

   param.lower_bound = "initb";
   param.upper_bound = "initd";
   result = plugin.read_only::get_table_by_scope(param);
   BOOST_REQUIRE_EQUAL(2, result.rows.size());
   BOOST_REQUIRE_EQUAL("", result.more);
   if (result.rows.size() >= 2) {
      BOOST_REQUIRE_EQUAL(name(N(initb)), result.rows[0].scope);
      BOOST_REQUIRE_EQUAL(name(N(initc)), result.rows[1].scope);      
   }

   param.limit = 1;
   result = plugin.read_only::get_table_by_scope(param);
   BOOST_REQUIRE_EQUAL(1, result.rows.size());
   BOOST_REQUIRE_EQUAL("initc", result.more);

   param.table = name(0);
   result = plugin.read_only::get_table_by_scope(param);
   BOOST_REQUIRE_EQUAL(1, result.rows.size());
   BOOST_REQUIRE_EQUAL("initc", result.more);

   param.table = N(invalid);
   result = plugin.read_only::get_table_by_scope(param);
   BOOST_REQUIRE_EQUAL(0, result.rows.size());
   BOOST_REQUIRE_EQUAL("", result.more); 

} FC_LOG_AND_RETHROW() /// get_scope_test

BOOST_AUTO_TEST_SUITE_END()


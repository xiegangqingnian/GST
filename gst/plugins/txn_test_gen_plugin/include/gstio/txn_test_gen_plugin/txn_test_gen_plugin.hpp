/**
 *  @file
 *  @copyright defined in gst/LICENSE.txt
 */
#pragma once
#include <appbase/application.hpp>
#include <gstio/http_plugin/http_plugin.hpp>
#include <gstio/chain_plugin/chain_plugin.hpp>

namespace gstio {

using namespace appbase;

class txn_test_gen_plugin : public appbase::plugin<txn_test_gen_plugin> {
public:
   txn_test_gen_plugin();
   ~txn_test_gen_plugin();

   APPBASE_PLUGIN_REQUIRES((http_plugin)(chain_plugin))
   virtual void set_program_options(options_description&, options_description& cfg) override;
 
   void plugin_initialize(const variables_map& options);
   void plugin_startup();
   void plugin_shutdown();

private:
   std::unique_ptr<struct txn_test_gen_plugin_impl> my;
};

}

#include <gstiolib/action.h>
#include <gstiolib/contract.hpp>
#include <gstiolib/dispatcher.hpp>
#include <identity/interface.hpp>

namespace identity_test {
   
   using gstio::action_meta;
   using gstio::singleton;
   using std::string;
   using std::vector;

   class contract : public gstio::contract {
      public:
         static constexpr uint64_t code = N(identitytest);
         typedef singleton<N(result), uint64_t> result_table;

         using gstio::contract::contract;

         void getowner( const uint64_t identity ) {
            identity::interface iface( N(identity) );
            account_name owner = iface.get_owner_for_identity(current_receiver(), identity);
            result_table( code, 0 ).set( owner, code ); //use scope = 0 for simplicity
         }

         void getidentity( const account_name account ) {
            identity::interface iface( N(identity) );
            identity::identity_name idnt = iface.get_identity_for_account(current_receiver(), account);
            result_table( code, 0 ).set(idnt, code ); //use scope = 0 for simplicity
         }
   };

} /// namespace identity

GSTIO_ABI( identity_test::contract, (getowner)(getidentity) );

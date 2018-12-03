#include <gstiolib/gstio.hpp>
using namespace gstio;

class hello : public gstio::contract {
  public:
      using contract::contract;

      /// @abi action 
      void hi( account_name user ) {
         print( "Hello, ", name{user} );
      }
};

GSTIO_ABI( hello, (hi) )

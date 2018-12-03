#include <gstiolib/gstio.hpp>
#include <gstiolib/print.hpp>
using namespace gstio;

class payloadless : public gstio::contract {
  public:
      using contract::contract;

      void doit() {
         print( "Im a payloadless action" );
      }
};

GSTIO_ABI( payloadless, (doit) )

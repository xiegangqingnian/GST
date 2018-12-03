/**
 *  @file
 *  @copyright defined in gst/LICENSE.txt
 */

#include <gstiolib/gstio.hpp>

namespace gstio {

   class noop: public contract {
      public:
         noop( account_name self ): contract( self ) { }
         void anyaction( account_name from,
                         const std::string& /*type*/,
                         const std::string& /*data*/ )
         {
            require_auth( from );
         }
   };

   GSTIO_ABI( noop, ( anyaction ) )

} /// gstio     

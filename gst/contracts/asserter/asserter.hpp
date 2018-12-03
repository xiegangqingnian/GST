/**
 *  @file
 *  @copyright defined in gst/LICENSE.txt
 */

#include <gstiolib/gstio.hpp>

namespace asserter {
   struct assertdef {
      int8_t      condition;
      std::string message;

      GSTLIB_SERIALIZE( assertdef, (condition)(message) )
   };
}

/**
 *  @file
 *  @copyright defined in gst/LICENSE.txt
 */
#include <gstiolib/print.hpp> /// defines transfer struct (abi)

extern "C" {

    /// The apply method just prints forever
    void apply( uint64_t, uint64_t, uint64_t ) {
       int idx = 0;
       while(true) {
          gstio::print(idx++);
       }
    }
}

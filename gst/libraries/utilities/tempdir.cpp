/**
 *  @file
 *  @copyright defined in gst/LICENSE.txt
 */

#include <gstio/utilities/tempdir.hpp>

#include <cstdlib>

namespace gstio { namespace utilities {

fc::path temp_directory_path()
{
   const char* gst_tempdir = getenv("GST_TEMPDIR");
   if( gst_tempdir != nullptr )
      return fc::path( gst_tempdir );
   return fc::temp_directory_path() / "gst-tmp";
}

} } // gstio::utilities

/**
 *  @file
 *  @copyright defined in gst/LICENSE.txt
 */
#pragma once

#include <string>

namespace gstio { namespace utilities {

  std::string escape_string_for_c_source_code(const std::string& input);

} } // end namespace gstio::utilities

#pragma once

#include <gstiolib/gstio.hpp>

namespace gstio {

   class sudo : public contract {
      public:
         sudo( account_name self ):contract(self){}

         void exec();

   };

} /// namespace gstio

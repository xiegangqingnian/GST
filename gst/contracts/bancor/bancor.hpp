/**
 *  @file
 *  @copyright defined in gst/LICENSE.txt
 */
#pragma once

#include <gstiolib/gstio.hpp>
#include <gstiolib/token.hpp>
#include <gstiolib/reflect.hpp>
#include <gstiolib/generic_currency.hpp>

#include <bancor/converter.hpp>
#include <currency/currency.hpp>

namespace bancor {
   typedef gstio::generic_currency< gstio::token<N(other),S(4,OTHER)> >  other_currency;
   typedef gstio::generic_currency< gstio::token<N(bancor),S(4,RELAY)> > relay_currency;
   typedef gstio::generic_currency< gstio::token<N(currency),S(4,CUR)> > cur_currency;

   typedef converter<relay_currency, other_currency, cur_currency > example_converter;
} /// bancor


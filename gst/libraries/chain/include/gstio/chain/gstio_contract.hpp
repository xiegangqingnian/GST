/**
 *  @file
 *  @copyright defined in gst/LICENSE.txt
 */
#pragma once

#include <gstio/chain/types.hpp>
#include <gstio/chain/contract_types.hpp>

namespace gstio { namespace chain {

   class apply_context;

   /**
    * @defgroup native_action_handlers Native Action Handlers
    */
   ///@{
   void apply_gstio_newaccount(apply_context&);
   void apply_gstio_updateauth(apply_context&);
   void apply_gstio_deleteauth(apply_context&);
   void apply_gstio_linkauth(apply_context&);
   void apply_gstio_unlinkauth(apply_context&);

   /*
   void apply_gstio_postrecovery(apply_context&);
   void apply_gstio_passrecovery(apply_context&);
   void apply_gstio_vetorecovery(apply_context&);
   */

   void apply_gstio_setcode(apply_context&);
   void apply_gstio_setabi(apply_context&);

   void apply_gstio_canceldelay(apply_context&);
   ///@}  end action handlers

} } /// namespace gstio::chain

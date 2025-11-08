/**
 * C++ version of the timer API
 * Creates mapping with chrono and objectify the timer_instance_t which
 *  becomes Instance
 */
#pragma once

#include <type_traits>

#include <timer.h>

#include <asx/chrono.hpp>

namespace asx {
   namespace timer {

      ///< Shortcut for the C++ handle
      class Instance {
         timer_instance_t instance;
      public:
         // Constructor to initialize handle
         explicit constexpr Instance() : instance(TIMER_INVALID_INSTANCE) {}

         // Constructor to initialize handle
         Instance(timer_instance_t inst) : instance(inst) {}

         // Cast operator to timer_instance_t
         operator timer_instance_t() const {
            return (timer_instance_t)instance;
         }

         // Assignment operator from timer_instance_t
         Instance& operator=(timer_instance_t inst) {
            instance = inst;
            return *this;
         }

      // Operations
      public:
         bool cancel() {
            if ( instance != timer_instance_t{TIMER_INVALID_INSTANCE} ) {
               return timer_cancel(instance);
            }

            // Instance is null
            return false;
         }
      };

      constexpr auto null = Instance();

      using duration = chrono::steady_clock::duration;
      using time_point = chrono::steady_clock::time_point;

      inline bool cancel(timer_instance_t timer_id) {
         return timer_cancel(timer_id);
      }
   }
} // End of asx namespace
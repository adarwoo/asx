/**
 * C++ version of the timer API
 * Creates mapping with chrono and objectify the timer_instance_t which
 *  becomes Instance
 */
#pragma once

#include <type_traits>

#include <timer.h>

#include <asx/chrono.hpp>
#include <asx/reactor.hpp>

namespace asx {
   namespace timer {
      // Define a steady clock based on your embedded 1ms timer
      struct steady_clock {
         using duration = std::chrono::duration<uint32_t, std::milli>;
         using rep = duration::rep; // uint32_t
         using period = duration::period; // std::milli
         using time_point = std::chrono::time_point<steady_clock>;
         static constexpr bool is_steady = true;

         // Returns the current time as a time_point
         static time_point now() noexcept {
            return time_point(duration(timer_get_count()));
         }

         // Cast operator to convert duration to timer_count_t
         static timer_count_t to_timer_count(duration d) {
            return static_cast<timer_count_t>(d.count());
         }

         // Cast operator to convert time_point to timer_count_t
         static timer_count_t to_timer_count(time_point tp) {
            return static_cast<timer_count_t>(tp.time_since_epoch().count());
         }

         static duration abs_distance(time_point a, time_point b) {
            using signed_timer_count_t = std::make_signed<timer_count_t>::type;

            timer_count_t a_count = timer::steady_clock::to_timer_count(a);
            timer_count_t b_count = timer::steady_clock::to_timer_count(b);

            // Signed difference, handles wraparound
            signed_timer_count_t diff = static_cast<signed_timer_count_t>(a_count - b_count);

            // Take absolute value (no UB since diff is signed)
            if (diff < 0) diff = -diff;

            return timer::steady_clock::duration(static_cast<timer_count_t>(diff));
         }
      };

      // Overload the lt operator to account for the roll over
      inline bool operator<(steady_clock::time_point lhs, steady_clock::time_point rhs) {
         using rep = steady_clock::rep;
         using signed_rep = std::make_signed<rep>::type;

         auto lhs_raw = steady_clock::to_timer_count(lhs);
         auto rhs_raw = steady_clock::to_timer_count(rhs);

         return static_cast<signed_rep>(lhs_raw - rhs_raw) < 0;
      }


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

      using duration = steady_clock::duration;
      using time_point = steady_clock::time_point;

      inline bool cancel(timer_instance_t timer_id) {
         return timer_cancel(timer_id);
      }
   }
} // End of asx namespace
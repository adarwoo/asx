#pragma once
// MIT License
//
// Copyright (c) 2025 software@arreckx.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <sysclk.h>
#include <timer.h>
#include <chrono>

namespace asx {
   namespace chrono {
      using cpu_tick_t = std::chrono::duration<long long, std::ratio<1, F_CPU>>;

      ///< Convert any duration into CPU ticks
      constexpr auto to_ticks = [](auto duration) -> cpu_tick_t {
         return std::chrono::duration_cast<cpu_tick_t>(duration).count();
      };

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

            timer_count_t a_count = steady_clock::to_timer_count(a);
            timer_count_t b_count = steady_clock::to_timer_count(b);

            // Signed difference, handles wraparound
            signed_timer_count_t diff = static_cast<signed_timer_count_t>(a_count - b_count);

            // Take absolute value (no UB since diff is signed)
            if (diff < 0) diff = -diff;

            return steady_clock::duration(static_cast<timer_count_t>(diff));
         }
      };

      // Define a time point representing zero time
      constexpr auto time_zero =
         chrono::steady_clock::time_point(chrono::steady_clock::duration::zero());
      
      // Overload the lt operator to account for the roll over
      inline bool operator<(steady_clock::time_point lhs, steady_clock::time_point rhs) {
         using rep = steady_clock::rep;
         using signed_rep = std::make_signed<rep>::type;

         auto lhs_raw = steady_clock::to_timer_count(lhs);
         auto rhs_raw = steady_clock::to_timer_count(rhs);

         return static_cast<signed_rep>(lhs_raw - rhs_raw) < 0;
      }
   }
}

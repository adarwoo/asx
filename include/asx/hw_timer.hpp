#pragma once
#pragma GCC diagnostic ignored "-Warray-bounds"
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

#include <avr/io.h>

#include <cstdint>
#include <tuple>
#include <limits>

#include "asx/reactor.hpp"
#include "asx/chrono.hpp"

namespace asx {
   namespace hw_timer
   {
      extern reactor::Handle on_timera_compare0;
      extern reactor::Handle on_timera_compare1;
      extern reactor::Handle on_timera_compare2;
      extern reactor::Handle on_timerb_compare;
      extern reactor::Handle on_timera_ovf;

      extern uint8_t timera_config_flag;

      // Single use flag
      constexpr auto single_use = uint8_t{1U<<1};

      enum class mode : uint8_t {
         period,
         timeout,
         input_capture_on_event,
         input_capture_freq,
         input_capture_pwm,
         pwm,
         single_shot,
         pwm_8bits
      };

      template <typename T>
      struct Counting {
         using Type = T;

         static constexpr auto maximum = std::numeric_limits<Type>::max();

         /** 8 for 8-bit timer, 16 for 16-bit timer */
         static constexpr uint8_t maximumPower2 = sizeof(Type) * 8;
      };

      using Counting8 = Counting<uint8_t>;
      using Counting16 = Counting<uint16_t>;

      /**
       * Specialized instance of the timer A
       */
      template<asx::chrono::cpu_tick_t::rep N>
      struct TimerDuration {
         // Recover cpu_tick_t from the raw tick count N
         static constexpr asx::chrono::cpu_tick_t duration = asx::chrono::cpu_tick_t(N);

         static constexpr auto count() {
            return duration.count();
         }

      };

      template<class Duration>
      struct TimerA
      {
         using type_t = TCA_t;  // Specify the timer type as TCA_t
         using value_t = Counting16;
         using self = TimerA;

         static inline reactor::Mask clear_masks{0};

         ///< @brief Possible prescaling values
         static constexpr TCA_SINGLE_CLKSEL_t clksel[] = {
            TCA_SINGLE_CLKSEL_DIV1_gc,
            TCA_SINGLE_CLKSEL_DIV2_gc,
            TCA_SINGLE_CLKSEL_DIV4_gc,
            TCA_SINGLE_CLKSEL_DIV8_gc,
            TCA_SINGLE_CLKSEL_DIV16_gc,
            TCA_SINGLE_CLKSEL_DIV64_gc,
            TCA_SINGLE_CLKSEL_DIV256_gc,
            TCA_SINGLE_CLKSEL_DIV1024_gc
         };

         ///< Actualling prescaling count - mapping clksel
         static constexpr long prescalers[] = {1, 2, 4, 8, 16, 64, 256, 1024};

         // Use statically only (there is only 1 timerA)
         TimerA() = delete;

         static constexpr TCA_SINGLE_t &TCA() {
            return TCA0.SINGLE;
         }

         // Replace the lambda function with this constexpr function
         static constexpr auto set_prescaler_for_maximum_ticks() {
            return (
                  Duration::count() <= prescalers[0] * value_t::maximum) ? std::make_tuple(prescalers[0], clksel[0])
               : (Duration::count() <= prescalers[1] * value_t::maximum) ? std::make_tuple(prescalers[1], clksel[1])
               : (Duration::count() <= prescalers[2] * value_t::maximum) ? std::make_tuple(prescalers[2], clksel[2])
               : (Duration::count() <= prescalers[3] * value_t::maximum) ? std::make_tuple(prescalers[3], clksel[3])
               : (Duration::count() <= prescalers[4] * value_t::maximum) ? std::make_tuple(prescalers[4], clksel[4])
               : (Duration::count() <= prescalers[5] * value_t::maximum) ? std::make_tuple(prescalers[5], clksel[5])
               : (Duration::count() <= prescalers[6] * value_t::maximum) ? std::make_tuple(prescalers[6], clksel[6])
               : (Duration::count() <= prescalers[7] * value_t::maximum) ? std::make_tuple(prescalers[7], clksel[7])
               : std::make_tuple(prescalers[0], clksel[0]); // Fallback (shouldn't happen with valid MaxTicks)
         }

         // Hold the 3 possible compare reactor handle
         template <typename... H>
         static constexpr void react_on_compare(H... reactor_handles) {
            static_assert(sizeof...(H) <= 3, "Error: Too many handles, maximum is 3.");

            // Helper lambda to set each compare value (CMP0, CMP1, CMP2)
            auto set_rh = [](const int cmp_index, reactor::Handle h) {
               if (cmp_index==0) {
                  on_timera_compare0 = h;
                  TCA().INTCTRL |= TCA_SINGLE_CMP0_bm;
                  clear_masks.append(h);
               } else if (cmp_index==1) {
                  on_timera_compare1 = h;
                  TCA().INTCTRL |= TCA_SINGLE_CMP1_bm;
                  clear_masks.append(h);
               } else {
                  on_timera_compare2 = h;
                  TCA().INTCTRL |= TCA_SINGLE_CMP2_bm;
                  clear_masks.append(h);
               }
            };

            auto indices = 0;
            (set_rh(indices++, reactor_handles), ...);
         }

         static constexpr void react_on_overflow(reactor::Handle h) {
            on_timera_ovf = h;
            TCA().INTCTRL |= TCA_SINGLE_OVF_bm;
            clear_masks.append(h);
         }

         // Variadic template function to set multiple compare registers
         template <typename... Durations>
         static constexpr void set_compare(Durations... compare_values) {
            static_assert(sizeof...(compare_values) <= 3, "Error: Too many compare values, maximum is 3.");

            // Ensure each duration is less than or equal to MaxDuration
            //(static_assert(compare_values <= max_duration, "Error: Compare value exceeds max timer duration"), ...);

            // Helper lambda to set each compare value (CMP0, CMP1, CMP2)
            auto set_cmp = [&](int cmp_index, asx::chrono::cpu_tick_t cmp_value) {
               (&(TCA().CMP0))[cmp_index] = cmp_value.count() / std::get<0>(set_prescaler_for_maximum_ticks());
            };

            auto indices = 0;
            (set_cmp(indices++, compare_values), ...);
         }

         /**
          * Start the timer
          * Any pending reactor actions are cleared
          */
         static void start() {
            // Stop the timer so we don't try to aim at a moving target
            TCA().CTRLA &= ~TCA_SINGLE_ENABLE_bm;

            //
            // No interrupts will fire from this point on
            //

            // Clear any pending interrupts
            TCA().INTFLAGS =
               TCA_SINGLE_OVF_bm | TCA_SINGLE_CMP0_bm | TCA_SINGLE_CMP1_bm | TCA_SINGLE_CMP2_bm;

            // Clear the reactor flags - so no callback pass this point
            reactor::clear(clear_masks);

            // Restart the timer
            TCA().CTRLESET = TCA_SINGLE_CMD_RESTART_gc;

            // Restart the timer
            TCA().CTRLA |= TCA_SINGLE_ENABLE_bm;
         }

         static void stop() {
            TCA().CTRLA &= ~TCA_SINGLE_ENABLE_bm;
         }

         static void init( uint8_t flags ) {
            // Update the way you access prescaler and clk_setting
            auto prescaler = set_prescaler_for_maximum_ticks();

            // Copy the flags
            timera_config_flag = flags;

            TCA().CNT = 0;
            TCA().PER = Duration::count() / std::get<0>(prescaler);
            TCA().CTRLA = std::get<1>(prescaler);
            TCA().CTRLB = 0; // Normal mode
         }
      };

      /**
       * Specialized instance of the timer B
       */
      template<int N>
      class TimerB
      {
         using type = TCB_t;  // Specify the timer type as TCB_t
         using value_t = Counting16;

         static_assert(N < 2, "Invalid timer number");

         static TCB_t * const get_timer() {
            if constexpr (N == 0) {
               return &TCB0;
            }

            return &TCB1;
         }

         static void react_on_cmp( reactor::Handle reactor ) {
            on_timerb_compare = reactor;
            // Enable the interrupt
            get_timer()->CTRLA |= TCB_ENABLE_bm;
         }

         // Variadic template function to set multiple compare registers
         template <typename Duration>
         constexpr void set_compare(const Duration& duration) {
            // Convert the given duration to cpu_tick_t based on F_CPU
            constexpr asx::chrono::cpu_tick_t compare_value = std::chrono::duration_cast<asx::chrono::cpu_tick_t>(duration);

            static_assert( compare_value.count() < (value_t::maximum * 2), "Number of ticks is too big" );

            // Set the timer prescaler
            auto* timer = get_timer();
            timer->CNT = 0;  // Reset the counter

            if (compare_value.count() < value_t::maximum) {
               timer->CTRLA = TCB_CLKSEL_DIV1_gc;
               timer->CCMP = compare_value.count();
            } else {
               timer->CTRLA = TCB_CLKSEL_DIV2_gc;
               timer->CCMP = compare_value.count() >> 1;
            }
         }

         // Use statically only (there is only 1 timerA)
         TimerB() = delete;
      };
   };
}
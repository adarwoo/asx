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

#include "conf_board.h"
#include <asx/modbus_rtu.hpp>


namespace asx {
   namespace modbus {

      using namespace asx::ioport;

      template<class Datagram, class Uart, class T = StaticTiming<Uart>>
      class Master {
         using Self = Master;

         // Create a 4xT or 2ms timer - whichever is the longest
         using Timer = asx::hw_timer::TimerA<T>;

         // Keep a mask for all pending transmit requests
         inline static auto pending_transmits      = reactor::Mask{0};

         // Reactor for errors
         inline static auto react_on_error         = reactor::Handle{};

         // Reply timeout
         inline static auto react_on_reply_timeout = reactor::Handle{};

         // Initiate a transmit
         inline static auto react_on_ready_to_send = reactor::Handle{};

         // Process the received character
         inline static auto react_on_char_received = reactor::Handle{};

         // Store the timeout timer to cancel it
         inline static auto timeout_timer          = asx::timer::Instance{};

         struct StateMachine {
            // Internal SM
            auto operator()() {
               using namespace boost::sml;

               auto start_timers   = [] {
                  Timer::start();
               };

               auto process_reply = [] {
                  auto reply = Datagram::process_reply();

                  if ( reply != error_t::ok ) {
                     react_on_error.notify(Datagram::get_buffer()[0], reply);
                  }
               };

               auto stop_timeout  = [] {
                  timeout_timer.cancel();
               };

               auto timeout_error = [] {
                  react_on_error.notify(Datagram::get_buffer()[0], error_t::reply_timeout);
               };

               auto frame_error   = [] {
                  react_on_error.notify(Datagram::get_buffer()[0], error_t::frame_error);
               };

               /** The frame just got sent - arm the timeout timer */
               auto wait_for_reply= [] {
                  using namespace std::chrono;

                  // Ready the datagram
                  Datagram::reset();

                  // The slave must reply without the timeout
                  timeout_timer = react_on_reply_timeout.delay(T::timeout());
               };

               auto handle_char   = [] (const auto& event) {
                  Timer::start(); // Restart the timers (15/35/40)
                  Datagram::process_char(event.c);
               };

               auto insert_pending_transmit = [](const auto& event, auto& sm, auto& deps, auto& subs) {
                  auto next = pending_transmits.pop();

                  if ( next != reactor::null ) {
                     Datagram::reset();

                     next.invoke(); // Call directly

                     // Add the CRC
                     Datagram::ready_request();

                     // Transition to ready_to_send to insert the frame and turn off echo
                     sm.process_event(rts{}, deps, subs);
                  }
               };

               auto send = []() {
                  Uart::disable_rx();
                  Uart::send(Datagram::get_buffer());
               };

               return make_transition_table(
                  * "cold"_s                + event<can_start>                     = "initial"_s
                  , "initial"_s             + on_entry<_>          / start_timers
                  , "initial"_s             + event<t35_timeout>                   = "idle"_s
                  , "initial"_s             + event<char_received> / start_timers
                  , "idle"_s                + on_entry<_>          / insert_pending_transmit
                  , "idle"_s                + event<check_pendings>/ insert_pending_transmit
                  , "idle"_s                + event<rts>           / send          = "sending"_s
                  , "idle"_s                + event<char_received>                 = "initial"_s
                  , "sending"_s             + event<frame_sent>    / wait_for_reply= "waiting_for_reply"_s
                  , "waiting_for_reply"_s   + event<reply_timeout> / timeout_error = "idle"_s
                  , "waiting_for_reply"_s   + event<char_received> / handle_char   = "reception"_s
                  , "reception"_s           + on_entry<_>          / stop_timeout
                  , "reception"_s           + event<char_received> / handle_char
                  , "reception"_s           + event<t15_timeout>                   = "control_and_waiting"_s
                  , "control_and_waiting"_s + event<char_received> / frame_error   = "idle"_s
                  , "control_and_waiting"_s + event<t35_timeout>   / process_reply = "prevent_race"_s
                  , "prevent_race"_s        + event<t40_timeout>                   = "idle"_s
               );
            }
         };

      #ifdef DEBUG
         inline static Logging logger;
         inline static boost::sml::sm<StateMachine, boost::sml::logger<Logging>> sm{logger};
      #else
         ///< The overall modbus state machine
         inline static auto sm = boost::sml::sm<StateMachine>{};
      #endif // DEBUG

         static void on_char_received(uint8_t c) {
            sm.process_event(char_received{c});
         }

         static void on_timeout() {
            sm.process_event(reply_timeout{});
         }

      public:
         static void init(asx::reactor::Handle error_reactor = asx::reactor::Handle{}) {
            Timer::init(hw_timer::single_use);
            Uart::init();

            // Set the compare for T15 and T35
            Timer::set_compare(T::t15(), T::t35());

            // Add reactor handler
            Timer::react_on_compare(
               reactor::bind(on_timeout_t15),
               reactor::bind(on_timeout_t35)
            );

            // Add reactor handler for the big timeout
            Timer::react_on_overflow(
               reactor::bind(on_timeout_t40)
            );

            // Add reactor handler for the Uart
            Uart::react_on_character_received(
               reactor::bind(on_char_received, reactor::prio::high)
            );

            // Add a reactor handler for when the transmit is complete
            Uart::react_on_send_complete(
               reactor::bind(
                  []() {
                     Uart::enable_rx(); // Ready to receive again
                     sm.process_event(frame_sent{});
                  },
                  reactor::prio::high
               )
            );

            // React on timeout
            react_on_reply_timeout = reactor::bind(on_timeout);

            // Plug the timeout handler
            react_on_error = error_reactor;

            // Start the SM
            sm.process_event(can_start{});
         }

         static void on_timeout_t15() {
            sm.process_event(t15_timeout{});
         }

         static void on_timeout_t35() {
            sm.process_event(t35_timeout{});
         }

         static void on_timeout_t40() {
            sm.process_event(t40_timeout{});
         }

         /// @brief Make a request to send a RTU frame as master. The request is added to other request
         ///        in a reactor mask. Once the bus is available, the reactor will be called.
         ///        If many request are pending, the highest priority reactor is served first
         /// @param h
         static void request_to_send(reactor::Handle h) {
            pending_transmits.append(h);
            sm.process_event(check_pendings{});
         }

         /// @return The pending request mask
         static reactor::Mask get_pending_request() {
            return pending_transmits;
         }
      };

   } // namespace modbus
} // end of namespace asx

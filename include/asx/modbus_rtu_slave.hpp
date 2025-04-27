#pragma once

#include <asx/modbus_rtu.hpp>

namespace asx {
   namespace modbus {

      template<class Datagram, class Uart, class T = StaticTiming<Uart>>
      class Slave {
         using Self = Slave;

         // Create a 4xT or 2ms timer - whichever is the longest
         using Timer = asx::hw_timer::TimerA<T>;

         inline static const auto must_reply = [](const t35_timeout&) {
            bool retval = false;

            switch ( Datagram::get_status() ) {
               case Datagram::status_t::NOT_FOR_ME:
                  break;
               case Datagram::status_t::BAD_CRC:
                  break;
               case Datagram::status_t::GOOD_FRAME:
                  return true;
               default:
                  break;
            }

            return retval;
         };

         inline static const auto broadcast = []() {
            return Datagram::get_buffer()[0] == 0;
         };

         struct StateMachine {
            // Internal SM
            auto operator()() {
               using namespace boost::sml;

               auto start_timer = [] () { Timer::start(); };
               auto reset       = [] () { Datagram::reset(); };
               auto ready_reply = [] () { Datagram::ready_reply(); };
               auto listen      = [] () { Uart::enable_rx(); }; // Ready to receive again

               auto reply       = [] () {
                  Uart::disable_rx();
                  Uart::send(Datagram::get_buffer());
               };

               auto handle_char = [] (const auto& event) {
                  Timer::start(); // Restart the timers (15/35/40)
                  Datagram::process_char(event.c);
               };

               return make_transition_table(
               * "cold"_s                + event<can_start>                              = "initial"_s
               , "initial"_s             + on_entry<_>                     / start_timer
               , "initial"_s             + event<t35_timeout>                            = "idle"_s
               , "initial"_s             + event<char_received>            / start_timer = "initial"_s
               , "idle"_s                + on_entry<_>                     / reset
               , "idle"_s                + event<char_received>            / handle_char = "reception"_s
               , "reception"_s           + event<t15_timeout>                            = "control_and_waiting"_s
               , "reception"_s           + event<char_received>            / handle_char = "reception"_s
               , "control_and_waiting"_s + event<t35_timeout> [must_reply]               = "reply"_s
               , "control_and_waiting"_s + event<char_received>                          = "initial"_s
               , "control_and_waiting"_s + event<t35_timeout>                            = "idle"_s
               , "reply"_s               + on_entry<_>                     / ready_reply
               , "reply"_s               + event<char_received>                          = "initial"_s // Unlikely - but a possibility
               , "reply"_s               + event<t40_timeout> [broadcast]                = "idle"_s    // In broadcast - no answer
               , "reply"_s               + event<t40_timeout>                            = "emission"_s
               , "emission"_s            + on_entry<_>                     / reply
               , "emission"_s            + event<frame_sent>               / listen      = "initial"_s
               );
            }
         };

      #ifdef SIM
         inline static Logging logger;
         inline static auto sm = boost::sml::sm<StateMachine, boost::sml::logger<Logging>>{logger};
      #else
         ///< The overall modbus state machine
         inline static auto sm = boost::sml::sm<StateMachine>{};
      #endif

      public:
         static void init() {
            Timer::init(hw_timer::single_use);
            Uart::init();

            // Set the compare for T15 and T35
            Timer::set_compare(T::t15(), T::t35());

            // Add reactor handler for the Uart (first - so it takes priority)
            Uart::react_on_character_received(reactor::bind(on_rx_char, reactor::high));

            // Add reactor handler
            Timer::react_on_compare(
               reactor::bind(on_timeout_t15, reactor::high),
               reactor::bind(on_timeout_t35, reactor::high)
            );

            Timer::react_on_overflow(reactor::bind(on_timeout_t40, reactor::low));

            // Add a reactor handler for when the transmit is complete
            Uart::react_on_send_complete(reactor::bind(on_send_complete, reactor::high));

            // Start the SM
            sm.process_event(can_start{});
         }

         static void on_rx_char(uint8_t c) {
            sm.process_event(char_received{c});
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

         static void on_send_complete() {
            sm.process_event(frame_sent{});
         }
      };
   } // namespace modbus
} // end of namespace asx
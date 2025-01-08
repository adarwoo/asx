#pragma once

#ifdef SIM
#include <cstdio>
#endif

#include <logger.h>
#include <string_view>

#include <boost/sml.hpp>

#include <asx/chrono.hpp>
#include <asx/reactor.hpp>
#include <asx/hw_timer.hpp>


namespace asx {
   namespace modbus {
      enum class error_t : uint8_t {
         ok = 0,
         illegal_function_code = 0x01, // Modbus standard for illegal function code
         illegal_data_address = 0x02,
         illegal_data_value = 0x03,
         slave_device_failure = 0x04,
         acknowledge = 0x05,
         slave_device_busy = 0x06,
         negative_acknowledge = 0x07,
         memory_parity_error = 0x08,
         ignore_frame = 255
      };

      class Crc {
         /// @brief Number of bytes received. Modbus limits to 256 bytes.
         uint8_t count;
         ///< The CRC for the currently received frame
         uint16_t crc;

         /// @brief Buffer of the last 2 bytes so they are not processed
         uint8_t n_minus_1;
         uint8_t n_minus_2;

      public:
         Crc();
         void reset();
         void operator()(uint8_t byte);
         void update(uint8_t byte);
         bool check();
         uint16_t update(std::string_view view);
      };

      struct can_start_receiving {};
      struct t15_timeout {};
      struct t35_timeout {};
      struct t40_timeout {};
      struct demand_of_emission {};
      struct char_received { uint8_t c{}; };
      struct frame_sent {};

      template<class Uart>
      struct StaticTiming {
         // Helper
         static auto _ticks(float multiplier, const int ms) {
            auto actual = Uart::get_byte_duration(multiplier);
            auto upto = std::chrono::duration_cast<asx::chrono::cpu_tick_t>(
               std::chrono::microseconds(ms)
            );
            return std::max(actual, upto);
         };

         static auto count() {
            return _ticks(4.0, 2000).count();
         }

         static auto t15() {
            return _ticks(1.5, 750);
         }

         static auto t35() {
            return _ticks(3.5, 1750);
         }
      };

      template<class Datagram, class Uart, class T = StaticTiming<Uart>>
      class Slave {
         using Self = Slave;

         // Create a 4xT or 2ms timer - whichever is the longest
         using Timer = asx::hw_timer::TimerA<T>;

         inline static const auto must_reply = [](const t35_timeout&) {
            bool retval = false;

            switch ( Datagram::get_status() ) {
               case Datagram::status_t::NOT_FOR_ME:
                  LOG_INFO("DGRAM", "Frame is not for me");
                  break;
               case Datagram::status_t::BAD_CRC:
                  LOG_WARN("DGRAM", "Bad CRC");
                  break;
               case Datagram::status_t::GOOD_FRAME:
                  LOG_WARN("DGRAM", "Good Frame received");
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
               auto reply       = [] () { Uart::send(Datagram::get_buffer()); };

               auto handle_char = [] (const auto& event) {
                  Timer::start(); // Restart the timers (15/35/40)
                  Datagram::process_char(event.c);
               };

               return make_transition_table(
               * "cold"_s                + event<can_start_receiving>                    = "initial"_s
               , "initial"_s             + on_entry<_>                     / start_timer
               , "initial"_s             + event<t35_timeout>                            = "idle"_s
               , "initial"_s             + event<char_received>            / start_timer = "initial"_s
               , "idle"_s                + on_entry<_>                     / reset
               , "idle"_s                + event<char_received>            / handle_char = "reception"_s
               , "idle"_s                + event<demand_of_emission>                     = "emission"_s
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
               , "emission"_s            + event<frame_sent>                             = "initial"_s
               );
            }
         };

#ifdef SIM
         struct Logging {
            template <class SM, class TEvent>
            void log_process_event(const TEvent&) {
               LOG_INFO("SM", "[process_event] %s", boost::sml::aux::get_type_name<TEvent>());
            }

            template <class SM, class TGuard, class TEvent>
            void log_guard(const TGuard&, const TEvent&, bool result) {
               LOG_INFO("SM", "[guard] %s %s %s", boost::sml::aux::get_type_name<TGuard>(),
                     boost::sml::aux::get_type_name<TEvent>(), (result ? "[OK]" : "[Reject]"));
            }

            template <class SM, class TAction, class TEvent>
            void log_action(const TAction&, const TEvent&) {
               LOG_INFO("SM", "[action] %s %s", boost::sml::aux::get_type_name<TAction>(),
                     boost::sml::aux::get_type_name<TEvent>());
            }

            template <class SM, class TSrcState, class TDstState>
            void log_state_change(const TSrcState& src, const TDstState& dst) {
               LOG_INFO("SM", "[transition] %s -> %s", src.c_str(), dst.c_str());
            }
         };

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

            // Add reactor handler
            Timer::react_on_compare(
               reactor::bind(on_timeout_t15),
               reactor::bind(on_timeout_t35)
            );

            Timer::react_on_overflow(reactor::bind(on_timeout_t40));

            // Add reactor handler for the Uart
            Uart::react_on_character_received(reactor::bind(on_rx_char));

            // Add a reactor handler for when the transmit is complete
            Uart::react_on_send_complete(reactor::bind(on_send_complete));

            // Start the SM
            sm.process_event(can_start_receiving{});
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
#pragma once

#ifdef SIM
#include <cstdio>
#endif

#include <trace.h>
#include <string_view>

#include <boost/sml.hpp>

#include <asx/chrono.hpp>
#include <asx/reactor.hpp>
#include <asx/hw_timer.hpp>


namespace asx {
   namespace modbus {
      enum class command_t : uint8_t {
         read_coils = 1,
         read_discrete_inputs = 2,
         read_holding_registers = 3,
         read_input_registers = 4,
         write_single_coil = 5,
         write_single_register = 6,
         write_multiple_coils = 15,
         write_multiple_registers = 16,
         read_write_multiple_registers = 23,
         custom = 101
      };

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

      struct can_start {
         constexpr const char *c_str() const  { return "can_start"; }
      };

      struct t15_timeout {
         constexpr const char *c_str() const { return "t15_timeout"; }
      };
      struct t35_timeout {
         constexpr const char *c_str() const { return "t35_timeout"; }
      };
      struct t40_timeout {
         constexpr const char *c_str() const { return "t40_timeout"; }
      };
      struct reply_timeout {
         constexpr const char *c_str() const { return "reply_timeout"; }
      };
      struct rts  {
         constexpr const char *c_str() const { return "rts"; }
      };
      struct char_received {
          uint8_t c{};
         constexpr const char *c_str() const { return "char_received"; }
      };
      struct frame_sent  {
         constexpr const char *c_str() const { return "frame_sent"; }
      };
      struct check_pendings {
         constexpr const char *c_str() const { return "check_pendings"; }
      };


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


      struct Logging {
         template <class SM, class TEvent>
         void log_process_event(const TEvent& evt) {
            trace("[evt] %s", (char*)evt.c_str());
         }

         template <class SM, class TGuard, class TEvent>
         void log_guard(const TGuard&, const TEvent&, bool result) {
            trace("[grd]");
         }

         template <class SM, class TAction, class TEvent>
         void log_action(const TAction& act, const TEvent& evt) {
            trace("[act]");
         }

         template <class SM, class TSrcState, class TDstState>
         void log_state_change(const TSrcState& src, const TDstState& dst) {
            trace("[>] %s -> %s", src.c_str(), dst.c_str());
         }
      };


      template<class Datagram, class Uart, class T = StaticTiming<Uart>>
      class Master {
         using Self = Master;

         // Create a 4xT or 2ms timer - whichever is the longest
         using Timer = asx::hw_timer::TimerA<T>;

         // Keep a mask for all pending transmit requests
         inline static auto pending_transmits = reactor::Mask{0};

         // Reactor for errors
         inline static auto react_on_error = reactor::Handle{};

         // Reply timeout
         inline static auto react_on_reply_timeout = reactor::Handle{};

         // React to transmit a payload - low prio - will always run after senders
         inline static auto react_on_transmit = reactor::Handle{};

         // Store the timeout timer to cancel it
         inline static auto timeout_timer = asx::timer::Instance{};

         struct StateMachine {
            // Internal SM
            auto operator()() {
               using namespace boost::sml;

               auto start_timer   = [] () { Timer::start(); };
               auto process_reply = [] () { timeout_timer.cancel(); Datagram::process_reply(); };
               auto transmit      = [] () { Uart::send(Datagram::get_buffer()); };
               auto timeout_error = [] () { react_on_error(); };

               auto wait_for_reply= [] () {
                  using namespace std::chrono;
                  timeout_timer = react_on_reply_timeout.delay(10ms);
               };

               auto handle_char   = [] (const auto& event) {
                  Timer::start(); // Restart the timers (15/35/40)
                  Datagram::process_char(event.c);
               };

               auto insert_pending_transmit = [] {
                  Datagram::reset();

                  auto next = pending_transmits.pop();

                  if ( next != reactor::null ) {
                     reactor_notify(next, nullptr);

                     // Also - arm the transmit on the SM which will be called AFTER since it
                     // is a low prio (guaranteed)
                     react_on_transmit();
                  }
               };

               return make_transition_table(
               * "cold"_s                + event<can_start>                     = "initial"_s
               , "initial"_s             + on_entry<_>          / start_timer
               , "initial"_s             + event<t35_timeout>                   = "idle"_s
               , "initial"_s             + event<char_received> / start_timer
               , "idle"_s                + on_entry<_>          / insert_pending_transmit
               , "idle"_s                + event<check_pendings>/ insert_pending_transmit
               , "idle"_s                + event<rts>           / transmit      = "sending"_s
               , "idle"_s                + event<char_received>                 = "initial"_s
               , "sending"_s             + event<frame_sent>    / wait_for_reply= "waiting_for_reply"_s
               , "waiting_for_reply"_s   + event<char_received> / handle_char   = "reception"_s
               , "waiting_for_reply"_s   + event<reply_timeout> / timeout_error = "idle"_s
               , "reception"_s           + event<t15_timeout>                   = "control_and_waiting"_s
               , "reception"_s           + event<char_received> / handle_char   = "reception"_s
               , "control_and_waiting"_s + event<t35_timeout>   / process_reply = "prevent_race"_s
               , "prevent_race"_s        + event<t40_timeout>                   = "idle"_s
               );
            }
         };


#ifdef DEBUG
         inline static Logging logger;
         inline static auto sm = boost::sml::sm<StateMachine, boost::sml::logger<Logging>>{logger};
#else
         ///< The overall modbus state machine
         inline static auto sm = boost::sml::sm<StateMachine>{};
#endif // DEBUG

      ///< Called to transmit the datagram buffer
      static void on_transmit() {
         sm.process_event(rts{});
      }

      static void on_send_complete() {
         sm.process_event(frame_sent{});
      }

      static void on_rx_char(uint8_t c) {
         sm.process_event(char_received{c});
      }

      static void on_reply_timeout() {
         sm.process_event(reply_timeout{});
      }

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

            Timer::react_on_overflow(reactor::bind(on_timeout_t40, reactor::low));

            // Add reactor handler for the Uart
            Uart::react_on_character_received(reactor::bind(on_rx_char));

            // Add a reactor handler for when the transmit is complete
            Uart::react_on_send_complete(reactor::bind(on_send_complete));

            // React on timeout
            react_on_reply_timeout = reactor::bind(on_reply_timeout);

            // React on transmit
            // The prio must be low so the transmit are called first
            react_on_transmit = reactor::bind(on_transmit, reactor::prio::low);

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
            trace("Pend %x", static_cast<uint32_t>(pending_transmits));
            sm.process_event(check_pendings{});
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
                  trace("Frame is not for me");
                  break;
               case Datagram::status_t::BAD_CRC:
                  trace("Bad CRC");
                  break;
               case Datagram::status_t::GOOD_FRAME:
                  trace("Good Frame received");
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
               , "emission"_s            + event<frame_sent>                             = "initial"_s
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
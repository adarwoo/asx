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

#ifdef SIM
#include <cstdio>
#endif

#include <string_view>

#include <boost/sml.hpp>

#include <asx/chrono.hpp>
#include <asx/reactor.hpp>
#include <asx/hw_timer.hpp>


namespace asx {
   namespace modbus {
      enum class command_t : uint8_t {
         read_coils                     = 0x01,
         read_discrete_inputs           = 0x02,
         read_holding_registers         = 0x03,
         read_input_registers           = 0x04,
         write_single_coil              = 0x05,
         write_single_register          = 0x06,
         write_multiple_coils           = 0x0F,
         write_multiple_registers       = 0x10,
         read_write_multiple_registers  = 0x17,
         custom                         = 0x65
      };

      enum class error_t : uint8_t {
         ok = 0,
         illegal_function_code = 0x01, // Modbus standard for illegal function code
         illegal_data_address  = 0x02,
         illegal_data_value    = 0x03,
         slave_device_failure  = 0x04,
         acknowledge           = 0x05,
         slave_device_busy     = 0x06,
         negative_acknowledge  = 0x07,
         memory_parity_error   = 0x08,
         unknown_error         = 0x09,
         // Non modbus standard codes
         comm_errors           = 0xF0, ///< Garbled response detected
         reply_timeout         = 0xF1, ///< No reply received in time
         frame_error           = 0xF2, ///< Detecting gap in the frame
         bad_crc               = 0xF3, ///< Bad Frame CRC
         ignore_frame          = 0xFF  ///< Frame not intented for us
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
         constexpr const char *c_str() const { return "t15"; }
      };
      struct t35_timeout {
         constexpr const char *c_str() const { return "t35"; }
      };
      struct t40_timeout {
         constexpr const char *c_str() const { return "t40"; }
      };
      struct reply_timeout {
         constexpr const char *c_str() const { return "reply_timeout"; }
      };
      struct rts  {
         constexpr const char *c_str() const { return "rts"; }
      };
      struct char_received {
         uint8_t c{};

         const char *c_str() const {
            static char desc[] = "char_rxd=0x..";
            utoa(c, desc+11, 16);
            return desc;
         }
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

         static auto timeout() {
            return std::chrono::milliseconds(100);
         }
      };

      struct Logging {
         template <class SM, class TEvent>
         void log_process_event(const TEvent& evt) {
            ULOG_INFO("[event]", evt.c_str());
         }

         template <class SM, class TGuard, class TEvent>
         void log_guard(const TGuard&, const TEvent&, bool result) {
            ULOG_INFO("[guard] - yeilds:", result);
         }

         template <class SM, class TAction, class TEvent>
         void log_action(const TAction& act, const TEvent& evt) {
            ULOG_INFO("[action]");
         }

         template <class SM, class TSrcState, class TDstState>
         void log_state_change(const TSrcState& src, const TDstState& dst) {
            ULOG_INFO("[state transition to] :", dst.c_str());
         }
      };
   } // namespace modbus
} // end of namespace asx

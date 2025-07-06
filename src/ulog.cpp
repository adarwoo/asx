#include <avr/interrupt.h>

#include <span>

#include <asx/uart.hpp>
#include <asx/ulog.hpp>

namespace {
   // ----------------------------------------------------------------------
   // Constants
   // ----------------------------------------------------------------------

   // Circular buffer (stubbed here)
   constexpr int MAX_PAYLOAD = 4;
   constexpr int BUF_SIZE = 16;

   // ----------------------------------------------------------------------
   // Local types
   // ----------------------------------------------------------------------
   struct LogPacket {
      uint8_t data[1 + MAX_PAYLOAD]; // The first is the ID
      uint8_t len;
   };

   // ----------------------------------------------------------------------
   // Private data
   // ----------------------------------------------------------------------
   volatile uint8_t log_head = 0, log_tail = 0;
   LogPacket log_buffer[BUF_SIZE];

   // ----------------------------------------------------------------------
   // Internal functions
   // ----------------------------------------------------------------------
   LogPacket *reserve_log_packet() {
      LogPacket *retval = nullptr;
      uint8_t next = (log_head + 1) % BUF_SIZE;

      if (next != log_tail) {
         retval = &log_buffer[log_head];
         log_head = next;
      }      
      
      return retval;
   }
}

// ----------------------------------------------------------------------
// C Linkage functions called from the inline assembly
// ----------------------------------------------------------------------

extern "C" void ulog_detail_emit0(uint8_t id) {
   if (LogPacket* dst = reserve_log_packet()) {
      dst->data[0] = id;
      dst->len = 0;
   }
}

extern "C" void ulog_detail_emit8(uint8_t id, uint8_t v0) {
   if (LogPacket* dst = reserve_log_packet()) {
      dst->data[0] = id;
      dst->data[1] = v0;
      dst->len = 1;
   }
}

extern "C" void ulog_detail_emit16(uint8_t id, uint16_t v) {
   if (LogPacket* dst = reserve_log_packet()) {
      dst->data[0] = id;
      dst->data[1] = v & 0xFF;
      dst->data[2] = v >> 8;
      dst->len = 2;
   }
}

extern "C" void ulog_detail_emit32(uint8_t id, uint32_t v) {
   if (LogPacket* dst = reserve_log_packet()) {
      dst->data[0] = id;
      dst->data[1] = v & 0xFF;
      dst->data[2] = (v >> 8) & 0xFF;
      dst->data[3] = (v >> 16) & 0xFF;
      dst->data[4] = (v >> 24) & 0xFF;
      dst->len = 4;
   }
}

// ----------------------------------------------------------------------
// Tranmission of the data
// ----------------------------------------------------------------------

namespace asx {
   namespace ulog {
      using uart = uart::Uart<
         0,
         uart::CompileTimeConfig<115200, uart::width::_8, uart::parity::none, uart::stop::_1>
      >;

      // Scratch buffer for encoded output (COBS adds +2 overhead worse case)
      static uint8_t tx_encoded[sizeof(LogPacket::data) + 2];
      static std::span<const uint8_t> to_send;

      // COBS encoder: encodes input into output, returns encoded length
      static uint8_t cobs_encode(const uint8_t* in, uint8_t len, uint8_t* out) {
         uint8_t* start = out++;
         uint8_t code = 1;
         uint8_t* code_ptr = start;

         for (uint8_t i = 0; i < len; ++i) {
            if (in[i] == 0) {
               *code_ptr = code;
               code_ptr = out++;
               code = 1;
            } else {
               *out++ = in[i];
               ++code;
               if (code == 0xFF) {
                  *code_ptr = code;
                  code_ptr = out++;
                  code = 1;
               }
            }
         }

         *code_ptr = code;
         *out++ = 0; // COBS terminator

         return static_cast<uint8_t>(out - start);
      }

      // Start transmission if idle
      static void start_tx_if_needed() {
         if (!to_send.empty()) return;

         if (log_tail == log_head) return; // No data

         LogPacket& pkt = log_buffer[log_tail];
         log_tail = (log_tail + 1) % BUF_SIZE;

         uint8_t encoded_len = cobs_encode(pkt.data, pkt.len, tx_encoded);
         to_send = std::span<const uint8_t>(tx_encoded, encoded_len);

         // Enable DRE interrupt
         uart::get().CTRLA |= USART_DREIE_bm;
      }

      // Overrides the reactor idle tasklet to re-start the output of log
      extern "C" void reactor_idle_tasklet(void) {
         start_tx_if_needed();
      }

      // Self initialise early on
      void __attribute__ ((section (".init0"), naked, used))
      init() {
         uart::init();
         uart::disable_rx();
         uart::get().CTRLA = 0; // No interrupt at this stage
      }

      // There is space in the buffer
      ISR(USART0_DRE_vect) {
         if ( not to_send.empty() ) {
            uart::get().TXDATAL = to_send.front();
            to_send = to_send.subspan(1);
         } else {
            // Disable all interrupts
            uart::get().CTRLA = 0;
         }
      }
   }
}

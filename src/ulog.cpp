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
      uint8_t len;
      uint8_t data[1 + MAX_PAYLOAD];
   };

   // ----------------------------------------------------------------------
   // Private data
   // ----------------------------------------------------------------------
   volatile uint8_t log_head = 0, log_tail = 0;
   LogPacket log_buffer[BUF_SIZE];

   // ----------------------------------------------------------------------
   // Internal functions
   // ----------------------------------------------------------------------
   uint8_t *reserve_log_packet() {
      uint8_t next = (log_head + 1) % BUF_SIZE;

      if (next == log_tail)
         return nullptr;

      uint8_t *ptr = log_buffer[log_head].data;
      log_head = next;
      return ptr;
   }
}

// ----------------------------------------------------------------------
// C Linkage functions called from the inline assembly
// ----------------------------------------------------------------------

extern "C" void ulog_detail_emit0(uint8_t id) {
   if (uint8_t* dst = reserve_log_packet()) {
      *dst = id;
   }
}

extern "C" void ulog_detail_emit8(uint8_t id, uint8_t v0) {
   if (uint8_t* dst = reserve_log_packet()) {
      *dst++ = id;
      *dst = v0;
   }
}

extern "C" void ulog_detail_emit16(uint8_t id, uint16_t v) {
   if (uint8_t* dst = reserve_log_packet()) {
      *dst++ = id;
      *dst++ = v & 0xFF;
      *dst = (v >> 8) & 0xFF;
   }
}

extern "C" void ulog_detail_emit32(uint8_t id, uint32_t v) {
   if (uint8_t* dst = reserve_log_packet()) {
      *dst++ = id;
      *dst++ = v & 0xFF;
      *dst++ = (v >> 8) & 0xFF;
      *dst++ = (v >> 16) & 0xFF;
      *dst = (v >> 24) & 0xFF;
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

      // Scratch buffer for encoded output (COBS adds +1 overhead, plus terminator)
      static uint8_t tx_encoded[1 + sizeof(LogPacket::data) + 1];
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

         uint8_t len = pkt.len ? pkt.len : sizeof(pkt.data);
         uint8_t encoded_len = cobs_encode(pkt.data, len, tx_encoded);

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
            to_send.subspan(1);
         } else {
            // Disable all interrupts
            uart::get().CTRLA = 0;
         }
      }
   }
}

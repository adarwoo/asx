#include <interrupt.h>

#include <string_view>

#include <asx/uart.hpp>
#include <asx/ulog.hpp>
#include <asx/reactor.hpp>

namespace {
   // ----------------------------------------------------------------------
   // Constants
   // ----------------------------------------------------------------------

   // Circular buffer (stubbed here)
   constexpr int MAX_PAYLOAD = 4;
   constexpr int BUF_SIZE = 16;
   // COBS framing char
   constexpr auto EOF = uint8_t{0xA6};

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
   asx::reactor::Handle react_to_initiate_transmit = asx::reactor::null;

   // ----------------------------------------------------------------------
   // Internal functions
   // ----------------------------------------------------------------------
   LogPacket *reserve_log_packet() {
      LogPacket *retval = nullptr;

      // Disable interrupt - but save since this could be used from within an interrupt
      auto save_flags =  cpu_irq_save();

      uint8_t next = (log_head + 1) % BUF_SIZE;

      if (next != log_tail) {
         retval = &log_buffer[log_head];
         log_head = next;
      }

      // Since we've issued a slot - it will require sending
      asx::reactor::notify_from_isr(react_to_initiate_transmit);

      // Restore SREG
      cpu_irq_restore(save_flags);

      return retval;
   }
}

// ----------------------------------------------------------------------
// C Linkage functions called from the inline assembly
// ----------------------------------------------------------------------

extern "C" void ulog_detail_emit0(uint8_t id) {
   if (LogPacket* dst = reserve_log_packet()) {
      dst->data[0] = id;
      dst->len = 1;
   }
}

extern "C" void ulog_detail_emit8(uint8_t id, uint8_t v0) {
   if (LogPacket* dst = reserve_log_packet()) {
      dst->data[0] = id;
      dst->data[1] = v0;
      dst->len = 2;
   }
}

extern "C" void ulog_detail_emit16(uint8_t id, uint16_t v) {
   if (LogPacket* dst = reserve_log_packet()) {
      dst->data[0] = id;
      dst->data[1] = v & 0xFF;
      dst->data[2] = v >> 8;
      dst->len = 3;
   }
}

extern "C" void ulog_detail_emit24(uint8_t id, uint8_t v0, uint8_t v1, uint8_t v2) {
   if (LogPacket* dst = reserve_log_packet()) {
      dst->data[0] = id;
      dst->data[1] = v0;
      dst->data[2] = v1;
      dst->data[3] = v2;
      dst->len = 4;
   }
}

extern "C" void ulog_detail_emit32(uint8_t id, uint32_t v) {
   if (LogPacket* dst = reserve_log_packet()) {
      dst->data[0] = id;
      dst->data[1] = v & 0xFF;
      dst->data[2] = (v >> 8) & 0xFF;
      dst->data[3] = (v >> 16) & 0xFF;
      dst->data[4] = (v >> 24) & 0xFF;
      dst->len = 5;
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

      // Scratch buffer for encoded output. Worse case - The payload(COBS adds +2 overhead worse case)
      static uint8_t tx_encoded[sizeof(LogPacket::data) + 2];

      // COBS encoder: encodes input into output, returns encoded length
      static uint8_t cobs_encode(const uint8_t* input, uint8_t length) {
         uint8_t read_index = 0;
         uint8_t write_index = 1;
         uint8_t code_index = 0;
         uint8_t code = 1;

         while (read_index < length) {
            if (input[read_index] == EOF) {
               tx_encoded[code_index] = code;
               code_index = write_index++;
               code = 1;
            } else {
               tx_encoded[write_index++] = input[read_index];
               code++;
            }

            ++read_index;
         }

         tx_encoded[code_index] = code;
         tx_encoded[write_index++] = EOF; // end frame

         return write_index;
   }

      /**
       * Initiate transmission reactor handler
       * Invoked at every insertion and once a transmit is complete
       * Checks for pending data
       * If found, encode and initiates the transmission on the UART
       */
      static void start_tx_if_needed() {
         // Avoid race condition since an interrupt could be logging
         auto save_flags = cpu_irq_save();

         if (uart::tx_ready() and log_tail != log_head) {
            // Data to send
            LogPacket& pkt = log_buffer[log_tail];
            log_tail = (log_tail + 1) % BUF_SIZE;

            uint8_t encoded_len = cobs_encode(pkt.data, pkt.len);
            uart::send( std::string_view((char *)tx_encoded, encoded_len) );
         }

         // Restore SREG
         cpu_irq_restore(save_flags);
      }

      // Self initialise very early on to allow all to use
      void __attribute__((constructor))  // Ensure this runs before main
      __attribute__((used))  // Ensure the compiler does not optimize this out
      init() {
         uart::init();
         uart::disable_rx();
         uart::get().CTRLA = 0; // No interrupt at this stage

         // Register the reactor
         react_to_initiate_transmit = asx::reactor::bind(start_tx_if_needed);
         
         // Get it called when the Tx buffer is available
         uart::react_on_send_complete(react_to_initiate_transmit);
      }
   }
}

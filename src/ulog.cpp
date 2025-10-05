/**
 * @file ulog.cpp
 * @brief Ultra-lightweight logging framework for embedded systems.
 * @author software@arreckx.com
 */
#include <interrupt.h>

#include <string_view>

#include <asx/uart.hpp>
#include <asx/ulog.hpp>
#include <asx/reactor.hpp>

//
// Apply configuration defaults
//
#ifndef ULOG_QUEUE_SIZE
#  define ULOG_QUEUE_SIZE 16
#endif

namespace asx {
   namespace ulog {
      namespace detail {
         // ----------------------------------------------------------------------
         // Constants
         // ----------------------------------------------------------------------

         // COBS framing char
         constexpr auto EOF = uint8_t{0xA6};

         // Circular buffer (stubbed here)
         constexpr int MAX_PAYLOAD = 4;
         constexpr int QUEUE_DEPTH = ULOG_QUEUE_SIZE;

         // ----------------------------------------------------------------------
         // Private types
         // ----------------------------------------------------------------------
         struct LogPacket {
            // Length of the data in the buffer (including the id)
            uint8_t payload_len;

            union {
               struct {
                  // Id of the log message
                  uint8_t id;
                  // Data to send over
                  uint8_t data[MAX_PAYLOAD];
               };
               uint8_t payload[1 + MAX_PAYLOAD];
            };
         };

         // UART type
         using uart = uart::Uart<0,
            uart::CompileTimeConfig<
               115200,
               uart::width::_8,
               uart::parity::none,
               uart::stop::_1>
         >;

         // ----------------------------------------------------------------------
         // Private data
         // ----------------------------------------------------------------------

         // Circular buffer pointers
         uint8_t log_head = 0, log_tail = 0;

         // Circular buffer itself
         LogPacket logs_circular_buffer[QUEUE_DEPTH];

         // Scratch buffer for encoded output. Worse case
         // The payload(COBS adds +2 overhead worse case)
         static uint8_t tx_encoded[sizeof(LogPacket::data) + 2];

         // Reactor handle to check for pending transmission
         auto react_to_initiate_transmit = asx::reactor::Handle();

         // ----------------------------------------------------------------------
         // Internal functions
         // ----------------------------------------------------------------------

         /**
          * Reserve a log packet in the circular buffer
          * @return Pointer to the reserved packet, or nullptr if the buffer is full
          */
         LogPacket *reserve_log_packet() {
            LogPacket *retval = nullptr;

            // Disable interrupt - but save since this could be used from within an interrupt
            auto save_flags = cpu_irq_save();

            uint8_t next = (log_head + 1) % QUEUE_DEPTH;

            if (next != log_tail) {
               retval = &logs_circular_buffer[log_head];
               log_head = next;
            }

            // Since we've issued a slot - it will require sending
            asx::reactor::notify_from_isr(react_to_initiate_transmit);

            // Restore SREG
            cpu_irq_restore(save_flags);

            return retval;
         }

         /**
          * COBS encoder: encodes input into output, returns encoded length
          * @param input Input data
          * @param length Length of input data
          * @return Length of encoded data
          */
         uint8_t cobs_encode(const uint8_t* input, uint8_t length) {
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
         void on_transmit() {
            // Avoid race condition since an interrupt could be logging
            auto save_flags = cpu_irq_save();

            if (log_tail != log_head) {
               // Data to send
               LogPacket& pkt = logs_circular_buffer[log_tail];
               log_tail = (log_tail + 1) % QUEUE_DEPTH;

               uint8_t encoded_len = cobs_encode(pkt.payload, pkt.payload_len);
               uart::send(std::span<const uint8_t>(tx_encoded, encoded_len));
            }

            // Restore SREG
            cpu_irq_restore(save_flags);
         }

         /**
          * Initialize the ULOG system
          * The logging can start before this is called, but no data will be sent
          */
         __attribute__((constructor()))
         void init() {
            uart::init();
            uart::disable_rx();
            uart::get().CTRLA = 0; // No interrupt at this stage

            // Register a reactor for filling the buffer
            react_to_initiate_transmit = asx::reactor::bind(on_transmit);

            // When the uart is done sending a frame, we can send the next one
            uart::react_on_send_complete(react_to_initiate_transmit);
         }
      } // namespace detail
   } // namespace ulog
} // namespace asx

// ----------------------------------------------------------------------
// C Linkage functions called from the inline assembly
// ----------------------------------------------------------------------

using namespace asx::ulog::detail;

extern "C" void ulog_detail_enqueue(uint8_t id) {
   if (LogPacket* dst = reserve_log_packet()) {
      dst->id = id;
      dst->payload_len = 1+0;
   }
}

extern "C" void ulog_detail_enqueue_1(uint8_t id, uint8_t v0) {
   if (LogPacket* dst = reserve_log_packet()) {
      dst->id = id;
      dst->payload_len = 1+1;
      dst->data[0] = v0;
   }
}

extern "C" void ulog_detail_enqueue_2(uint8_t id, uint8_t v0, uint8_t v1) {
   if (LogPacket* dst = reserve_log_packet()) {
      dst->id = id;
      dst->payload_len = 1+2;
      dst->data[0] = v0;
      dst->data[1] = v1;
   }
}

extern "C" void ulog_detail_enqueue_3(uint8_t id, uint8_t v0, uint8_t v1, uint8_t v2) {
   if (LogPacket* dst = reserve_log_packet()) {
      dst->id = id;
      dst->payload_len = 1+3;
      dst->data[0] = v0;
      dst->data[1] = v1;
      dst->data[2] = v2;
   }
}

extern "C" void ulog_detail_enqueue_4(uint8_t id, uint8_t v0, uint8_t v1, uint8_t v2, uint8_t v3) {
   if (LogPacket* dst = reserve_log_packet()) {
      dst->id = id;
      dst->payload_len = 1+4;
      dst->data[0] = v0;
      dst->data[1] = v1;
      dst->data[2] = v2;
      dst->data[3] = v3;
   }
}

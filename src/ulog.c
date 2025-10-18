/**
 * @file ulog.c
 * @brief Ultra-lightweight logging framework for embedded systems.
 * Common flavourless implementation of the library.
 * @author software@arreckx.com
 */

#include <ulog.h>
#include <ulog/ulog_porting.h>

//
// Porting guide macros;
// 
// Define these macros or functions in ulog_porting.h to adapt to your platform
// _ULOG_PORT_INIT                   : Initialize the UART and any other required resources
// _ULOG_PORT_INIT_ATTRIBUTES        : Attributes for the init function (e.g. constructor)
// _ULOG_PORT_ENTER_CRITICAL_SECTION : Enter critical section (save SREG if needed
// _ULOG_PORT_EXIT_CRITICAL_SECTION  : Exit critical section (restore SREG if needed)
// _ULOG_PORT_RING_THE_BELL          : Optional - ring a bell to notify that data is available
//


//
// Apply configuration defaults
//
#ifndef ULOG_QUEUE_SIZE
#  define ULOG_QUEUE_SIZE 16
#endif

// COBS framing char
#define EOF 0xA6

// Circular buffer (stubbed here)
#define MAX_PAYLOAD 4

// Circular buffer depth
#define QUEUE_DEPTH ULOG_QUEUE_SIZE

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

// Flag to indicate a buffer overrun masked into the trait of the log
// This is a simple way to indicate that some logs were lost
// due to buffer full condition
static uint8_t buffer_overrun = 0;


// ----------------------------------------------------------------------
// Internal functions
// ----------------------------------------------------------------------

/**
 * Reserve a log packet in the circular buffer
 * @return Pointer to the reserved packet, or nullptr if the buffer is full
 */
LogPacket *reserve_log_packet() {
    LogPacket *retval = nullptr;

    // As required - disable interrupt - but save since this could be used from within an interrupt
    _ULOG_PORT_ENTER_CRITICAL_SECTION();

    // Don't accept new logs while we are in overrun state
    // Rationale: Allow the buffer to clean, process logs faster to recover properly
    if ( buffer_overrun == 0 ) {
        uint8_t next = (log_head + 1) % QUEUE_DEPTH;

        if (next != log_tail) {
            retval = &logs_circular_buffer[log_head];
            log_head = next;
        } else {
            // Buffer full - set the overrun flag
            buffer_overrun = 1;
        }
    } else {
        if ( buffer_overrun < 255 ) {
            // Count the number of overruns - saturate at 255
            ++buffer_overrun;
        }
    }

    // Notify that data is available so a transmission can be initiated
    // This function must be callable from an interrupt context
    _ULOG_PORT_NOTIFY();

    // Restore SREG
    _ULOG_PORT_EXIT_CRITICAL_SECTION();

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
        uint8_t input_byte = input[read_index];

        if (input_byte == EOF) {
            tx_encoded[code_index] = code;
            code_index = write_index++;
            code = 1;
        } else {
            tx_encoded[write_index++] = input_byte;
            ++code;
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
    _ULOG_PORT_ENTER_CRITICAL_SECTION();

    if (log_tail != log_head) {
        // Data to send
        LogPacket& pkt = logs_circular_buffer[log_tail];
        log_tail = (log_tail + 1) % QUEUE_DEPTH;

        uint8_t encoded_len = cobs_encode(pkt.payload, pkt.payload_len);

        _ULOG_PORT_SEND_DATA(tx_encoded, encoded_len);
    } else if (buffer_overrun > 0) {
        static uint8_t overrun_payload[2] = { 0xFF, buffer_overrun };
        uint8_t encoded_len = cobs_encode(overrun_payload, 2);

        // Send an overrun notification packet
        _ULOG_PORT_SEND_DATA(tx_encoded, encoded_len);

        // Clear the overrun flag
        buffer_overrun = 0;
    }

    // Restore SREG
    _ULOG_PORT_EXIT_CRITICAL_SECTION();
}


// ----------------------------------------------------------------------
// C Linkage functions called from the inline assembly
// ----------------------------------------------------------------------

void ulog_detail_enqueue(uint8_t id) {
   if (LogPacket* dst = reserve_log_packet()) {
      dst->id = id;
      dst->payload_len = 1+0;
   }
}

void ulog_detail_enqueue_1(uint8_t id, uint8_t v0) {
   if (LogPacket* dst = reserve_log_packet()) {
      dst->id = id;
      dst->payload_len = 1+1;
      dst->data[0] = v0;
   }
}

void ulog_detail_enqueue_2(uint8_t id, uint8_t v0, uint8_t v1) {
   if (LogPacket* dst = reserve_log_packet()) {
      dst->id = id;
      dst->payload_len = 1+2;
      dst->data[0] = v0;
      dst->data[1] = v1;
   }
}

void ulog_detail_enqueue_3(uint8_t id, uint8_t v0, uint8_t v1, uint8_t v2) {
   if (LogPacket* dst = reserve_log_packet()) {
      dst->id = id;
      dst->payload_len = 1+3;
      dst->data[0] = v0;
      dst->data[1] = v1;
      dst->data[2] = v2;
   }
}

void ulog_detail_enqueue_4(uint8_t id, uint8_t v0, uint8_t v1, uint8_t v2, uint8_t v3) {
   if (LogPacket* dst = reserve_log_packet()) {
      dst->id = id;
      dst->payload_len = 1+4;
      dst->data[0] = v0;
      dst->data[1] = v1;
      dst->data[2] = v2;
      dst->data[3] = v3;
   }
}

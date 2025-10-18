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

namespace {
    // UART type
    using uart = uart::Uart<0,
    uart::CompileTimeConfig<
        115200,
        uart::width::_8,
        uart::parity::none,
        uart::stop::_1>
    >;

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
}

extern "C" void _ulog_avr_send_data(const uint8_t *data, size_t len) {
   uart::send(std::span<const uint8_t>(data, len));
}



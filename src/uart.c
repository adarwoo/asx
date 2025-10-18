/**
 * @file uart.c
 * @brief Basic UART support for AVR devices.
 * @author software@arreckx.com
 * C implementation of the UART to allow using the UART from C and C++ code.
 */
#include <stddef.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <reactor.h>

// Type for the DRE callback
typedef void(*uart_dre_callback)();

// ----------------------------------------------------------------------------
// Local reactor handles. To use, declare these extern in your C code.
// ----------------------------------------------------------------------------

// Reactor handles for RX and TX complete
// These are used by C code. Declare them extern in your C code to use them.
// This allow using the interrupt-driven UART from C code.
reactor_handle_t uart_on_usart0_rx_complete = REACTOR_NULL_HANDLE;
reactor_handle_t uart_on_usart0_tx_complete = REACTOR_NULL_HANDLE;

reactor_handle_t uart_on_usart1_rx_complete = REACTOR_NULL_HANDLE;
reactor_handle_t uart_on_usart1_tx_complete = REACTOR_NULL_HANDLE;

// These callbacks are managed by the Uart directly
uart_dre_callback dre_callback_uart0 = NULL;
uart_dre_callback dre_callback_uart1 = NULL;

// ----------------------------------------------------------------------------
// Reactor based ISR implementations
// ----------------------------------------------------------------------------
ISR(USART0_RXC_vect) {
    char c = USART0.RXDATAL; // Shifts the data
    reactor_notify(uart_on_usart0_rx_complete, (void*)c);
}

ISR(USART1_RXC_vect) {
    char c = USART1.RXDATAL; // Shifts the data
    reactor_notify(uart_on_usart1_rx_complete, (void*)c);
}

// The entire frame in the Transmit Shift register has been shifted out and there
//  are no new data in the transmit buffer (TXCIE)
ISR(USART0_TXC_vect) {
    reactor_notify_from_isr(uart_on_usart0_tx_complete);
    USART0.STATUS |= USART_TXCIE_bm;
}

ISR(USART1_TXC_vect) {
    reactor_notify_from_isr(uart_on_usart1_tx_complete);
    USART1.STATUS |= USART_TXCIE_bm;
}

// ----------------------------------------------------------------------------
// Callback based ISR implementations
// The DRE (Data Register Empty) ISR calls a user-defined callback and MUST be
//  set up by the user of this UART module.
// ----------------------------------------------------------------------------
ISR(USART0_DRE_vect) {
    dre_callback_uart0();
}

ISR(USART1_DRE_vect) {
    dre_callback_uart1();
}

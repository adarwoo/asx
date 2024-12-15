#pragma once
#pragma GCC diagnostic ignored "-Warray-bounds"

#include <stdint.h>
#include <string_view>

#include <asx/reactor.hpp>
#include <asx/utils.hpp>
#include <asx/chrono.hpp>

#include <avr/io.h>

#include "sysclk.h"
#ifdef SIM
#include <cstdio>
#include <iostream>
#endif


namespace asx {
   namespace uart {
      extern reactor::Handle on_usart0_tx_complete;
      extern reactor::Handle on_usart1_tx_complete;
      extern reactor::Handle on_usart0_rx_complete;
      extern reactor::Handle on_usart1_rx_complete;

      using dre_callback = void(*)();

      extern dre_callback dre_callback_uart0;
      extern dre_callback dre_callback_uart1;

      enum class width { _5=5, _6=6, _7=7, _8=8 }; // Note 9bits is not supported
      enum class parity { none, odd, even };
      enum class stop { _1=1, _2=2 };

      // Options
      constexpr auto onewire = 1<<1;
      constexpr auto rs485   = 1<<2;;
      constexpr auto map_to_alt_position = 1<<3;
      constexpr auto disable_rx = 1<<4;
      constexpr auto disable_tx = 1<<5;

      template<int N, long BAUD, width W, parity P, stop S, int OPTIONS=0>
      class Uart {
         ///< Contains a view to transmit
         inline static std::string_view to_send;

         static_assert(N < 2, "Invalid USART number");

         static USART_t & get() {
            if constexpr (N == 0) {
               return USART0;
            }

            return USART1;
         }

         static constexpr uint16_t get_baud() {
            // Compute on full precision
            unsigned long baud = ((64UL * F_CPU) / ((unsigned long)BAUD)) / 16UL;
            return static_cast<uint16_t>(baud);
         }

         static consteval uint8_t get_ctrl_a() {
            uint8_t retval = 0;

            if (OPTIONS & rs485) {
               retval |= USART_RS485_bm;
            }

            if (OPTIONS & onewire) {
               retval |= USART_LBME_bm;
            }

            return retval;
         }

         static consteval uint8_t get_ctrl_b() {
            uint8_t retval = USART_RXEN_bm | USART_TXEN_bm | USART_RXMODE_NORMAL_gc;

            if (OPTIONS & onewire) {
               retval |= USART_ODME_bm;
            }

            if (OPTIONS & disable_rx) {
               retval &= (~USART_RXEN_bm);
            }

            if (OPTIONS & disable_rx) {
               retval &= (~USART_TXEN_bm);
            }

            return retval;
         }

         static consteval uint8_t get_ctrl_c() {
            uint8_t retval = USART_CMODE_ASYNCHRONOUS_gc;

            if (W == width::_5) {
               retval |= USART_CHSIZE_5BIT_gc;
            } else if (W == width::_6) {
               retval |= USART_CHSIZE_6BIT_gc;
            } else if (W == width::_7) {
               retval |= USART_CHSIZE_7BIT_gc;
            } else if (W == width::_8) {
               retval |= USART_CHSIZE_8BIT_gc;
            }

            if (P == parity::odd) {
               retval |= USART_PMODE_ODD_gc;
            } else if (P == parity::even) {
               retval |= USART_PMODE_EVEN_gc;
            }

            if (S == stop::_1) {
               retval |= USART_SBMODE_1BIT_gc;
            } else if (S == stop::_2) {
               retval |= USART_SBMODE_2BIT_gc;
            }

            return retval;
         }

      public:
         static void init() {
            if (OPTIONS & map_to_alt_position) {
               if (N == 0) {
                  PORTMUX_USARTROUTEA |= PORTMUX_USART0_ALT1_gc;

                  if (OPTIONS & onewire) {
                     PORTA.PIN1CTRL |= PORT_PULLUPEN_bm;
                     VPORTA_DIR |= _BV(4);
                  } else {
                     VPORTA_DIR |= _BV(1);
                  }
               } else {
                  PORTMUX_USARTROUTEA |= 4; // Bug in AVR defs

                  if (OPTIONS & onewire) {
                     PORTC.PIN2CTRL |= PORT_PULLUPEN_bm;
                     VPORTC_DIR |= _BV(3);
                  } else {
                     VPORTC_DIR |= _BV(2);
                  }
               }
            } else {
               if (N == 0) {
                  if (OPTIONS & onewire) {
                     PORTB.PIN2CTRL |= PORT_PULLUPEN_bm;
                     VPORTB_DIR |= _BV(0);
                  } else {
                     VPORTB_DIR |= _BV(2);
                  }

               } else {
                  if (OPTIONS & onewire) {
                     PORTA.PIN1CTRL |= PORT_PULLUPEN_bm;
                     VPORTA_DIR |= _BV(4);
                  } else {
                     VPORTA_DIR |= _BV(1);
                  }
               }
            }

            get().CTRLA = get_ctrl_a();
            get().CTRLB = get_ctrl_b();
            get().CTRLC = get_ctrl_c();
            get().BAUD = get_baud();

            // Register a reactor for filling the buffer
            if ( N == 0 ) {
               dre_callback_uart0 = &on_dre;
            } else {
               dre_callback_uart1 = &on_dre;
            }
         }

         static void send(const std::string_view view_to_send) {
            // Store the view to transmit
            to_send = view_to_send;

            // Enable the DRE and TXCIE interrupts
            get().CTRLA |= USART_DREIE_bm | USART_TXCIE_bm;
            #ifdef SIM
               char buffer[512]; // Ensure the buffer is large enough
               size_t buffer_pos = 0;

               // Convert the string_view to hex and write to the buffer
               for (unsigned char c : view_to_send) {
                  buffer_pos += std::snprintf(buffer + buffer_pos, sizeof(buffer) - buffer_pos, "%02X ", c);
               }

               // Remove the trailing space, if necessary
               if (buffer_pos > 0) {
                  buffer[buffer_pos - 1] = '\0';
               } else {
                  buffer[0] = '\0';
               }

               LOG_INFO("UART", "%s", buffer);
            #endif
         }

         // Called from the DRE interrupt to indicate there is space in the Tx buffer
         static void on_dre()
         {
            if ( not to_send.empty() ) {
               get().TXDATAL = to_send.front();
               to_send.remove_prefix(1);
            } else {
               // Disable the DRE interrupt
               get().CTRLA &= ~USART_DREIE_bm;
            }
         }

		   static void react_on_send_complete( reactor_handle_t reactor ) {
            // Register a reactor for filling the buffer
            if ( N == 0 ) {
               on_usart0_tx_complete = reactor;
            } else {
               on_usart1_tx_complete = reactor;
            }
		   }

		   static void react_on_character_received( reactor_handle_t reactor ) {
            // Register a reactor for filling the buffer
            if ( N == 0 ) {
               on_usart0_rx_complete = reactor;
            } else {
               on_usart1_rx_complete = reactor;
               // Enable the interrupt
               get().CTRLA |= USART_RXCIE_bm;
            }
		   }

         static constexpr asx::chrono::cpu_tick_t get_byte_duration(const float length_multipler=1.0) {
            int width = 1 /* start bit */
               + (int)W /* Width 5 to 9 */
               + (int)S /* Number of stop bits 1 to 2 */
               + (P==parity::none ? 0 : 1); /* Extra parity bit */

            return asx::chrono::cpu_tick_t(
               static_cast<unsigned long>(
                  (width * F_CPU * length_multipler) / (unsigned long)BAUD
               )
            );
         }
      };
   } // end of namespace uart
} // end of namespace asx



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

#include <alert.h>
#include <asx/reactor.hpp>
#include <asx/i2c_common.hpp>

namespace asx {
    namespace i2c {
        /* Transaction status defines.*/
        static constexpr auto STATUS_READY = 0;
        static constexpr auto STATUS_BUSY  = 1;

        /* Transaction result enumeration */
        enum class result_t {
            UNKNOWN            = (0x00<<0),
            OK                 = (0x01<<0),
            BUFFER_OVERFLOW    = (0x02<<0),
            TRANSMIT_COLLISION = (0x03<<0),
            BUS_ERROR          = (0x04<<0),
            FAIL               = (0x05<<0),
            ABORTED            = (0x06<<0),
        };

        /// Buffer size defines
        static constexpr auto RECEIVE_BUFFER_SIZE = 8;
        /// Buffer size defines
        static constexpr auto SEND_BUFFER_SIZE    = 8;

        class Slave {
            static void (*Process_Data) (void);                   /*!< Pointer to process data function*/
            static register8_t receivedData[RECEIVE_BUFFER_SIZE]; /*!< Read data*/
            static register8_t sendData[SEND_BUFFER_SIZE];        /*!< Data to write*/
            static register8_t bytesReceived;                     /*!< Number of bytes received*/
            static register8_t bytesSent;                         /*!< Number of bytes sent*/
            static register8_t status;                            /*!< Status of transaction*/
            static register8_t result;                            /*!< Result of transaction*/
            static bool abort;                                    /*!< Strobe to abort*/

        private:
            static void init(void (*processDataFunction) (void));

            /*! \brief Enable Slave Mode of the TWI. */
            static inline void twi_slave_enable(TWI_t *twi) {
                TWI0.SCTRLA |= TWI_ENABLE_bm;
            }

            /*! \brief Disable Slave Mode of the TWI. */
            static inline void twi_slave_disable(TWI_t *twi) {
                TWI0.SCTRLA &= (~TWI_ENABLE_bm);
            }

        protected:
            static void initialize_module(uint8_t address);
            static void interrupt_handler();
            static void address_match_handler();
            static void stop_handler();
            static void data_handler();
            static void read_handler();
            static void write_handler();
            static void transaction_finished(uint8_t result);
        };
    } // i2c
} // asx

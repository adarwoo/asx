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

#include <cstdint>

namespace asx {
    namespace i2c {

        /**
         * Status code that may be returned by shell commands and protocol
         * implementations.
         *
         * \note Any change to these status codes and the corresponding
         * message strings is strictly forbidden. New codes can be added,
         * however, but make sure that any message string tables are updated
         * at the same time.
         */
        enum class status_code_t : int8_t {
            STATUS_OK               =  0, //!< Success
            ERR_IO_ERROR            =  -1, //!< I/O error
            ERR_FLUSHED             =  -2, //!< Request flushed from queue
            ERR_TIMEOUT             =  -3, //!< Operation timed out
            ERR_BAD_DATA            =  -4, //!< Data integrity check failed
            ERR_PROTOCOL            =  -5, //!< Protocol error
            ERR_UNSUPPORTED_DEV     =  -6, //!< Unsupported device
            ERR_NO_MEMORY           =  -7, //!< Insufficient memory
            ERR_INVALID_ARG         =  -8, //!< Invalid argument
            ERR_BAD_ADDRESS         =  -9, //!< Bad address
            ERR_BUSY                =  -10, //!< Resource is busy
            ERR_BAD_FORMAT          =  -11, //!< Data format not recognized
            ERR_NO_TIMER            =  -12, //!< No timer available
            ERR_TIMER_ALREADY_RUNNING   =  -13, //!< Timer already running
            ERR_TIMER_NOT_RUNNING   =  -14, //!< Timer not running

            /**
             * \brief Operation in progress
             *
             * This status code is for driver-internal use when an operation
             * is currently being performed.
             *
             * \note Drivers should never return this status code to any
             * callers. It is strictly for internal use.
             */
            OPERATION_IN_PROGRESS	= -128,
        };

        /// @brief Callback type for when a i2c transaction has completed
        using CompleteCb = void(*)(status_code_t);

        /*!
        * \brief Information concerning the data transmission
        */
        struct Package
        {
            //! TWI chip address to communicate with.
            uint8_t chip;
            //! TWI address/commands to issue to the other chip (node).
            uint8_t addr[3];
            //! Length of the TWI data address segment (1-3 bytes).
            uint8_t addr_length;
            //! Where to find the data to be written.
            uint8_t *buffer;
            //! How many bytes do we want to write.
            uint8_t length;
            //! Callback when the operation is complete. Called from the reactor.
            CompleteCb on_complete;
        };
    } // i2c
} // asx
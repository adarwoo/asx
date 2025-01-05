#pragma once

#include <cmath>

#include <alert.h>
#include <asx/reactor.hpp>
#include <asx/i2c_common.hpp>

#define TWI_BAUD(freq, t_rise) ((F_CPU / freq) / 2) - (5 + (((F_CPU / 1000000) * t_rise) / 2000))

namespace asx {
    namespace i2c {

        /// Helper which computes the value of the BAUD register based on the frequency.
        constexpr uint8_t calc_baud(uint32_t frequency)
        {
            int16_t baud;

        #if (F_CPU == 20000000) || (F_CPU == 10000000)
            if (frequency >= 600000)
            { // assuming 1.5kOhm
                baud = TWI_BAUD(frequency, 250);
            }
            else if (frequency >= 400000)
            { // assuming 2.2kOhm
                baud = TWI_BAUD(frequency, 350);
            }
            else
            {									 // assuming 4.7kOhm
                baud = TWI_BAUD(frequency, 600); // 300kHz will be off at 10MHz. Trade-off between size and accuracy
            }
        #else
            if (frequency >= 600000)
            { // assuming 1.5kOhm
                baud = TWI_BAUD(frequency, 250);
            }
            else if (frequency >= 400000)
            { // assuming 2.2kOhm
                baud = TWI_BAUD(frequency, 400);
            }
            else
            { // assuming 4.7kOhm
                baud = TWI_BAUD(frequency, 600);
            }
        #endif

        #if (F_CPU >= 20000000)
            const uint8_t baudlimit = 2;
        #elif (F_CPU == 16000000) || (F_CPU == 8000000) || (F_CPU == 4000000)
            const uint8_t baudlimit = 1;
        #else
            const uint8_t baudlimit = 0;
        #endif

            if (baud < baudlimit)
            {
                return baudlimit;
            }
            else if (baud > 255)
            {
                return 255;
            }

            return (uint8_t)baud;
        }

        // Define the user-defined literal for kilohertz
        constexpr unsigned long long operator""_KHz(unsigned long long v) {
            return static_cast<unsigned long long>(v * 1000L);
        }

        // Define the user-defined literal for megahertz
        constexpr unsigned long long operator""_MHz(unsigned long long v) {
            return static_cast<unsigned long long>(v * 1000000L);
        }

        // Define the user-defined literal for kilohertz
        constexpr unsigned long long operator""_KHz(long double v) {
            return static_cast<unsigned long long>(std::round(v * 1000L));
        }

        // Define the user-defined literal for megahertz
        constexpr unsigned long long operator""_MHz(long double v) {
            return static_cast<unsigned long long>(std::round(v * 1000000L));
        }

        class Master {
            /// TWI device to use
            static inline Package *pkg = nullptr;

            /// @brief TWI Instance
            static inline int8_t addr_count;	   // Bus transfer address data counter
            static inline uint8_t data_count;	   // Bus transfer payload data counter
            static inline bool read;			   // Bus transfer direction
            static inline volatile status_code_t status; // Transfer status

        public:
            static void init(const unsigned long bus_speed_hz) {
                TWI0.MBAUD = calc_baud(bus_speed_hz);
                TWI0.MCTRLB |= TWI_FLUSH_bm;
                TWI0.MCTRLA = TWI_RIEN_bm | TWI_WIEN_bm | TWI_ENABLE_bm;
                TWI0.MSTATUS = TWI_BUSSTATE_IDLE_gc;

                status = status_code_t::STATUS_OK;
            }

            /// \brief Enable Master Mode of the TWI.
            static inline void enable() {
                TWI0.MCTRLA |= TWI_ENABLE_bm;
            }

            /// \brief Disable Master Mode of the TWI.
            static inline void disable(TWI_t *twi) {
                TWI0.MCTRLA &= (~TWI_ENABLE_bm);
            }

            static void transfer(Package &package, bool _read = false);

            /// \brief Test for an idle bus state.
            static inline bool is_idle () {
                return ((TWI0.MSTATUS & TWI_BUSSTATE_gm)
                        == TWI_BUSSTATE_IDLE_gc);
            }
        private:
            static void write_handler();
            static void read_handler(void);

        public: // Evil but required for the interrupt with a C linkage
            static void interrupt_handler();
        };
    } // i2c
} // asx

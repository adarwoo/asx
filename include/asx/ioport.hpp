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

/**
 * @file ioport.hpp
 * @brief C++ ioport service for AVR microcontrollers
 * @details
 * This file provides a C++ interface for the ioport service, allowing
 * for easy configuration and control of GPIO pins.
 * It includes features such as setting pin direction, level, and
 * configuring pull-up resistors, sense modes, and inversion.
 * The implementation is designed to be efficient and easy to use,
 * leveraging C++ features like templates and constexpr for compile-time
 * evaluation.
 *
 * The library allows for managing ports and pins in 2 different ways:
 * 1. Using the PortDef and PinDef classes for compile-time configuration.
 * 2. Using the Port and Pin classes for run-time configuration.
 * Note: The PinDef and PortDef classes are RValue-only and do not
 *       carry any data. They are used for compile-time evaluation.
 *       The Pin and Port classes are data-carrying classes that can
 *       be constructed from PortDef and PinDef respectively, as well as
 *       passed in functions.
 *
 * Note: The MACRO IOPORT is also used by the C library to define IOPORT pins.
 *       When defining a pin using the IOPORT macro, the pin can be used from
 *       either C or C++.
 * For C++, the namespace asx::ioport must be used and the pins can be defined
 * in a heaader as constexpr variables.
 *
 * Warning: This code will not compile if the optimization level is set to -O0.
 * Even with -Og or -O1, the code will be optimized to a single instruction.
 *
 * The init() method of the Pin class can be used to configure the pin in one
 * go, similar to the device tree configuration.
 * The pin options available are:
 * - Initial value        : value_t::high, value_t::low
 * - Direction of the pin : dir_t::in, dir_t::out
 * - Pull-up resistor     : pullup::enabled, pullup::disabled
 * - Sense mode           : sense::rising, sense::falling, sense::bothedges, sense::level_low
 * - Inversion            : invert::normal, invert::inverted
 *
 * Example usage:
 * @code
 * #include "ioport.hpp"
 *
 * using namespace asx::ioport;
 *
 * // Define a pin using the PinDef template
 * using MyPin = PinDef<A, 0>; // As a type
 * constexpr auto my_pin = PinDef<C, 5>{}; // As a variable
 *
 * // Alternatively, you can use the IOPORT macro to define a pin as a variable
 * #define OTHER_PIN IOPORT(A, 0)
 *
 * // Set the pin direction to output
 * MyPin::set_dir(dir_t::out);
 * my_pin.set_dir(dir_t::out);
 * OTHER_PIN.set_dir(dir_t::out);
 *
 * // Yeilds a single machine instruction
 *
 * // For run time configuration, use the Port and Pin classes
 * Port port{A}; // Create a Port object for port A
 * Pin pin{A, 0}; // Create a Pin object for pin A0
 * // The Pin object is only 1 byte and can be passed around
 * // Set the pin direction to output (same API as above)
 * pin.set_dir(dir_t::out);
 *
 * // Pins can be configured with options like in device tree
 * pin.init(dir_t::in, pullup::enabled, sense::rising);
 * pin.init(dir_t::out, sense::rising, invert::inverted);
 *
 * </code>
 * @author software@arreckx.com
 */

#include <cstdint>
#include <type_traits>

#include <avr/io.h>

namespace asx {
   namespace ioport {
      enum class dir_t : uint8_t {
         in = 0,
         out = 1
      };

      enum class value_t : uint8_t{
         low = 0,
         high = 1
      };

      struct option_t {
         uint8_t value;
      };

      struct pinctrl_t : option_t  {};

      // CRTP Base for Scoped Options
      template <typename Derived>
      struct scoped_option_t : pinctrl_t {
         constexpr explicit scoped_option_t(uint8_t v) : pinctrl_t{v} {}

         // Allow implicit conversion to uint8_t for ease of use
         constexpr operator uint8_t() const { return value; }
      };

      // Specialized Scoped Options
      struct sense_t : scoped_option_t<sense_t> {
         constexpr explicit sense_t(uint8_t v) : scoped_option_t(v) {}
      };

      namespace sense {
         static constexpr sense_t interrupt_disable{PORT_ISC_INTDISABLE_gc};
         static constexpr sense_t bothedges{PORT_ISC_BOTHEDGES_gc};
         static constexpr sense_t rising{PORT_ISC_RISING_gc};
         static constexpr sense_t falling{PORT_ISC_FALLING_gc};
         static constexpr sense_t input_disabled{PORT_ISC_INPUT_DISABLE_gc};
         static constexpr sense_t level_low{PORT_ISC_LEVEL_gc};
      }

      struct invert_t : scoped_option_t<invert_t> {
         constexpr explicit invert_t(uint8_t v) : scoped_option_t(v) {}
      };

      namespace invert{
         static constexpr invert_t normal{0};
         static constexpr invert_t inverted{PORT_INVEN_bm};
      }

      struct pullup_t : scoped_option_t<pullup_t>{
         constexpr explicit pullup_t(uint8_t v) : scoped_option_t(v) {}
      };

      namespace pullup {
         static constexpr pullup_t disabled{0};
         static constexpr pullup_t enabled{PORT_PULLUPEN_bm};
      }

      // Forward declaration of PinDef
      // This is needed to allow the use of PinDef in the init_impl function
      template <typename PORTDEF, uint8_t PIN_NUMBER>
      struct PinDef;

      namespace aux {
         // Compute PINCTRL register value by summing options that inherit from pinctrl_t
         template <typename... OPTS>
         static constexpr uint8_t compute_pinctrl(OPTS... opts) {
            uint8_t result = 0;
            ((result |= static_cast<uint8_t>(std::is_base_of_v<pinctrl_t, OPTS> ? static_cast<uint8_t>(opts) : 0)), ...);
            return result;
         }

         // Extract argument helper
         template <typename Target, typename First, typename... Rest>
         constexpr Target extract_argument(First first, Rest... rest) {
            if constexpr (std::is_same_v<First, Target>) {
               return first; // Found the value
            } else {
               return extract_argument<Target>(rest...); // Recurse
            }
         }

         template <typename PORTDEF, uint8_t PIN_NUMBER, typename... T>
         static inline constexpr void init_impl(PORTDEF, uint8_t pin_number, T... args) {
             constexpr bool has_value = (std::is_same_v<T, value_t> || ...);

             if constexpr (has_value) {
                 PinDef<PORTDEF, PIN_NUMBER>::set(extract_argument<value_t>(args...));
             }

             constexpr bool has_dir = (std::is_same_v<T, dir_t> || ...);

             if constexpr (has_dir) {
                 PinDef<PORTDEF, PIN_NUMBER>::set_dir(extract_argument<dir_t>(args...));
             }

             // Check if any argument contributes to the PINCTRL register
             constexpr bool has_pinctrl = (std::is_base_of_v<pinctrl_t, T> || ...);

             if constexpr (has_pinctrl) {
                 // Compute the PINCTRL register value
                 uint8_t pinctrl_value = compute_pinctrl(args...);
                 if (pinctrl_value != 0) {
                     register8_t* pinctrl = &(PORTDEF::base()->PIN0CTRL) + pin_number;
                     *pinctrl = pinctrl_value;
                 }
             }
         }
      } // namespace aux


      // ----------------------------------------------------------------------
      // PortDef: Rvalue-only, no data, static methods
      // ----------------------------------------------------------------------
      template <uint8_t PORT_INDEX>
      struct PortDef {
         inline static constexpr uint8_t index() { return PORT_INDEX; }

         inline static constexpr PORT_t* base() {
            return reinterpret_cast<PORT_t*>(0x400 + (PORT_INDEX * 0x20));
         }

         inline static constexpr VPORT_t* vbase() {
            return reinterpret_cast<VPORT_t*>(0x0000 + (PORT_INDEX * 0x04));
         }

         inline static constexpr void set_slewrate(bool enabled) {
            if (enabled) {
               base()->PORTCTRL |= 1;
            } else {
               base()->PORTCTRL &= ~1;
            }
         }
      };

      // ----------------------------------------------------------------------
      // Port: Data-carrying class, constructible from PortDef
      // ----------------------------------------------------------------------
      class Port {
         uint8_t port_index;

      public:
         constexpr Port(uint8_t index) : port_index(index) {}

         template <typename PortDef>
         constexpr Port(PortDef) : port_index(PortDef::index()) {}

         constexpr uint8_t index() const { return port_index; }

         constexpr PORT_t* base() const {
            return reinterpret_cast<PORT_t*>(0x400 + (port_index * 0x20));
         }

         constexpr VPORT_t* vbase() const {
            return reinterpret_cast<VPORT_t*>(0x0000 + (port_index * 0x04));
         }

         void set_slewrate(bool enabled) const {
            if (enabled) {
               base()->PORTCTRL |= 1;
            } else {
               base()->PORTCTRL &= ~1;
            }
         }
      };

      // ----------------------------------------------------------------------
      // PinDef: Rvalue-only, no data, static methods
      // ----------------------------------------------------------------------
      template <typename PORTDEF, uint8_t PIN_NUMBER>
      struct PinDef {
         using PortDef = PORTDEF;
         inline static constexpr uint8_t pin() { return PIN_NUMBER; }
         inline static constexpr uint8_t mask() { return 1U << PIN_NUMBER; }

         inline static constexpr void set(value_t value = value_t::high) {
            if (value == value_t::high) {
               asm volatile("sbi %0, %1" : : "I"(&PORTDEF::vbase()->OUT), "I"(PIN_NUMBER));
            } else {
               asm volatile("cbi %0, %1" : : "I"(&PORTDEF::vbase()->OUT), "I"(PIN_NUMBER));
            }
         }

         inline static constexpr void set(bool value = true) {
            if (value) {
               asm volatile("sbi %0, %1" : : "I"(&PORTDEF::vbase()->OUT), "I"(PIN_NUMBER));
            } else {
               asm volatile("cbi %0, %1" : : "I"(&PORTDEF::vbase()->OUT), "I"(PIN_NUMBER));
            }
         }

         static inline constexpr void clear() {
            asm volatile("cbi %0, %1" : : "I"(&PORTDEF::vbase()->OUT), "I"(PIN_NUMBER));
         }

         static inline constexpr void toggle() {
            asm volatile("sbi %0, %1" : : "I"(&PORTDEF::vbase()->OUTTGL), "I"(PIN_NUMBER));
         }

         static inline constexpr bool get() {
            return PORTDEF::vbase()->IN & mask();
         }

         // Add operator*() to behave like get()
         constexpr bool operator*() {
            return get();
         }

         static constexpr void set_dir(const dir_t dir) {
            if (dir == dir_t::in) {
               asm volatile("sbi %0, %1" : : "I"(&PORTDEF::vbase()->DIR), "I"(PIN_NUMBER));
            } else {
               asm volatile("cbi %0, %1" : : "I"(&PORTDEF::vbase()->DIR), "I"(PIN_NUMBER));
            }
         }

         /**
          * @brief Initialize the pin with the given options
          * @param args Options to configure the pin
          * @details
          * This method allows for configuring the pin with multiple options
          * in a single call. The options can include:
          * - Initial value        : value_t::high, value_t::low
          * - Direction of the pin : dir_t::in, dir_t::out
          * - Pull-up resistor     : pullup::enabled, pullup::disabled
          * - Sense mode           : sense::rising, sense::falling, sense::bothedges, sense::level_low
          * - Inversion            : invert::normal, invert::inverted
          * @note The order of the arguments does not matter, but the types must be correct.
          * @note The function will only set the pinctrl register if any of the arguments
          *       are of type pinctrl_t or derived from it.
          */
         template <typename... T>
         static inline constexpr void init(T... args) {
             aux::init_impl<PORTDEF, PIN_NUMBER>(PORTDEF{}, PIN_NUMBER, args...);
         }
      };


      /**
       * @brief Pin class for runtime pin manipulation
       * @details
       * The Pin class can be used in a similar way to the PinDef class, but
       * can be constructed at runtime.
       * Note: The Pin cannot be construction uninitialized as this would result in
       *       undefined behavior.
       */
      class Pin {
      public:
         using mask_t = uint8_t;

      private:
         uint8_t port_pin;

      public:
         // Constructors
         constexpr Pin(const Port port, const uint8_t pin) : port_pin((port.index() * 8U) + pin) {}

         template <typename PinDef>
         constexpr Pin(PinDef) : port_pin( PinDef::PortDef::index() * 8U + PinDef::pin() ) {}

         // Delete the default constructor to prevent uninitialized Pins
         Pin() = delete;

         constexpr Pin(const Pin& copy) : port_pin(copy.port_pin) {}

         constexpr Pin(Pin&& rvalue) : port_pin(rvalue.port_pin) {}

         // Assignment Operators
         constexpr Pin& operator=(const Pin& copy) {
            port_pin = copy.port_pin;
            return *this;
         }

         constexpr Pin& operator=(Pin&& rvalue) {
            port_pin = rvalue.port_pin;
            return *this;
         }

         /**
          * Initialize the pin with the given options
          * @see PinDef::init()
          * @returns a reference to the Pin object
          */
         template <typename... T>
         inline constexpr Pin& init(T... args) {
             aux::init_impl<Port, pin()>(port(), pin(), args...);
             return *this;
         }

         // Accessors
         inline constexpr Port port() const {
            uint8_t index = port_pin >> 3;
            return Port{index};
         }

         inline constexpr uint8_t pin() const {
            return port_pin & 0x07;
         }

         inline constexpr mask_t mask() const {
            return 1U << pin();
         }

#ifdef SIM
         inline PORT_t* base() const {
            return port().base();
         }

         inline VPORT_t* vbase() const {
            return port().vbase();
         }
#else
         inline constexpr PORT_t* base() const {
            return port().base();
         }

         inline constexpr VPORT_t* vbase() const {
            return port().vbase();
         }
#endif

         // Pin Operations
         inline auto operator*() -> bool {
            return vbase()->IN & mask();
         }

         inline auto set(bool value=true) -> void {
            if (value) {
               vbase()->OUT |= mask();
            } else {
               vbase()->OUT &= ~mask();
            }
         }

         inline auto set(const value_t value) -> void {
            if (value == value_t::high) {
               vbase()->OUT |= mask();
            } else {
               vbase()->OUT &= ~mask();
            }
         }

         inline auto set_dir(const dir_t dir) -> void {
            if (dir == dir_t::in) {
               vbase()->DIR &= ~mask();
            } else {
               vbase()->DIR |= mask();
            }
         }

         inline auto clear() -> void {
            vbase()->OUT &= ~mask();
         }

         inline auto toggle() -> void {
            vbase()->OUT ^= mask();
         }
      };

      // Actual ports - rvalue only
      using A = PortDef<0>;
      using B = PortDef<1>;
      using C = PortDef<2>;
   } // namespace ioport
}

#undef IOPORT

// Helper function to create a PinDef compatible with a macro definition
#define ASX_IOPORT_A asx::ioport::A
#define ASX_IOPORT_B asx::ioport::B
#define ASX_IOPORT_C asx::ioport::C

// Actual macro that uses token pasting
#define IOPORT(PORTDEF, PIN) \
   asx::ioport::PinDef<ASX_IOPORT_ ## PORTDEF, PIN>{}

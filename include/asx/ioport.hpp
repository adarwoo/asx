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
#include <avr/io.h>

#undef Pin

namespace asx {
   // Provide own traits to remove need for stdlib
   namespace traits {
      template <typename Base, typename Derived>
      concept is_base_of = requires(Derived *d) {
         { static_cast<Base *>(d) };
      };

      template <typename Base, typename Derived>
      inline constexpr bool is_base_of_v = is_base_of<Base, Derived>;

      template <class T, T V>
      struct integral_constant {
         using type = integral_constant;
         static constexpr T value = V;
      };

      using true_type = integral_constant<bool, true>;
      using false_type = integral_constant<bool, false>;

      template <class, class>
      struct is_same : false_type  {};

      template <class T>
      struct is_same<T, T> : true_type {};

      template <typename T1, typename T2>
      inline constexpr bool is_same_v = is_same<T1, T2>::value;
   }

   namespace ioport {
      enum class dir_t : uint8_t {
         in = 0,
         out = 1,
         configured = 2
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

      enum class slewrate_limit : uint8_t {
         disabled = 0,
         enabled = 1
      };

      namespace aux {
         // Compute PINCTRL register value by summing options that inherit from pinctrl_t
         template <typename... OPTS>
         static constexpr uint8_t compute_pinctrl() {
            uint8_t result = 0;
            ((result |= static_cast<uint8_t>(traits::is_base_of_v<pinctrl_t, OPTS> ? OPTS::value : 0)), ...);
            return result;
         }

         // Extract argument helper
         template <typename Target, typename First, typename... Rest>
         constexpr Target extract_argument(First first, Rest... rest) {
            if constexpr (traits::is_same_v<First, Target>) {
               return first; // Found the value
            } else {
               return extract_argument<Target>(rest...); // Recurse
            }
         }
      } // namespace aux


      // ----------------------------------------------------------------------
      // PortDef: Rvalue-only, no data, static methods
      // ----------------------------------------------------------------------
      template <uint8_t PORT_INDEX>
      struct PortDef {
         static constexpr uint8_t index() { return PORT_INDEX; }

         static constexpr PORT_t* base() {
            return reinterpret_cast<PORT_t*>(0x400 + (PORT_INDEX * 0x20));
         }

         static constexpr VPORT_t* vbase() {
            return reinterpret_cast<VPORT_t*>(0x0000 + (PORT_INDEX * 0x04));
         }

         static constexpr void set_slewrate(bool enabled) {
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
      template <typename PortDef, uint8_t PIN_NUMBER>
      struct PinDef {
         static constexpr uint8_t pin() { return PIN_NUMBER; }
         static constexpr uint8_t mask() { return 1U << PIN_NUMBER; }

         static constexpr void set(bool value = true) {
            if (value) {
               asm volatile("sbi %0, %1" : : "I"(&PortDef::vbase()->OUT), "I"(PIN_NUMBER));
            } else {
               asm volatile("cbi %0, %1" : : "I"(&PortDef::vbase()->OUT), "I"(PIN_NUMBER));
            }
         }

         static constexpr void clear() {
            asm volatile("cbi %0, %1" : : "I"(&PortDef::vbase()->OUT), "I"(PIN_NUMBER));
         }

         static constexpr void toggle() {
            asm volatile("sbi %0, %1" : : "I"(&PortDef::vbase()->OUTTGL), "I"(PIN_NUMBER));
         }

         static constexpr bool get() {
            return PortDef::vbase()->IN & mask();
         }

         // Add operator*() to behave like get()
         constexpr bool operator*() {
            return get();
         }

         static constexpr void set_dir(const dir_t dir) {
            if (dir == dir_t::in) {
               asm volatile("sbi %0, %1" : : "I"(&PortDef::vbase()->DIR), "I"(PIN_NUMBER));
            } else {
               asm volatile("cbi %0, %1" : : "I"(&PortDef::vbase()->DIR), "I"(PIN_NUMBER));
            }
         }

         // Initialization
         template <typename... T>
         inline constexpr PinDef& init(T... args) {
            constexpr bool has_value = (traits::is_same_v<T, value_t> || ...);

            if constexpr (has_value) {
               set(extract_argument<value_t>(args...));
            }

            constexpr bool has_dir = (traits::is_same_v<T, dir_t> || ...);

            if constexpr (has_dir) {
               set_dir(extract_argument<dir_t>(args...));
            }

            // Compute the PINCTRL register value
            uint8_t pinctrl_value = aux::compute_pinctrl();

            if (pinctrl_value != 0) {
               register8_t* pinctrl = &(PortDef::base()->PIN0CTRL) + PIN_NUMBER;
               *pinctrl = pinctrl_value;
            }

            return *this;
         }
      };

      // Helper function to create a PinDef compatible with a macro definition
      #define IOPORT(PORTDEF, PIN) \
         asx::ioport::Pin(PORTDEF, PIN)

      class Pin {
      public:
         using portpin_t = uint8_t;
         using mask_t = uint8_t;

      private:
         portpin_t port_pin;

      public:
         // Constructors
         constexpr Pin(const Port port, const uint8_t pin) : port_pin((port.index() * 8U) + pin) {}

         template <typename PinDef>
         constexpr Pin(PinDef) : port_pin( PinDef::PortDef::index() << 3 & PinDef::pin() ) {}

         constexpr Pin(const Pin& copy) : port_pin(copy.port_pin) {}

         constexpr Pin(Pin&& rvalue) : port_pin(rvalue.port_pin) {}

         constexpr Pin(const portpin_t pp) : port_pin(pp) {}

         // Assignment Operators
         constexpr Pin& operator=(const Pin& copy) {
            port_pin = copy.port_pin;
            return *this;
         }

         constexpr Pin& operator=(Pin&& rvalue) {
            port_pin = rvalue.port_pin;
            return *this;
         }

         constexpr Pin& operator=(const portpin_t pp) {
            port_pin = pp;
            return *this;
         }

         // Initialization
         template <typename... T>
         inline constexpr Pin& init(T... args) {
            constexpr bool has_value = (traits::is_same_v<T, value_t> || ...);

            if constexpr (has_value) {
               set(aux::extract_argument<value_t>(args...));
            }

            constexpr bool has_dir = (traits::is_same_v<T, dir_t> || ...);

            if constexpr (has_dir) {
               set_dir(aux::extract_argument<dir_t>(args...));
            }

            // Compute the PINCTRL register value
            uint8_t pinctrl_value = aux::compute_pinctrl();

            if (pinctrl_value != 0) {
               register8_t* pinctrl = &(base()->PIN0CTRL) + (port_pin & 0x07);
               *pinctrl = pinctrl_value;
            }

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

         inline constexpr portpin_t integral() const {
            return port_pin;
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

         inline auto set(bool value) -> void {
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
      constexpr auto A = PortDef<0>{};
      constexpr auto B = PortDef<1>{};
      constexpr auto C = PortDef<2>{};

   } // namespace ioport
}

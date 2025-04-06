#pragma once

#include <cstdint>
#include <avr/io.h>

#undef PinDef

namespace asx
{
   // Provide own traits to remove need for stdlib
   namespace aux
   {
      template <typename Base, typename Derived>
      concept is_base_of = requires(Derived *d) {
         { static_cast<Base *>(d) };
      };

      template <typename Base, typename Derived>
      inline constexpr bool is_base_of_v = is_base_of<Base, Derived>;

      template <class T, T V>
      struct integral_constant
      {
         using type = integral_constant;
         static constexpr T value = V;
      };

      using true_type = integral_constant<bool, true>;
      using false_type = integral_constant<bool, false>;

      template <class, class>
      struct is_same : false_type
      {
      };

      template <class T>
      struct is_same<T, T> : true_type
      {
      };

      template <typename T1, typename T2>
      inline constexpr bool is_same_v = is_same<T1, T2>::value;
   }

   namespace ioport
   {
      using port_pin_t = uint8_t;
      using mask_t = uint8_t;

      enum class dir_t : uint8_t
      {
         in = 0,
         out = 1,
         configured = 2
      };

      enum class value_t : uint8_t
      {
         low = 0,
         high = 1
      };

      struct option_t
      {
         uint8_t value;
      };

      struct pinctrl_t : option_t
      {
      };

      // CRTP Base for Scoped Options
      template <typename Derived>
      struct scoped_option_t : pinctrl_t
      {
         constexpr explicit scoped_option_t(uint8_t v) : pinctrl_t{v} {}

         // Allow implicit conversion to uint8_t for ease of use
         constexpr operator uint8_t() const { return value; }
      };

      // Specialized Scoped Options
      struct sense_t : scoped_option_t<sense_t>
      {
         constexpr explicit sense_t(uint8_t v) : scoped_option_t(v) {}
      };

      namespace sense
      {
         static constexpr sense_t interrupt_disable{PORT_ISC_INTDISABLE_gc};
         static constexpr sense_t bothedges{PORT_ISC_BOTHEDGES_gc};
         static constexpr sense_t rising{PORT_ISC_RISING_gc};
         static constexpr sense_t falling{PORT_ISC_FALLING_gc};
         static constexpr sense_t input_disabled{PORT_ISC_INPUT_DISABLE_gc};
         static constexpr sense_t level_low{PORT_ISC_LEVEL_gc};
      }

      struct invert_t : scoped_option_t<invert_t>
      {
         constexpr explicit invert_t(uint8_t v) : scoped_option_t(v) {}
      };

      namespace invert
      {
         static constexpr invert_t normal{0};
         static constexpr invert_t inverted{PORT_INVEN_bm};
      }

      struct pullup_t : scoped_option_t<pullup_t>
      {
         constexpr explicit pullup_t(uint8_t v) : scoped_option_t(v) {}
      };

      namespace pullup
      {
         static constexpr pullup_t disabled{0};
         static constexpr pullup_t enabled{PORT_PULLUPEN_bm};
      }

      enum class slewrate_limit : uint8_t
      {
         disabled = 0,
         enabled = 1
      };

      class Port
      {
         static constexpr auto BASE_ADDRESS  = uintptr_t{0x400};
         static constexpr auto VBASE_ADDRESS = uintptr_t{0};

         static inline constexpr auto PORT_OFFSET  = uint8_t{0x20};
         static inline constexpr auto VPORT_OFFSET = uint8_t{0x04};

         ///< Port number
         uint8_t port;

      public:
         constexpr Port(const uint8_t _port) : port{_port} {}

#ifdef SIM
         PORT_t *base() const
         {
            return reinterpret_cast<PORT_t *>(SIM_OFFSET(port * PORT_OFFSET));
         }

         VPORT_t *vbase() const
         {
            return reinterpret_cast<VPORT_t *>(SIM_OFFSET(port * VPORT_OFFSET));
         }
#else
         constexpr PORT_t *base() const
         {
            return reinterpret_cast<PORT_t *>(BASE_ADDRESS + (port * PORT_OFFSET));
         }

         constexpr VPORT_t *vbase() const
         {
            return reinterpret_cast<VPORT_t *>(VBASE_ADDRESS + (index() * VPORT_OFFSET));
         }
#endif

         constexpr uint8_t index() const
         {
            return port;
         }

         void set_slewrate(const slewrate_limit sr)
         {
            if (sr == slewrate_limit::enabled)
               base()->PORTCTRL |= 1;
            else
               base()->PORTCTRL &= ~1;
         }

         constexpr bool operator==(const Port &p)
         {
            return &(p.port) == &port;
         }

         constexpr uint8_t operator*() const
         {
            return port;
         }
      };

      // Actual ports
      constexpr auto A = Port{0};
      constexpr auto B = Port{1};
      constexpr auto C = Port{2};

      // Extract
      template <typename Target, typename First, typename... Rest>
      constexpr Target extract_argument(First first, Rest... rest)
      {
         if constexpr (aux::is_same_v<First, Target>)
         {
            return first; // Found the value
         }
         else
         {
            return extract_argument<Target>(rest...); // Recurse
         }
      }

      // Pin object holding a value
      class PinDef
      {
      protected:
         port_pin_t port_pin;

      public:
         constexpr PinDef(const Port port, const uint8_t pin) : port_pin((port.index() * 8U) + pin) {}

         inline constexpr Port port() const
         {
            uint8_t index = port_pin >> 3;
            return Port{index};
         }

#ifdef SIM
         inline PORT_t *base() const
         {
            return port().base();
         }

         inline VPORT_t *vbase() const
         {
            return port().vbase();
         }
#else
         inline constexpr PORT_t *base() const
         {
            return port().base();
         }

         inline constexpr VPORT_t *vbase() const
         {
            return port().vbase();
         }
#endif

         inline constexpr mask_t mask() const
         {
            return 1U << (port_pin & 0x07);
         }
      };

      // Pin object holding a value
      class Pin : public PinDef
      {
         template <typename Target, typename First, typename... Rest>
         constexpr Target extract_argument(First first, Rest... rest)
         {
            if constexpr (aux::is_same_v<First, Target>)
            {
               return first; // Found the value
            }
            else
            {
               return extract_argument<Target>(rest...); // Recurse
            }
         }

      public:
         inline Pin &set_output()
         {
            vbase()->OUT |= mask();

            return *this;
         }

         template <typename... T>
         inline constexpr Pin &init(T... args)
         {
            constexpr bool has_value = (aux::is_same_v<T, value_t> || ...);

            if constexpr (has_value)
            {
               set(extract_argument<value_t>(args...));
            }

            constexpr bool has_dir = (aux::is_same_v<T, dir_t> || ...);

            if constexpr (has_dir)
            {
               set_dir(extract_argument<dir_t>(args...));
            }

            // Compute the PINCTRL register value
            uint8_t pinctrl_value = compute_pinctrl();

            if (pinctrl_value != 0)
            {
               register8_t *pinctrl = &(base()->PIN0CTRL) + (port_pin & 0x07);
               *pinctrl = pinctrl_value;
            }

            return *this;
         }

         inline auto operator*() -> bool
         {
            return vbase()->IN & mask();
         }


         inline auto set(const bool value = true) -> void
         {
            if (value)
            {
               vbase()->OUT |= mask();
            }
            else
            {
               vbase()->OUT &= ~mask();
            }
         }

         inline auto set(const value_t value) -> void
         {
            if (value == value_t::high)
            {
               vbase()->OUT |= mask();
            }
            else
            {
               vbase()->OUT &= ~mask();
            }
         }

         inline auto set_dir(const dir_t dir) -> void
         {
            if (dir == dir_t::in)
            {
               vbase()->DIR &= ~mask();
            }
            else
            {
               vbase()->DIR |= mask();
            }
         }


         inline auto clear() -> void
         {
            vbase()->OUT &= ~mask();
         }

         inline auto toggle() -> void
         {
            vbase()->OUT ^= mask();
         }

      private:
         // Compute PINCTRL register value by summing options that inherit from pinctrl_t
         template <typename... OPTS>
         static constexpr uint8_t compute_pinctrl()
         {
            uint8_t result = 0;
            ((result |= static_cast<uint8_t>(aux::is_base_of_v<pinctrl_t, OPTS> ? OPTS::value : 0)), ...);
            return result;
         }
      };
   } // End of ioport namespace
}

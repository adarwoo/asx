#pragma once
/**
 * @file
 * C++ Reactor API declaration
 * @addtogroup service
 * @{
 * @addtogroup reactor
 * @{
 *****************************************************************************
 * TMP Reactor API.
 * The template version allow using functions taking arguments in the reactor
 *  and passing the argument from the reactor.
 * Additional timer service is added to the reactors
 * @author software@arreckx.com
 */
#include <reactor.h>
#include <trace.h>

#include <asx/timer.hpp>

namespace asx {
   namespace reactor
   {
      enum prio : uint8_t {
         low,
         high
      };

      ///< Shortcut for the C++ handle
      using Handler = reactor_handler_t;

      ///< Null handle for C++
      constexpr auto null = reactor_handle_t{255};

      ///< Wrap into a class
      class Handle {
         reactor_handle_t handle;
         using clock = timer::steady_clock;
      public:
         // Constructor to initialize handle
         explicit Handle() : handle(null) {}

         // Constructor to initialize handle
         Handle(reactor_handle_t h) : handle(h) {}

         Handle(const Handle& h) = default;

         // Assignment operator from another Handle
         Handle& operator=(const Handle& other) = default;

         // Assignment operator from reactor_handle_t
         Handle& operator=(reactor_handle_t h) {
            handle = h;
            return *this;
         }

         // Cast operator to reactor_handle_t
         operator reactor_handle_t() const {
            return handle;
         }

         // Notify no arg
         void notify() {
            reactor_notify(handle, nullptr);
         }

         void operator()() {
            reactor_notify(handle, nullptr);
         }

         template <typename T>
         inline void operator()(T arg) {
            reactor_notify(handle, reinterpret_cast<void*>(static_cast<uintptr_t>(arg)));
         }

         // Notify function for one argument
         template <typename T>
         inline void notify(T arg) {
            reactor_notify(handle, reinterpret_cast<void*>(static_cast<uintptr_t>(arg)));
         }

         // Notify function for two arguments, packing them into a single 32-bit value
         template <typename T1, typename T2>
         inline void notify(T1 arg1, T2 arg2) {
            uint32_t packed = pack(static_cast<uint8_t>(arg1), static_cast<uint8_t>(arg2));
            reactor_notify(handle, reinterpret_cast<void*>(static_cast<uintptr_t>(packed)));
         }

         // Invoke (direct call of the handler) no arg
         void invoke() {
            reactor_invoke(handle, nullptr);
         }

         // Notify function for one argument
         template <typename T>
         inline void invoke(T arg) {
            reactor_invoke(handle, reinterpret_cast<void*>(static_cast<uintptr_t>(arg)));
         }

         // Notify function for two arguments, packing them into a single 32-bit value
         template <typename T1, typename T2>
         inline void invoke(T1 arg1, T2 arg2) {
            uint32_t packed = pack(static_cast<uint8_t>(arg1), static_cast<uint8_t>(arg2));
            reactor_invoke(handle, reinterpret_cast<void*>(static_cast<uintptr_t>(packed)));
         }

         // Notify in the future
         inline timer::Instance delay(timer::duration after) {
            return timer_arm(handle, clock::to_timer_count(clock::now() + after), 0, nullptr);
         }

         // Notify in the future
         inline timer::Instance repeat(timer::duration after, timer::duration repeat) {
            return timer_arm(
               handle,
               clock::to_timer_count(clock::now() + after),
               clock::to_timer_count(repeat),
               nullptr
            );
         }

         /**
          * Start repeating invocations after 'repeat'
          */
         inline timer::Instance repeat(timer::duration repeat) {
            return timer_arm(
               handle,
               clock::to_timer_count(clock::now() + repeat),
               clock::to_timer_count(repeat),
               nullptr
            );
         }

         // Notify in the future
         inline timer::Instance delay(timer::time_point at)
         {
            return timer_arm(handle, clock::to_timer_count(at), 0, nullptr);
         }

         // Notify in the future
         inline timer::Instance repeat(timer::time_point at, timer::duration repeat)
         {
            return timer_arm(
               handle,
               clock::to_timer_count(at),
               clock::to_timer_count(repeat),
               nullptr
            );
         }

         inline void clear() {
            reactor_clear( 1UL << handle );
         }

         template <typename T>
         inline timer::Instance delay(
            timer::duration after, T arg)
         {
            return timer_arm(
               handle,
               clock::to_timer_count(clock::now() + after),
               0,
               reinterpret_cast<void*>(static_cast<uintptr_t>(arg))
            );
         }

         template <typename T>
         inline timer::Instance repeat(
            timer::duration after, timer::duration repeat, T arg)
         {
            return timer_arm(
               handle,
               clock::to_timer_count(clock::now() + after),
               clock::to_timer_count(repeat),
               reinterpret_cast<void*>(static_cast<uintptr_t>(arg))
            );
         }

      private:
         // Helper function to pack two 16-bit values into a single 32-bit integer
         constexpr uint16_t pack(uint8_t a, uint8_t b) {
            return (static_cast<uint16_t>(a) << 8) | static_cast<uint16_t>(b);
         }
      };

      ///< Shortcut for the C++ mask
      class Mask {
         reactor_mask_t mask;

      public:         
         // Constructor to initialize handle
         explicit Mask() : mask(0) {}

         // Constructor to initialize handle
         Mask(reactor_mask_t h) : mask(h) {}

         // Cast operator to reactor_handle_t
         operator reactor_mask_t() const {
            return mask;
         }

         /// @brief Get the highest prio handler and remove from the mask
         /// @return A matching handle object which could be reactor::null
         Handle pop() {
            return Handle(reactor_mask_pop(&mask));
         }

         /// @brief Append another reactor
         /// @param h A reactor handle
         void append(const Handle h) {
            mask |= reactor_mask_of(h);
         }

         /// @brief Append another reactor
         /// @param h A reactor handle
         void append(const Mask m) {
            mask |= m.mask;
         }
      };

      template <typename Func>
      static constexpr auto bind(Func func, prio p = prio::low) -> Handle {
         // Convert the func to a void(void)
         using vv = void(*)();
         auto pv = (vv)func;

         return Handle(
            reactor_register(
               (reactor_handler_t)(pv),
               (reactor_priority_t)p
            )
         );
      }

      template <typename... Args>
      static constexpr Mask mask_of(Handle a, Args... args) {
         reactor_mask_t m = reactor_mask_of(a);

         // Use a fold expression to OR m with the masks of the remaining handles
         ((m |= reactor_mask_of(args)), ...);

         return Mask(m);
      }

      static inline void clear(Mask m) { reactor_clear(m); }

      static inline void notify_from_isr(Handle on_xx) { reactor_null_notify_from_isr(on_xx); }

      // Yield - no arg
      inline void yield() {
         reactor_yield(NULL);
      }

      // Yield - single arg
      template <typename T>
      inline void yield(T arg) {
         reactor_yield(reinterpret_cast<void*>(static_cast<uintptr_t>(arg)));
      }

      // Yield 2 args
      template <typename T1, typename T2>
      inline void yield(T1 arg1, T2 arg2) {
         // Helper function to pack two 16-bit values into a single 32-bit integer
         constexpr auto pack = [](uint8_t a, uint8_t b) -> uint16_t {
            return (static_cast<uint16_t>(a) << 8) | static_cast<uint16_t>(b);
         };

         uint32_t packed = pack(static_cast<uint8_t>(arg1), static_cast<uint8_t>(arg2));
         reactor_yield(reinterpret_cast<void*>(static_cast<uintptr_t>(packed)));
      }


      static inline void run() { reactor_run(); }
   }
} // End of namespace asx
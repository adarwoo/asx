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
#include <type_traits>
#include <functional>

#include <reactor.h>

#include <asx/timer.hpp>

namespace asx {
   namespace reactor
   {
      enum prio : uint8_t {
         low,
         high
      };

      namespace detail {
         template <typename T>
         concept EightBit = sizeof(std::remove_cvref_t<T>) == 1;

         template <EightBit A, EightBit B>
         constexpr void* pack(A a, B b) {
             uint16_t result = (static_cast<uint16_t>(b) << 8) | static_cast<uint16_t>(a);
             return reinterpret_cast<void*>(static_cast<uintptr_t>(result));
         }

         template <EightBit A, EightBit B>
         constexpr void unpack(void *_packed, A& out_a, B& out_b) {
            uint16_t packed = reinterpret_cast<uint16_t>(_packed);
            out_a = static_cast<A>(packed & 0xFF);
            out_b = static_cast<B>((packed >> 8) & 0xFF);
         }

         //
         // Helper type trait to check the number of arguments and access argument types
         //
         template <typename T>
         struct function_traits;

         template <typename Ret, typename... Args>
         struct function_traits<Ret(Args...)> {
            static constexpr std::size_t arity = sizeof...(Args);

            template <std::size_t N>
            struct arg {
               static_assert(N < arity, "Index out of bounds");
               using type = typename std::tuple_element<N, std::tuple<Args...>>::type;
            };
         };

         template <typename Ret, typename... Args>
         struct function_traits<Ret(*)(Args...)> : function_traits<Ret(Args...)> {};

         template <typename Ret, typename... Args>
         struct function_traits<Ret(&)(Args...)> : function_traits<Ret(Args...)> {};

         template <typename ClassType, typename Ret, typename... Args>
         struct function_traits<Ret(ClassType::*)(Args...)> : function_traits<Ret(Args...)> {};

         template <typename ClassType, typename Ret, typename... Args>
         struct function_traits<Ret(ClassType::*)(Args...) const> : function_traits<Ret(Args...)> {};

         template <typename F>
         struct function_traits : function_traits<decltype(&F::operator())> {};

         // Trampoline template for functions with 2 arguments
         //
         // On the AVR, a  void (*)(void*) pointer is passed in registers R24:R25.
         // BUT, a function like void func(uint8_t, uint8_t) uses R24 and R22, not packed.
         // This difference is critical on AVR — the trampoline must call the actual function
         //  with two 8-bit arguments, not just reinterpret the function pointer.
         template <typename Func>
         struct Trampoline {
            static Func func;

            static void call(void* context) {
               uint8_t a, b;
               unpack(context, a, b);

               func(
                  static_cast<typename function_traits<Func>::template arg<0>::type>(a),
                  static_cast<typename function_traits<Func>::template arg<1>::type>(b)
               );
            }
         };

         // Create instances of the trampoline for each function type
         template <typename Func>
         Func Trampoline<Func>::func;
      }

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
            reactor_notify(handle, detail::pack(arg1, arg2));
         }

         /**
          * @brief Invoke the handler directly
          * @details This is a direct call to the handler, bypassing the reactor.
          * It then treats the handler like a regular function pointer.
          * Use invoke when using Mask in your application.
          */
         void invoke() {
            reactor_invoke(handle, nullptr);
         }

         /**
          * @brief Invoke the handler directly with a single argument
          * @see invoke()
          */
         template <typename T>
         inline void invoke(T arg) {
            reactor_invoke(handle, static_cast<uintptr_t>(arg));
         }

         /**
          * @brief Invoke the handler directly with a 2 arguments
          * Both arguments are packed into a single 16-bit value, therefore each argument must be
          * 8-bit long.
          * @see invoke()
          */
         template <typename T1, typename T2>
         inline void invoke(T1 arg1, T2 arg2) {
            reactor_invoke(handle, detail::pack(arg1, arg2));
         }

         /**
          * @brief Invoke the reactor handler after a delay from now
          * @param after The time to wait before invoking the handler
          * @return The underlying running timer instance. This is required to cancel the timer.
          * @details A timer is armed with the given time and will invoke the handler
          * after the given time.
          * @note Do not use this function inside an interrupt context.
          */
         inline timer::Instance delay(timer::duration after) {
            return timer_arm(handle, clock::to_timer_count(clock::now() + after), 0, nullptr);
         }

         /**
          * @brief Repeatingly invoke the reactor handler after a delay from now. This version
          * all controlling the initial delay and the repeat time.
          * @param after The time to wait before invoking the handler
          * @param repeat The time to wait before invoking the handler again
          * @return The underlying running timer instance. This is required to cancel the timer.
          * @note Do not use this function inside an interrupt context.
          * @see delay()
          */
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
          * @note Do not use this function inside an interrupt context.
          */
         inline timer::Instance repeat(timer::duration repeat) {
            return timer_arm(
               handle,
               clock::to_timer_count(clock::now() + repeat),
               clock::to_timer_count(repeat),
               nullptr
            );
         }

         /**
          * Invoke the handler at a specific time in the future
          * @param at The timepoint when the handler is notified
          * @note If the time is expired already, the handler is invoked in the next millisecond.
          * @note Do not use this function inside an interrupt context.
          */
         inline timer::Instance delay(timer::time_point at) {
            return timer_arm(handle, clock::to_timer_count(at), 0, nullptr);
         }

         /**
          * Invoke the handler after a delay from now. This version allow passing a single
          * argument to the handler.
          * The argument must be castable to a uintptr_t (no more than 16bits).
          * @param at The timepoint when the handler is notified
          * @param arg The argument to pass to the handler
          * @note If the time is expired already, the handler is invoked in the next millisecond.
          * @note Do not use this function inside an interrupt context.
          */
         template <typename T>
         inline timer::Instance delay(
            timer::duration after, T arg) {
            return timer_arm(
               handle,
               clock::to_timer_count(clock::now() + after),
               0,
               reinterpret_cast<void*>(static_cast<uintptr_t>(arg))
            );
         }

         /**
          * @brief Repeatingly invoke the handler at a given interval,
          *         starting at a specific time in the future.
          * @param at The timepoint when the handler is notified
          * @param repeat The time to wait before invoking the handler again
          * @return The underlying running timer instance. This is required to cancel the timer.
          * @note If the time is expired already, the handler is invoked in the next millisecond.
          * @note Do not use this function inside an interrupt context.
          */
         inline timer::Instance repeat(timer::time_point at, timer::duration repeat)
         {
            return timer_arm(
               handle,
               clock::to_timer_count(at),
               clock::to_timer_count(repeat),
               nullptr
            );
         }

         /**
          * @brief Clear this reactor handler from the reactor
          * Allows removing a handler from the reactor. If the handler is not in the reactor, the
          * function has no effect.
          * @note Do not use this function inside an interrupt context.
          */
         inline void clear() {
            reactor_clear( 1UL << handle );
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
      };

      /**
       * @brief Reactor mask class
       * @details This class is used to manage a set of reactor handles. (up to 32)
       * It allows to create a mask of handles, append new handles to the mask,
       * and check if the mask is empty.
       * You can add reactor handles to the mask using the append() method.
       * The pop method will return the highest priority handle and remove it from the mask.
       * The is_empty() method checks if the mask is empty.
       * Unlike a queue, the mask will not expand. Items added more than once are simple added.
       * @note The mask is a bitwise representation of the handles.
       * Each handle is represented by a single bit in the mask.
       * @example
       * reactor::Mask m;
       * // h2 has a higher priority than h1 as low priority handlers are packed from the bottom.
       * // The order of registration is important for the priority.
       * reactor::Handle h1 = reactor::bind([](){...});
       * reactor::Handle h2 = reactor::bind([](){...});
       * m.append(h1);
       * m.append(h2);
       * while (not m.is_empty()) {
       *    m.pop().invoke(); // Directly call the handler
       * }
       */
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

         /// @brief Append a reactor handle to the mask
         /// @param h A reactor handle
         void append(const Handle h) {
            mask |= reactor_mask_of(h);
         }

         /// @brief Combine the handles from another mask
         /// @param m A reactor mask
         void append(const Mask m) {
            mask |= m.mask;
         }

         /// @brief Check pending handles
         bool is_empty() {
            return mask == 0;
         }
      };

      /**
       * @brief Bind a function to the reactor
       * @return A reactor handle object
       * @details The function must be a callable object (function pointer, lambda, etc.).
       * The function will be called when its handle is notified.
       * The function must have a specific signature:
       *  - No arguments
       *  - One argument : must be castable to a uintptr_t
       *  - Two arguments : Each argument size must be 8-bit long
       * The function will be called with the arguments passed to the notify() method.
       * @warning The function cannot not be a member function of a class.
       * @warning The invoke/notify() methods do not check if the arguments passed
       * match the prototype of the handler. A mismatch will result in undefined behavior.
       * The function must be a callable object (function pointer, lambda, etc.). The function
       * will be called when the reactor is notified.
       */
      template <typename F>
      std::enable_if_t<detail::function_traits<std::decay_t<F>>::arity <= 1, Handle>
      bind(F&& func, reactor_priority_t p = prio::low) {
         using vv = void(*)();
         auto pv = (vv)func;
         return Handle(
            reactor_register(
                  reinterpret_cast<reactor_handler_t>(pv),
                  static_cast<reactor_priority_t>(p)
            )
         );
      }

      template <typename F>
      std::enable_if_t<detail::function_traits<std::decay_t<F>>::arity == 2, Handle>
      bind(F&& func, reactor_priority_t p = prio::low) {
         using Func = std::decay_t<F>;
         detail::Trampoline<Func>::func = func;
         return Handle(
            reactor_register(
                  reinterpret_cast<reactor_handler_t>(detail::Trampoline<Func>::call),
                  static_cast<reactor_priority_t>(p)
            )
         );
      }

      template <typename C, typename Ret, typename... Args>
      requires (
         sizeof...(Args) == 2
         && detail::EightBit<typename std::tuple_element<0, std::tuple<Args...>>::type>
         && detail::EightBit<typename std::tuple_element<1, std::tuple<Args...>>::type>
      )
      Handle bind(Ret (C::*method)(Args...), C* instance, reactor_priority_t p = prio::low) {
         // Create a lambda capturing the instance and method
         auto thunk = [instance, method](Args... args) {
            (instance->*method)(args...);
         };

         // Bind using existing 2-arg infrastructure (it will go through Trampoline)
         return bind(thunk, p);
      }

      /**
       * @brief Create a mask from a set of handles
       * @param a The first handle
       * @param args The remaining handles
       * @return A mask containing all the handles
       * @details This function creates a mask from a set of handles. The handles are combined
       * using a bitwise OR operation. The resulting mask can be used to notify all the handles
       * at once.
       * @note The function uses a fold expression to combine the handles.
       */
      template <typename... Args>
      static constexpr Mask mask_of(Handle a, Args... args) {
         reactor_mask_t m = reactor_mask_of(a);

         // Use a fold expression to OR m with the masks of the remaining handles
         ((m |= reactor_mask_of(args)), ...);

         return Mask(m);
      }

      static inline void clear(Mask m) {
         reactor_clear(m);
      }

      /**
       * @brief Notify a reactor handler from an interrupt context. This function is fast.
       * @param on_xx The reactor handler to notify
       * @details This function is used to notify a reactor handler from an interrupt context.
       * It is a no-op if the handler is not registered in the reactor.
       * @note This function should only be used from an interrupt context.
       * @note NULL arguments are passed to the handler
       */
      static inline void notify_from_isr(Handle on_xx) {
         reactor_null_notify_from_isr(on_xx);
      }

      /**
       * @brief yield execution to the reactor
       * @details This function is used to yield execution to the reactor.
       * It allows the reactor to process other handlers but automatically
       *  returns to the caller when the reactor is done.
       * This function does not take any arguments.
       * To keep track of the state of the handler, a local static can be used.
       */
      inline void yield() {
         reactor_yield(NULL);
      }

      /**
       * @brief yield execution to the reactor with a single argument
       * @details This function is used to yield execution to the reactor.
       * It allows the reactor to process other handlers but automatically
       *  returns to the caller when the reactor is done.
       * This function takes a single argument which is passed to the handler.
       * The argument must be castable to a uintptr_t (no more than 16bits).
       * @param arg The argument to pass to the handler
       * @note The argument can be used to determine the state of the handler.
       */
      template <typename T>
      inline void yield(T arg) {
         reactor_yield(reinterpret_cast<void*>(static_cast<uintptr_t>(arg)));
      }

      /**
       * @brief yield execution to the reactor with two arguments
       * @see yield()
       * @param arg1 The first argument to pass to the handler
       * @param arg2 The second argument to pass to the handler
       * @note The arguments must be castable to a uintptr_t (no more than 8bits).
       * @note The arguments are packed into a single 16-bit value, therefore each argument must be
       * 8-bit long.
       */
      template <typename T1, typename T2>
      inline void yield(T1 arg1, T2 arg2) {
         reactor_yield(detail::pack(arg1, arg2));
      }

      /**
       * @brief Run the reactor
       * @details This function is used to run the reactor.
       * It should only be called once.
       * This function never returns.
       */
      static inline void run() {
         reactor_run();
      }
   }
} // End of namespace asx
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
#include <type_traits>
#include <functional>

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

         // Invoke (direct call of the handler) no arg
         void invoke() {
            reactor_invoke(handle, nullptr);
         }

         // Notify function for one argument
         template <typename T>
         inline void invoke(T arg) {
            reactor_invoke(handle, static_cast<uintptr_t>(arg));
         }

         // Notify function for two arguments, packing them into a single 32-bit value
         template <typename T1, typename T2>
         inline void invoke(T1 arg1, T2 arg2) {
            reactor_invoke(handle, detail::pack(arg1, arg2));
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

         /// @brief Check pending handles
         bool is_empty() {
            return mask == 0;
         }
      };

      // Register handler function
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

      static inline void notify_from_isr(Handle on_xx) {
         reactor_null_notify_from_isr(on_xx);
      }

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
         reactor_yield(detail::pack(arg1, arg2));
      }

      static inline void run() { reactor_run(); }
   }
} // End of namespace asx
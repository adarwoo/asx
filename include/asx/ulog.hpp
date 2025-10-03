#pragma once
/**
 * @file ulog.hpp
 * @brief Ultra-lightweight logging framework for embedded systems.
 *
 * This header defines a compile-time evaluated logging system designed for
 * memory-constrained embedded environments. It embeds log metadata into custom
 * ELF sections (.logs) while emitting runtime log payloads via register-based
 * emission routines. Format strings and type signatures are preserved at
 * compile time and can be indexed offline using the binary metadata.
 *
 * Features:
 * - Zero runtime format string overhead
 * - Compile-time argument encoding and dispatch
 * - Interrupt compatible
 * - Packed binary logs with 0/1/2/4-byte payloads
 * - Auto-tagged log level and type signature
 * - Inline assembly generates .logs metadata per callsite
 * - Double buffering. First buffer stores the logs, second the messages to send
 * - Messages are sent over a UART using COBS encoding
 * - Messages are from 2 to 6 bytes long only,
 *    yeilding throughput of 1920 to 5760 messages/seconds.
 * - Small footprint with <600bytes of flash and 117bytes of RAM (for 16 messages buffer)
 *
 *
 * Usage:
 * ```cpp
 * #include "ulog.hpp"
 *
 * // Simple log, no args
 * ULOG(ulog::info, "Starting up...");
 *
 * // 1-byte payload
 * uint8_t id = 42;
 * ULOG(ulog::debug0, "Device ID: %02x", id);
 *
 * // 2 bytes, packed from two uint8_t values
 * uint8_t x = 10, y = 20;
 * ULOG(ulog::warn, "Pos: (%u,%u)", x, y);
 *
 * // 4 bytes, float packed as IEEE 754
 * float temperature = 36.7f;
 * ULOG(ulog::info, "Temp: %.1f", temperature);
 *
 * // Alternative: template call
 * using namespace ulog;
 * ulog<error>("Fatal at %u", code);
 * ```
 *
 * @note Only types up to 4 bytes total are supported per log. Format strings must be literals.
 * @note The `.logs` section can be parsed from the ELF to map runtime packets back to messages.
 * @note You are limited to 255 messages per application
 *
 * @author
 * software@arreckx.com
 */
#include <tuple>
#include <cstring>
#include <type_traits>
#include <utility>

#include <ulog.h>

namespace asx {
   namespace ulog {
      enum Level : uint8_t {
         error = 0,
         warn,
         mile,
         trace,
         info,
         debug0,
         debug1,
         debug2,
         debug3
      };

      namespace detail {
         /** Value to pack for the argument trait */
         enum class ArgTrait : uint8_t {
            none = 0,
            u8 = 0x10,
            s8 = 0x11,
            b8 = 0x12,
            u16 = 0x20,
            s16 = 0x21,
            ptr16 = 0x22,
            u32 = 0x40,
            s32 = 0x41,
            float32 = 0x42,
            str4 = 0x43
         };

         template <typename T>
         constexpr ArgTrait arg_trait() {
            using U = std::remove_cv_t<std::remove_reference_t<T>>;

            if constexpr (std::is_same_v<U, bool>)
               return ArgTrait::b8;
            else if constexpr (std::is_same_v<U, const char*> || std::is_same_v<U, char*>)
               return ArgTrait::str4;
            else if constexpr (std::is_floating_point_v<U>)
               return ArgTrait::float32;
            else if constexpr (std::is_pointer_v<U> && sizeof(U) == 2)
               return ArgTrait::ptr16;
            else if constexpr (std::is_integral_v<U>) {
               if constexpr (std::is_signed_v<U>) {
                  if constexpr (sizeof(U) == 1) return ArgTrait::s8;
                  if constexpr (sizeof(U) == 2) return ArgTrait::s16;
                  if constexpr (sizeof(U) == 4) return ArgTrait::s32;
               } else {
                  if constexpr (sizeof(U) == 1) return ArgTrait::u8;
                  if constexpr (sizeof(U) == 2) return ArgTrait::u16;
                  if constexpr (sizeof(U) == 4) return ArgTrait::u32;
               }
            }

            return ArgTrait::none;
         }

         template<typename... Ts>
         constexpr uint32_t encode_traits() {
            uint32_t result = 0;
            uint8_t i = 0;
            ((result |= static_cast<uint32_t>(arg_trait<Ts>()) << (i++ * 8)), ...);
            return result;
         }

         template<typename... Ts>
         constexpr size_t packed_sizeof() {
            return (sizeof(Ts) + ... + 0);
         }

         template <typename T>
         constexpr auto split_to_u8_tuple(T value) {
            using U = std::remove_cv_t<std::remove_reference_t<T>>;

            if constexpr (std::is_integral_v<U>) {
               if constexpr (sizeof(T) == 1) {
                  return std::make_tuple(static_cast<uint8_t>(value));
               } else if constexpr (sizeof(T) == 2) {
                  return std::make_tuple(
                        static_cast<uint8_t>(value & 0xFF),
                        static_cast<uint8_t>((value >> 8) & 0xFF)
                  );
               } else if constexpr (sizeof(T) == 4) {
                  return std::make_tuple(
                        static_cast<uint8_t>(value & 0xFF),
                        static_cast<uint8_t>((value >> 8) & 0xFF),
                        static_cast<uint8_t>((value >> 16) & 0xFF),
                        static_cast<uint8_t>((value >> 24) & 0xFF)
                  );
               } else {
                  static_assert(0, "Unsupported integer size");
               }
            } else if constexpr (std::is_same_v<U, float>) {
               static_assert(sizeof(float) == 4, "Unexpected float size");
               union {
                  float f;
                  uint8_t bytes[4];
               } conv = { value };

               return std::make_tuple(conv.bytes[0], conv.bytes[1], conv.bytes[2], conv.bytes[3]);
            } else if constexpr (std::is_same_v<U, const char*> || std::is_same_v<U, char*>) {
               // We could read beyond the string - but that's OK, the display will fix it for us
               return std::make_tuple(
                  static_cast<uint8_t>(value[0]),
                  static_cast<uint8_t>(value[1]),
                  static_cast<uint8_t>(value[2]),
                  static_cast<uint8_t>(value[3])
               );
            } else {
               static_assert(0, "Unsupported type for packing");
            }
         }

         template <typename... Args>
         constexpr auto pack_bytes_to_tuple(Args&&... args) {
            static_assert((... && (
               std::is_integral_v<std::remove_reference_t<Args>> ||
               std::is_same_v<std::remove_reference_t<Args>, float>
            )), "Only integral or float arguments are supported");

            return std::tuple_cat(split_to_u8_tuple(std::forward<Args>(args))...);
         }
      }
   }
}


#define ULOG(level, fmt, ...)                                                 \
do {                                                                          \
   constexpr uint8_t _level = static_cast<uint8_t>(level);                    \
   [&]<typename... Args>(Args&&... args) {                                    \
      constexpr uint32_t _typecode = ::asx::ulog::detail::encode_traits<Args...>();\
      auto values = ::asx::ulog::detail::pack_bytes_to_tuple(args...);        \
      constexpr size_t _nbytes = std::tuple_size<decltype(values)>::value;    \
      uint8_t id;                                                             \
      asm volatile(                                                           \
         ".pushsection .logs,\"\",@progbits\n\t"                              \
         ".balign 256\n\t"                                                    \
         "1:\n\t"                                                             \
         ".byte %0\n\t"                                                       \
         ".long %1\n\t"                                                       \
         ".long %2\n\t"                                                       \
         ".asciz \"" __FILE__"\"\n\t"                                         \
         ".asciz \"" fmt "\"\n\t"                                             \
         ".popsection\n\t"                                                    \
         :: "i"(_level), "i"(__LINE__), "i"(_typecode)                        \
      );                                                                      \
      if constexpr (_nbytes == 0) {                                           \
         asm volatile(                                                        \
            "ldi r24, hi8(1b)\n\t"                                            \
            "call ulog_detail_enqueue\n\t"                                    \
            ::: "r24"                                                         \
         );                                                                   \
      } else if constexpr (_nbytes == 1) {                                    \
         auto&& value = std::get<0>(values);                                  \
         asm volatile(                                                        \
            "ldi r24, hi8(1b)\n\t"                                            \
            "push r22\n\t"                                                    \
            "mov r22, %[value]\n\t"                                           \
            "call ulog_detail_enqueue_1\n\t"                                  \
            "pop r22\n\t"                                                    \
            :: [value] "r"(value)                                             \
            : "r24", "r22"                                                    \
         );                                                                   \
      } else if constexpr (_nbytes == 2) {                                    \
         auto&& b0 = std::get<0>(values);                                     \
         auto&& b1 = std::get<1>(values);                                     \
         asm volatile(                                                        \
            "ldi r24, hi8(1b)\n\t"                                            \
            "push r22\n\t"                                                    \
            "push r23\n\t"                                                    \
            "mov r22, %[b0]\n\t"                                              \
            "mov r23, %[b1]\n\t"                                              \
            "call ulog_detail_enqueue_2\n\t"                                  \
            "pop r23\n\t"                                                     \
            "pop r22\n\t"                                                     \
            :: [b0] "r"(b0), [b1] "r"(b1)                                     \
            : "r24", "r23", "r22"                                             \
         );                                                                   \
      } else if constexpr (_nbytes == 3) {                                    \
         auto&& b0 = std::get<0>(values);                                     \
         auto&& b1 = std::get<1>(values);                                     \
         auto&& b2 = std::get<2>(values);                                     \
         asm volatile(                                                        \
            "ldi r24, hi8(1b)\n\t"                                            \
            "push r20\n\t"                                                    \
            "push r22\n\t"                                                    \
            "push r23\n\t"                                                    \
            "mov r22, %[b0]\n\t"                                              \
            "mov r23, %[b1]\n\t"                                              \
            "mov r20, %[b2]\n\t"                                              \
            "call ulog_detail_enqueue_3\n\t"                                  \
            "pop r23\n\t"                                                     \
            "pop r22\n\t"                                                     \
            "pop r20\n\t"                                                     \
            :: [b0] "r"(b0), [b1] "r"(b1), [b2] "r"(b2)                       \
            : "r24", "r22", "r23", "r20"                                      \
         );                                                                   \
      } else if constexpr (_nbytes == 4) {                                    \
         auto&& b0 = std::get<0>(values);                                     \
         auto&& b1 = std::get<1>(values);                                     \
         auto&& b2 = std::get<2>(values);                                     \
         auto&& b3 = (_nbytes > 3) ? std::get<3>(values) : 0;                 \
         asm volatile(                                                        \
            "ldi r24, hi8(1b)\n\t"                                            \
            "push r20\n\t"                                                    \
            "push r21\n\t"                                                    \
            "push r22\n\t"                                                    \
            "push r23\n\t"                                                    \
            "mov r20, %[b0]\n\t"                                              \
            "mov r21, %[b1]\n\t"                                              \
            "mov r22, %[b2]\n\t"                                              \
            "mov r23, %[b3]\n\t"                                              \
            "call ulog_detail_enqueue_4\n\t"                                  \
            "pop r23\n\t"                                                     \
            "pop r22\n\t"                                                     \
            "pop r21\n\t"                                                     \
            "pop r20\n\t"                                                     \
            :: [b0] "r"(b0), [b1] "r"(b1), [b2] "r"(b2), [b3] "r"(b3)         \
            : "r24", "r20", "r21", "r22", "r23"                               \
         );                                                                   \
      }                                                                       \
   }(__VA_ARGS__);                                                            \
} while(0)

// MACRO Compatible levels
#define ULOG_LEVEL_ERROR    0
#define ULOG_LEVEL_WARN     1
#define ULOG_LEVEL_MILE     2
#define ULOG_LEVEL_TRACE    3
#define ULOG_LEVEL_INFO     4
#define ULOG_LEVEL_DEBUG0   5
#define ULOG_LEVEL_DEBUG1   6
#define ULOG_LEVEL_DEBUG2   7
#define ULOG_LEVEL_DEBUG3   8

// Include the project trace config
#ifdef HAS_ULOG_CONFIG_FILE
#  include "conf_ulog.h"
#endif

#ifndef ULOG_LEVEL
#  ifdef NDEBUG
#     define ULOG_LEVEL ULOG_LEVEL_MILE
#  else
#     define ULOG_LEVEL ULOG_LEVEL_INFO
#  endif
#endif

#if ULOG_LEVEL >= ULOG_LEVEL_ERROR
  #define ULOG_ERROR(text, ...) ULOG(ULOG_LEVEL_ERROR, text, ##__VA_ARGS__)
#else
  #define ULOG_ERROR(text, ...) do {} while (0)
#endif

#if ULOG_LEVEL >= ULOG_LEVEL_WARN
  #define ULOG_WARN(text, ...) ULOG(ULOG_LEVEL_WARN, text, ##__VA_ARGS__)
#else
  #define ULOG_WARN(text, ...) do {} while (0)
#endif

#if ULOG_LEVEL >= ULOG_LEVEL_MILE
  #define ULOG_MILE(text, ...) ULOG(ULOG_LEVEL_MILE, text, ##__VA_ARGS__)
#else
  #define ULOG_MILE(text, ...) do {} while (0)
#endif

#if ULOG_LEVEL >= ULOG_LEVEL_TRACE
  #define ULOG_TRACE(text, ...) ULOG(ULOG_LEVEL_TRACE, text, ##__VA_ARGS__)
#else
  #define ULOG_TRACE(text, ...) do {} while (0)
#endif

#if ULOG_LEVEL >= ULOG_LEVEL_INFO
  #define ULOG_INFO(text, ...) ULOG(ULOG_LEVEL_INFO, text, ##__VA_ARGS__)
#else
  #define ULOG_INFO(text, ...) do {} while (0)
#endif

#if ULOG_LEVEL >= ULOG_LEVEL_DEBUG0
  #define ULOG_DEBUG0(text, ...) ULOG(ULOG_LEVEL_DEBUG0, text, ##__VA_ARGS__)
#else
  #define ULOG_DEBUG0(text, ...) do {} while (0)
#endif

#if ULOG_LEVEL >= ULOG_LEVEL_DEBUG1
  #define ULOG_DEBUG1(text, ...) ULOG(ULOG_LEVEL_DEBUG1, text, ##__VA_ARGS__)
#else
  #define ULOG_DEBUG1(text, ...) do {} while (0)
#endif

#if ULOG_LEVEL >= ULOG_LEVEL_DEBUG2
  #define ULOG_DEBUG2(text, ...) ULOG(ULOG_LEVEL_DEBUG2, text, ##__VA_ARGS__)
#else
  #define ULOG_DEBUG2(text, ...) do {} while (0)
#endif

#if ULOG_LEVEL >= ULOG_LEVEL_DEBUG3
  #define ULOG_DEBUG3(text, ...) ULOG(ULOG_LEVEL_DEBUG3, text, ##__VA_ARGS__)
#else
  #define ULOG_DEBUG3(text, ...) do {} while (0)
#endif

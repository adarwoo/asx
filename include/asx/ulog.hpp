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
      namespace detail {
         /** Value to pack for the argument trait */
         enum class ArgTrait : uint8_t {
            none    = ULOG_TYPE_TRAIT_NONE,
            u8      = ULOG_TYPE_TRAIT_U8,
            s8      = ULOG_TYPE_TRAIT_S8,
            b8      = ULOG_TYPE_TRAIT_BOOL,
            u16     = ULOG_TYPE_TRAIT_U16,
            s16     = ULOG_TYPE_TRAIT_S16,
            ptr16   = ULOG_TYPE_TRAIT_PTR,
            u32     = ULOG_TYPE_TRAIT_U32,
            s32     = ULOG_TYPE_TRAIT_S32,
            float32 = ULOG_TYPE_TRAIT_FLOAT,
            str4    = ULOG_TYPE_TRAIT_STR4
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
      } // namespace detail
   } // namespace ulog
} // namespace asx

#define ULOG(level, fmt, ...)                                                 \
do {                                                                          \
   constexpr uint8_t _level = static_cast<uint8_t>(level);                    \
   [&]<typename... Args>(Args&&... args) {                                    \
      constexpr uint32_t _typecode = ::asx::ulog::detail::encode_traits<Args...>();\
      auto values = ::asx::ulog::detail::pack_bytes_to_tuple(args...);        \
      constexpr size_t _nbytes = std::tuple_size<decltype(values)>::value;    \
      static_assert(_nbytes <= 4, "ULOG supports up to 4 bytes of payload");  \
      register uint8_t id asm("r24");                                         \
      asm volatile(                                                           \
         ".pushsection .logs,\"\",@progbits\n\t"                              \
         ".balign 256\n\t"                                                    \
         "1:\n\t"                                                             \
         ".byte %1\n\t"                                                       \
         ".long %2\n\t"                                                       \
         ".long %3\n\t"                                                       \
         ".asciz \"" __FILE__ "\"\n\t"                                        \
         ".asciz \"" fmt "\"\n\t"                                             \
         ".popsection\n\t"                                                    \
         "ldi %0, hi8(1b)\n\t"                                                \
         : "=r" (id)                                                          \
         : "i" (_level), "i" (__LINE__), "i" (_typecode)                      \
         :                                                                    \
      );                                                                      \
      if constexpr (_nbytes == 0) {                                           \
         ulog_detail_enqueue(id);                                             \
      } else if constexpr (_nbytes == 1) {                                    \
         auto&& b0 = std::get<0>(values);                                     \
         ulog_detail_enqueue_1(id, b0);                                       \
      } else if constexpr (_nbytes == 2) {                                    \
         auto&& b0 = std::get<0>(values);                                     \
         auto&& b1 = std::get<1>(values);                                     \
         ulog_detail_enqueue_2(id, b0, b1);                                   \
      } else if constexpr (_nbytes == 3) {                                    \
         auto&& b0 = std::get<0>(values);                                     \
         auto&& b1 = std::get<1>(values);                                     \
         auto&& b2 = std::get<2>(values);                                     \
         ulog_detail_enqueue_3(id, b0, b1, b2);                               \
      } else if constexpr (_nbytes == 4) {                                    \
         auto&& b0 = std::get<0>(values);                                     \
         auto&& b1 = std::get<1>(values);                                     \
         auto&& b2 = std::get<2>(values);                                     \
         auto&& b3 = std::get<3>(values);                                     \
         ulog_detail_enqueue_4(id, b0, b1, b2, b3);                           \
      }                                                                       \
   }(__VA_ARGS__);                                                            \
} while(0)

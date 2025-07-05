#pragma once
/**
 * @file ulog.hpp
 * @brief 🪵 Ultra-lightweight embedded logger (ulog)
 *
 * @details
 * ulog is a zero-allocation, flash-efficient logging system purpose-built for
 * deeply embedded systems. Each log site produces a compact metadata block
 * stored in a custom `.logs` section and emits only two bytes per runtime entry:
 * an 8-bit log ID and a tightly packed binary payload.
 *
 * ## ✨ Key Features
 * - Zero-runtime format string parsing — all format info resolved at compile time
 * - Compact payload encoding (raw binary), no string formatting on target
 * - 8-bit log ID for high-density trace logging (up to 255 logs)
 * - Metadata stored in `.logs`; strings (`__FILE__`, fmt) in `.logstr`
 * - Compile-time type introspection (trait and size per argument)
 * - Works from ISRs or constrained systems (no dynamic allocation)
 * - Host-side tooling can decode logs using ELF metadata only
 *
 * ## 🧩 Design Overview
 * - Each call to `ULOG(level, "fmt", ...)` generates:
 *   - A `LogMetadata` block in `.logs`, 256-byte aligned
 *   - String literals in `.logstr`
 *   - A runtime payload of `{log_id, [binary args...]}` pushed to a ring buffer
 * - Since the logs are a in a fake segment which starts at 0x10000, and since the AVR
 *    pointers are 16-bits - they are start at 0 and are aligned on 256 bytes.
 *    So we bit shift by 8 to get a unique ID.
 *  It is not possible to get the pointer as a constexpr, and no matter what we do
 *   the linker will not pass the ID as an 8-bit (because it derives from a 16!)
 *  So making the ID 16-bits actually optimizes the code.
 *
 * ## 🧰 Toolchain Notes
 * - Requires C++20 for `constexpr` template deduction and lambdas
 * - Linker script must define logs section base
 * - Host tooling extracts and decodes `.logs` and `.logstr` for postmortem or live analysis
 *
 * ## 📌 Usage
 * @code
 *   ULOG(WARN, "Battery low: {}%", percent);
 * @endcode
 *
 * This creates a log entry with metadata (including file, line, trait info) in `.logs`,
 * while pushing only 2 bytes + 1 arg = ~3 bytes per log entry at runtime.
 *
 * ## ⚠️ Limitations
 * - Max 8 arguments per log site
 * - Max 255 unique log IDs per firmware image
 * - Type-safe only for supported traits (bool, int, float, etc.)
 *
 * @author software@arreckx.com
 */
#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>


namespace ulog
{
   enum class LogLevel : uint8_t {
      error = 0,
      warn,
      mile,
      info,
      trace,
      debug
   };

   namespace detail {
      constexpr uint8_t ALIGN_BITS = 8;

      #define ULOG_META_SECTION __attribute__((section(".logs"), aligned(256), used))
      #define ULOG_STR_SECTION __attribute__((section(".note.ulog"), used))

      enum ArgTrait : uint8_t
      {
         ARG_NONE = 0,
         ARG_UINT,
         ARG_INT,
         ARG_BOOL,
         ARG_FLOAT,
         ARG_FIXED // For fixed point types
      };

      template <typename T>
      constexpr ArgTrait arg_trait() {
         if constexpr (std::is_same_v<T, bool>)
            return ARG_BOOL;
         else if constexpr (std::is_floating_point_v<T>)
            return ARG_FLOAT;
         else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
            return ARG_INT;
         else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>)
            return ARG_UINT;
         else
            return ARG_NONE;
      }

      struct ArgInfo {
         uint8_t size;
         ArgTrait trait;
      };

      struct LogMetadata {
         const char *fmt;
         const char *file;
         uint16_t line;
         LogLevel level;
         uint8_t nargs;
         ArgInfo args[8];
      };

      // Circular buffer (stubbed here)
      constexpr int MAX_PAYLOAD = 16;
      constexpr int BUF_SIZE = 32;

      struct LogPacket {
         uint8_t len;
         uint8_t data[1 + MAX_PAYLOAD];
      };

      inline volatile uint8_t log_head = 0, log_tail = 0;
      inline LogPacket log_buffer[BUF_SIZE];

      inline uint8_t *reserve_log_packet() {
         uint8_t next = (log_head + 1) % BUF_SIZE;
         if (next == log_tail)
            return nullptr;
         uint8_t *ptr = log_buffer[log_head].data;
         log_head = next;
         return ptr;
      }

      inline void pack_args(uint8_t *) {}

      template <typename T, typename... Rest>
      inline void pack_args(uint8_t *dst, T val, Rest... rest) {
         if constexpr (sizeof(T) == 1) {
            *dst = static_cast<uint8_t>(val);
         } else if constexpr (sizeof(T) == 2) {
            uint16_t v = static_cast<uint16_t>(val);
            dst[0] = v & 0xFF;
            dst[1] = v >> 8;
         } else if constexpr (sizeof(T) == 4) {
            uint32_t v = static_cast<uint32_t>(val);
            dst[0] = v & 0xFF;
            dst[1] = v >> 8;
            dst[2] = v >> 16;
            dst[3] = v >> 24;
         } else {
            std::memcpy(dst, &val, sizeof(T));
         }

         pack_args(dst + sizeof(T), rest...);
      }

      template <size_t N>
      struct StaticStr {
         char value[N];

         constexpr StaticStr(const char (&str)[N]) {
            for (size_t i = 0; i < N; ++i)
               value[i] = str[i];
         }
      };

      template <typename... Args>
      constexpr auto make_arglist_type() {
         return std::array<ArgInfo, sizeof...(Args)>{
             ArgInfo{sizeof(Args), arg_trait<Args>()}...};
      }


      template<typename... Args>
      __attribute__((cold, noinline))
      std::enable_if_t<(sizeof...(Args) > 1), void>
      emit(uint16_t id, Args&&... args) {
         if (uint8_t* dst = reserve_log_packet()) {
            *dst++ = id;
            pack_args(dst, std::forward<Args>(args)...);
         }
      }

      __attribute__((cold, noinline))
      void emit0(uint16_t id) {
         if (uint8_t* dst = detail::reserve_log_packet())
            *dst = id;
      }

      __attribute__((cold, noinline))
      void emit(uint16_t id, uint8_t a) {
         if (uint8_t* dst = detail::reserve_log_packet()) {
            *dst++ = id;
            *dst = a;
         }
      }

      __attribute__((cold, noinline))
      void emit(uint16_t id, uint16_t a) {
         if (uint8_t* dst = detail::reserve_log_packet()) {
            *dst++ = id;
            *dst++ = static_cast<uint8_t>(a >> 0);
            *dst = static_cast<uint8_t>(a >> 8);
         }
      }
   } // namespace detail
} // namespace ulog

#define ULOG(level, fmt, ...)                                                    \
   do {                                                                          \
      static ::ulog::detail::StaticStr<sizeof(fmt)> _ulog_fmt = fmt;   \
      static const char *const ULOG_STR_SECTION _ulog_fmt_ptr = _ulog_fmt.value; \
      static const char ULOG_STR_SECTION _ulog_file[] = __FILE__;                \
      auto _traits = []<typename... Args>(Args && ...) constexpr {     \
         return ::ulog::detail::make_arglist_type<Args...>();                    \
      }(__VA_ARGS__);                                                            \
      static ::ulog::detail::LogMetadata ULOG_META_SECTION _ulog_meta = {  \
          _ulog_fmt_ptr,                                                         \
          _ulog_file,                                                            \
          __LINE__,                                                              \
          level,                                                                 \
          static_cast<uint8_t>(_traits.size()),                                  \
          {_traits.size() > 0 ? _traits[0] : ::ulog::detail::ArgInfo{},          \
           _traits.size() > 1 ? _traits[1] : ::ulog::detail::ArgInfo{},          \
           _traits.size() > 2 ? _traits[2] : ::ulog::detail::ArgInfo{},          \
           _traits.size() > 3 ? _traits[3] : ::ulog::detail::ArgInfo{},          \
           _traits.size() > 4 ? _traits[4] : ::ulog::detail::ArgInfo{},          \
           _traits.size() > 5 ? _traits[5] : ::ulog::detail::ArgInfo{},          \
           _traits.size() > 6 ? _traits[6] : ::ulog::detail::ArgInfo{},          \
           _traits.size() > 7 ? _traits[7] : ::ulog::detail::ArgInfo{}}};        \
      [&]<typename... Args>(Args&&... args) {                                    \
         auto _id = (uint16_t)((uintptr_t)&_ulog_meta >> 8); \
         if constexpr (sizeof...(Args) == 0) {                                   \
            ::ulog::detail::emit0(_id);                   \
         } else if constexpr (sizeof...(Args) == 1) {                            \
            auto&& a = std::get<0>(std::forward_as_tuple(std::forward<Args>(args)...)); \
            if constexpr (std::is_same_v<std::decay_t<decltype(a)>, uint8_t>) { \
               ::ulog::detail::emit(ULOG_STATIC_ID(_ulog_meta), a); \
            } else if constexpr (std::is_same_v<std::decay_t<decltype(a)>, uint16_t>) { \
               ::ulog::detail::emit(ULOG_STATIC_ID(_ulog_meta), a); \
            } else { \
               ::ulog::detail::emit(ULOG_STATIC_ID(_ulog_meta), std::forward<Args>(args)...); \
            } \
         } else { \
            ::ulog::detail::emit(ULOG_STATIC_ID(_ulog_meta), std::forward<Args>(args)...); \
         } \
      }(__VA_ARGS__);                                                            \
   } while (0)

#define ULOG_ERROR(...)  ULOG(::ulog::LogLevel::error, __VA_ARGS__)
#define ULOG_WARN(...) ULOG(::ulog::LogLevel::warn, __VA_ARGS__)
#define ULOG_MILE(...)  ULOG(::ulog::LogLevel::mile, __VA_ARGS__)
#define ULOG_INFO(...) ULOG(::ulog::LogLevel::info, __VA_ARGS__)
#define ULOG_TRACE(...)  ULOG(::ulog::LogLevel::trace, __VA_ARGS__)
#define ULOG_DEBUG(...) ULOG(::ulog::LogLevel::debug, __VA_ARGS__)
#pragma once
/**
 * @file ulog.h
 * @brief C-compatible Ultra-lightweight logging framework for embedded systems.
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
 * - Messages are sent over a UART using COBS encoding
 *
 * Usage:
 * ```c
 * #include "ulog.h"
 *
 * // Simple log, no args
 * ULOG(ULOG_LEVEL_INFO, "Starting up...");
 * // or
 * ULOG_INFO("Starting up...");
 * // 2 bytes, packed from two uint8_t values. Use Python f-string style {} for formatting
 * uint8_t x = 10, y = 20;
 * ULOG_WARN("Pos:", x, y); // No formatting required
 * ULOG_WARN("Pos: ({.2%},{})", x, y); // Formatting supported
 *
 * // 4 bytes, float packed as IEEE 754
 * float temperature = 36.7f;
 * ULOG_INFO("Temp: {.<4f}", temperature);
 * ```
 *
 * @note Only types up to 4 bytes total are supported per log. Format strings must be literals.
 * @note The `.logs` section can be parsed from the ELF to map runtime packets back to messages.
 * @note You are limited to 255 messages per application
 * @note This header is C-compatible. C++ users should prefer the type-safe templated version in ulog.hpp
 * @note This header requires C11 or later for _Generic support.
 *
 * @author software@arreckx.com
 */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Forwarding prototypes
// ============================================================================
void ulog_detail_enqueue(uint8_t id);
void ulog_detail_enqueue_1(uint8_t id, uint8_t v0);
void ulog_detail_enqueue_2(uint8_t id, uint8_t v0, uint8_t v1);
void ulog_detail_enqueue_3(uint8_t id, uint8_t v0, uint8_t v1, uint8_t v2);
void ulog_detail_enqueue_4(uint8_t id, uint8_t v0, uint8_t v1, uint8_t v2, uint8_t v3);


// ============================================================================
// MACRO Compatible levels
// ============================================================================
#define ULOG_LEVEL_ERROR    0
#define ULOG_LEVEL_WARN     1
#define ULOG_LEVEL_MILE     2
#define ULOG_LEVEL_INFO     3
#define ULOG_LEVEL_TRACE    4
#define ULOG_LEVEL_DEBUG0   5
#define ULOG_LEVEL_DEBUG1   6
#define ULOG_LEVEL_DEBUG2   7
#define ULOG_LEVEL_DEBUG3   8

// ============================================================================
// C type codes
// ============================================================================
#define ULOG_TRAIT_ID_NONE  0
#define ULOG_TRAIT_ID_U8    0x10
#define ULOG_TRAIT_ID_S8    0x11
#define ULOG_TRAIT_ID_BOOL  0x12
#define ULOG_TRAIT_ID_U16   0x20
#define ULOG_TRAIT_ID_S16   0x21
#define ULOG_TRAIT_ID_PTR   0x22
#define ULOG_TRAIT_ID_U32   0x40
#define ULOG_TRAIT_ID_S32   0x41
#define ULOG_TRAIT_ID_FLOAT 0x42
#define ULOG_TRAIT_ID_STR4  0x43

// ============================================================================
// Helper to generate .logs section and get ID
// ============================================================================

/**
   * @brief Generate a unique log ID based on level, format string, line number, and typecode.
   *
   * This function combines the log level, a pointer to the format string,
   * the line number, and a typecode representing the argument types into
   * a unique 8-bit identifier. The identifier is used to reference the
   * log message in the .logs section of the ELF binary.
   * The function also emits the log metadata into a dedicated .logs section
   *
   * @param level The log level (e.g., debug, info, warn, error).
   * @param fmt A pointer to the format string (must be a string literal).
   * @param line The line number where the log call is made.
   * @param typecode A 32-bit code representing the types of the arguments.
   * @return A unique 8-bit log ID.
   */
#define _ULOG_GENERATE_LOG_ID(level, fmt, typecode) \
   register uint8_t id asm("r24"); \
   asm volatile( \
      ".pushsection .logs,\"\",@progbits\n\t" \
      ".balign 256\n\t" \
      "1:\n\t" \
      ".byte %1\n\t" \
      ".long %2\n\t" \
      ".long %3\n\t" \
      ".asciz \"" __FILE__ "\"\n\t" \
      ".asciz \"" fmt "\"\n\t" \
      ".popsection\n\t" \
      "ldi %0, hi8(1b)\n\t" \
      : "=r" (id) \
      : "i" (level), "i" (__LINE__), "i" (typecode) \
      : \
   );

// This section creates the macros for C usage
// C++ users should use the templated version in ulog.hpp which is more type-safe
#ifndef __cplusplus

// ============================================================================
// Static inline runtime dispatch functions
// ============================================================================

// ----------------------------------------------------------------------------
// inline helpers to enqueue all possible combinations based on the number and types
// ----------------------------------------------------------------------------
static inline void _ulog_dispatch_0(uint8_t id) {
    ulog_detail_enqueue(id);
}

static inline void _ulog_dispatch_1_u8(uint8_t id, uint8_t a) {
    ulog_detail_enqueue_1(id, a);
}

static inline void _ulog_dispatch_1_u16(uint8_t id, uint16_t a) {
    ulog_detail_enqueue_2(id, (uint8_t)(a), (uint8_t)(a >> 8));
}

static inline void _ulog_dispatch_1_u32(uint8_t id, uint32_t a) {
    ulog_detail_enqueue_4(id,
        (uint8_t)(a),
        (uint8_t)(a >> 8),
        (uint8_t)(a >> 16),
        (uint8_t)(a >> 24));
}

static inline void _ulog_dispatch_1_float(uint8_t id, float a) {
    union { float f; uint32_t u; } conv = { .f = a };
    ulog_detail_enqueue_4(id,
        (uint8_t)(conv.u),
        (uint8_t)(conv.u >> 8),
        (uint8_t)(conv.u >> 16),
        (uint8_t)(conv.u >> 24));
}

static inline void _ulog_dispatch_2_u8_u8(uint8_t id, uint8_t a, uint8_t b) {
    ulog_detail_enqueue_2(id, a, b);
}

static inline void _ulog_dispatch_2_u8_u16(uint8_t id, uint8_t a, uint16_t b) {
    ulog_detail_enqueue_3(id, a, (uint8_t)(b), (uint8_t)(b >> 8));
}

static inline void _ulog_dispatch_2_u16_u8(uint8_t id, uint16_t a, uint8_t b) {
    ulog_detail_enqueue_3(id, (uint8_t)(a), (uint8_t)(a >> 8), b);
}

static inline void _ulog_dispatch_2_u16_u16(uint8_t id, uint16_t a, uint16_t b) {
    ulog_detail_enqueue_4(id,
        (uint8_t)(a), (uint8_t)(a >> 8),
        (uint8_t)(b), (uint8_t)(b >> 8));
}

static inline void _ulog_dispatch_3_u8_u8_u8(uint8_t id, uint8_t a, uint8_t b, uint8_t c) {
    ulog_detail_enqueue_3(id, a, b, c);
}

static inline void _ulog_dispatch_3_u8_u16(uint8_t id, uint8_t a, uint16_t b) {
    ulog_detail_enqueue_3(id, a, (uint8_t)(b), (uint8_t)(b >> 8));
}

static inline void _ulog_dispatch_3_u16_u8(uint8_t id, uint16_t a, uint8_t b) {
    ulog_detail_enqueue_3(id, (uint8_t)(a), (uint8_t)(a >> 8), b);
}

static inline void _ulog_dispatch_4_u8_u8_u8_u8(uint8_t id, uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    ulog_detail_enqueue_4(id, a, b, c, d);
}

// ============================================================================
// Compile time type code extraction helper
// Given a type x, return the corresponding ULOG_TRAIT_ID_XXX value
// ============================================================================
#define _ULOG_TC(x) _Generic((x), \
   uint8_t:  ULOG_TRAIT_ID_U8, \
   int8_t:   ULOG_TRAIT_ID_S8, \
   _Bool:    ULOG_TRAIT_ID_BOOL, \
   uint16_t: ULOG_TRAIT_ID_U16, \
   int16_t:  ULOG_TRAIT_ID_S16, \
   uint32_t: ULOG_TRAIT_ID_U32, \
   int32_t:  ULOG_TRAIT_ID_S32, \
   float:    ULOG_TRAIT_ID_FLOAT, \
   double:   ULOG_TRAIT_ID_FLOAT, \
   default:  ULOG_TRAIT_ID_NONE)

// Helper macros that generate ID first, then call the dispatch function
#define _ULOG_SELECT_0(level, fmt) \
   do { \
      _ULOG_GENERATE_LOG_ID(level, fmt, 0) \
      _ulog_dispatch_0(id); \
   } while(0)

// ============================================================================
// Simplified dispatch macros based only on argument count
// The expansion yields a compile-time evaluated typecode and calls the appropriate
// dispatcher function.
// This relies on compiler support for C11 _Generic. The final resolution happens
// at compile time, so there is no runtime overhead.
// ============================================================================

#define _ULOG_DISPATCH_0(level, fmt) \
   do { \
      _ULOG_GENERATE_LOG_ID(level, fmt, 0) \
      _ulog_dispatch_0(id); \
   } while(0)

#define _ULOG_DISPATCH_1(level, fmt, a) \
   do { \
      uint32_t typecode = _ULOG_TC(a); \
      _ULOG_GENERATE_LOG_ID(level, fmt, typecode) \
      _Generic((a), \
         uint8_t:  _ulog_dispatch_1_u8(id, a), \
         int8_t:   _ulog_dispatch_1_u8(id, a), \
         _Bool:    _ulog_dispatch_1_u8(id, a), \
         uint16_t: _ulog_dispatch_1_u16(id, a), \
         int16_t:  _ulog_dispatch_1_u16(id, a), \
         uint32_t: _ulog_dispatch_1_u32(id, a), \
         int32_t:  _ulog_dispatch_1_u32(id, a), \
         float:    _ulog_dispatch_1_float(id, a), \
         double:   _ulog_dispatch_1_float(id, a)); \
   } while(0)

#define _ULOG_DISPATCH_2(level, fmt, a, b) \
   do { \
      uint32_t typecode = (_ULOG_TC(b) << 8) | _ULOG_TC(a); \
      _ULOG_GENERATE_LOG_ID(level, fmt, typecode) \
      _Generic((a), \
         uint8_t: _Generic((b), \
            uint8_t:  _ulog_dispatch_2_u8_u8( id, a, b), \
            int8_t:   _ulog_dispatch_2_u8_u8( id, a, b), \
            _Bool:    _ulog_dispatch_2_u8_u8( id, a, b), \
            uint16_t: _ulog_dispatch_2_u8_u16(id, a, b), \
            int16_t:  _ulog_dispatch_2_u8_u16(id, a, b), \
            default:  _ulog_dispatch_2_u8_u8( id, a, b)), \
         int8_t: _Generic((b), \
            uint8_t:  _ulog_dispatch_2_u8_u8( id, a, b), \
            int8_t:   _ulog_dispatch_2_u8_u8( id, a, b), \
            _Bool:    _ulog_dispatch_2_u8_u8( id, a, b), \
            uint16_t: _ulog_dispatch_2_u8_u16(id, a, b), \
            int16_t:  _ulog_dispatch_2_u8_u16(id, a, b), \
            default:  _ulog_dispatch_2_u8_u8( id, a, b)), \
         _Bool: _Generic((b), \
            uint8_t:  _ulog_dispatch_2_u8_u8( id, a, b), \
            int8_t:   _ulog_dispatch_2_u8_u8( id, a, b), \
            _Bool:    _ulog_dispatch_2_u8_u8( id, a, b), \
            uint16_t: _ulog_dispatch_2_u8_u16(id, a, b), \
            int16_t:  _ulog_dispatch_2_u8_u16(id, a, b), \
            default:  _ulog_dispatch_2_u8_u8( id, a, b)), \
         uint16_t: _Generic((b), \
            uint8_t:  _ulog_dispatch_2_u16_u8( id, a, b), \
            int8_t:   _ulog_dispatch_2_u16_u8( id, a, b), \
            _Bool:    _ulog_dispatch_2_u16_u8( id, a, b), \
            uint16_t: _ulog_dispatch_2_u16_u16(id, a, b), \
            int16_t:  _ulog_dispatch_2_u16_u16(id, a, b), \
            default:  _ulog_dispatch_2_u16_u8( id, a, b)), \
         int16_t: _Generic((b), \
            uint8_t:  _ulog_dispatch_2_u16_u8( id, a, b), \
            int8_t:   _ulog_dispatch_2_u16_u8( id, a, b), \
            _Bool:    _ulog_dispatch_2_u16_u8( id, a, b), \
            uint16_t: _ulog_dispatch_2_u16_u16(id, a, b), \
            int16_t:  _ulog_dispatch_2_u16_u16(id, a, b), \
            default:  _ulog_dispatch_2_u16_u8( id, a, b)), \
         default: _ulog_dispatch_2_u8_u8(id, a, b)); \
   } while(0)

#define _ULOG_DISPATCH_3(level, fmt, a, b, c) \
   do { \
      uint32_t typecode = (_ULOG_TC(c) << 16) | (_ULOG_TC(b) << 8) | _ULOG_TC(a); \
      _ULOG_GENERATE_LOG_ID(level, fmt, typecode) \
      _Generic((a), \
         uint8_t: _Generic((b), \
            uint8_t: _ulog_dispatch_3_u8_u8_u8(id, a, b, c), \
            uint16_t: _ulog_dispatch_3_u8_u16(id, a, b), \
            default: _ulog_dispatch_3_u8_u8_u8(id, a, b, c)), \
         uint16_t: _ulog_dispatch_3_u16_u8(id, a, b), \
         default: _ulog_dispatch_3_u8_u8_u8(id, a, b, c)); \
   } while(0)

#define _ULOG_DISPATCH_4(level, fmt, a, b, c, d) \
   do { \
      uint32_t typecode = (_ULOG_TC(d) << 24) | (_ULOG_TC(c) << 16) | (_ULOG_TC(b) << 8) | _ULOG_TC(a); \
      _ULOG_GENERATE_LOG_ID(level, fmt, typecode) \
      _ulog_dispatch_4_u8_u8_u8_u8(id, a, b, c, d); \
   } while(0)

// Support macros to expand ... into actual dispatcher based on argument count
#define _ULOG_GET_ARG_COUNT(...) _ULOG_GET_ARG_COUNT_IMPL(0, ##__VA_ARGS__, 4, 3, 2, 1, 0)
#define _ULOG_GET_ARG_COUNT_IMPL(dummy, _1, _2, _3, _4, N, ...) N
#define _ULOG_DISPATCH(n) _ULOG_DISPATCH_IMPL(n)
#define _ULOG_DISPATCH_IMPL(n) _ULOG_DISPATCH_##n

// Main ULOG macro (C version)
#define ULOG(level, fmt, ...) \
    _ULOG_DISPATCH(_ULOG_GET_ARG_COUNT(__VA_ARGS__))(level, fmt, ##__VA_ARGS__)

#endif // End of the C-only section

// Include the project trace config (unless passed on the command line)
#if defined HAS_ULOG_CONFIG_FILE && !defined ULOG_LEVEL
#  include "conf_ulog.h"
#endif

// Default log level if not defined yet
#ifndef ULOG_LEVEL
#  ifdef NDEBUG
#     define ULOG_LEVEL ULOG_LEVEL_INFO
#  else
#     define ULOG_LEVEL ULOG_LEVEL_DEBUG3
#  endif
#endif

#if ULOG_LEVEL >= ULOG_LEVEL_ERROR
  #define ULOG_ERROR(text, ...)       ULOG(ULOG_LEVEL_ERROR, text, ##__VA_ARGS__)
#else
  #define ULOG_ERROR(text, ...)       do {} while (0)
#endif

#if ULOG_LEVEL >= ULOG_LEVEL_WARN
  #define ULOG_WARN(text, ...)       ULOG(ULOG_LEVEL_WARN, text, ##__VA_ARGS__)
#else
  #define ULOG_WARN(text, ...)       do {} while (0)
#endif

#if ULOG_LEVEL >= ULOG_LEVEL_MILE
   #define ULOG_MILE(text, ...)       ULOG(ULOG_LEVEL_MILE, text, ##__VA_ARGS__)
#else
   #define ULOG_MILE(text, ...)       do {} while (0)
#endif

#if ULOG_LEVEL >= ULOG_LEVEL_INFO
  #define ULOG_INFO(text, ...)       ULOG(ULOG_LEVEL_INFO, text, ##__VA_ARGS__)
#else
  #define ULOG_INFO(text, ...)       do {} while (0)
#endif

#if ULOG_LEVEL >= ULOG_LEVEL_TRACE
  #define ULOG_TRACE(text, ...)       ULOG(ULOG_LEVEL_TRACE, text, ##__VA_ARGS__)
#else
  #define ULOG_TRACE(text, ...)       do {} while (0)
#endif

#if ULOG_LEVEL >= ULOG_LEVEL_DEBUG0
  #define ULOG_DEBUG0(text, ...)      ULOG(ULOG_LEVEL_DEBUG0, text, ##__VA_ARGS__)
#else
  #define ULOG_DEBUG0(text, ...)      do {} while (0)
#endif

#if ULOG_LEVEL >= ULOG_LEVEL_DEBUG1
  #define ULOG_DEBUG1(text, ...)      ULOG(ULOG_LEVEL_DEBUG1, text, ##__VA_ARGS__)
#else
  #define ULOG_DEBUG1(text, ...)      do {} while (0)
#endif

#if ULOG_LEVEL >= ULOG_LEVEL_DEBUG2
  #define ULOG_DEBUG2(text, ...)      ULOG(ULOG_LEVEL_DEBUG2, text, ##__VA_ARGS__)
#else
  #define ULOG_DEBUG2(text, ...)      do {} while (0)
#endif

#if ULOG_LEVEL >= ULOG_LEVEL_DEBUG3
  #define ULOG_DEBUG3(text, ...)      ULOG(ULOG_LEVEL_DEBUG3, text, ##__VA_ARGS__)
#else
  #define ULOG_DEBUG3(text, ...)      do {} while (0)
#endif

#ifdef __cplusplus
}
#endif

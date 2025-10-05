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
 * - Double buffering. First buffer stores the logs, second the messages to send
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
#define ULOG_TYPE_TRAIT_NONE  0
#define ULOG_TYPE_TRAIT_U8    0x10
#define ULOG_TYPE_TRAIT_S8    0x11
#define ULOG_TYPE_TRAIT_BOOL  0x12
#define ULOG_TYPE_TRAIT_U16   0x20
#define ULOG_TYPE_TRAIT_S16   0x21
#define ULOG_TYPE_TRAIT_PTR   0x22
#define ULOG_TYPE_TRAIT_U32   0x40
#define ULOG_TYPE_TRAIT_S32   0x41
#define ULOG_TYPE_TRAIT_FLOAT 0x42
#define ULOG_TYPE_TRAIT_STR4  0x43

// This section creates the macros for C usage
// C++ users should use the templated version in ulog.hpp which is more type-safe
#ifndef __cplusplus

// Macro to map types -> ArgTrait
#define _ULOG_ARG_TRAIT(x) _Generic((x),     \
    bool:      ULOG_TYPE_TRAIT_BOOL,   \
    char*:     ULOG_TYPE_TRAIT_STR4,   \
    const char*: ULOG_TYPE_TRAIT_STR4, \
    float:     ULOG_TYPE_TRAIT_FLOAT,  \
    double:    ULOG_TYPE_TRAIT_FLOAT,  \
    int8_t:    ULOG_TYPE_TRAIT_S8,     \
    int16_t:   ULOG_TYPE_TRAIT_S16,    \
    int32_t:   ULOG_TYPE_TRAIT_S32,    \
    int64_t:   ULOG_TYPE_TRAIT_S64,    \
    uint8_t:   ULOG_TYPE_TRAIT_U8,     \
    uint16_t:  ULOG_TYPE_TRAIT_U16,    \
    uint32_t:  ULOG_TYPE_TRAIT_U32,    \
    uint64_t:  ULOG_TYPE_TRAIT_U64,    \
    default:   ULOG_TYPE_TRAIT_NONE    \
)

#define _ULOG_TRAIT_SIZE(trait) \
    ((trait)==ULOG_TYPE_TRAIT_U8    ? 1 : \
     (trait)==ULOG_TYPE_TRAIT_S8    ? 1 : \
     (trait)==ULOG_TYPE_TRAIT_BOOL  ? 1 : \
     (trait)==ULOG_TYPE_TRAIT_U16   ? 2 : \
     (trait)==ULOG_TYPE_TRAIT_S16   ? 2 : \
     (trait)==ULOG_TYPE_TRAIT_PTR   ? 2 : \
     (trait)==ULOG_TYPE_TRAIT_U32   ? 4 : \
     (trait)==ULOG_TYPE_TRAIT_S32   ? 4 : \
     (trait)==ULOG_TYPE_TRAIT_FLOAT ? 4 : \
     (trait)==ULOG_TYPE_TRAIT_STR4  ? 4 : \
     0)

// ============================================================================
// Trait folding into uint32_t signature
// ============================================================================
#define _ULOG_TRAITS_1(a) ((uint32_t)_ULOG_ARG_TRAIT(a))
#define _ULOG_TRAITS_2(a,b) (_ULOG_TRAITS_1(a) | ((uint32_t)_ULOG_ARG_TRAIT(b) << 8))
#define _ULOG_TRAITS_3(a,b,c) (_ULOG_TRAITS_2(a,b) | ((uint32_t)_ULOG_ARG_TRAIT(c) << 16))
#define _ULOG_TRAITS_4(a,b,c,d) (_ULOG_TRAITS_3(a,b,c) | ((uint32_t)_ULOG_ARG_TRAIT(d) << 24))

// ============================================================================
// Total payload size per arity
// ============================================================================
#define _ULOG_TRAITS_SIZE_1(a) (_ULOG_TRAIT_SIZE(_ULOG_ARG_TRAIT(a)))
#define _ULOG_TRAITS_SIZE_2(a,b) (_ULOG_TRAITS_SIZE_1(a) + _ULOG_TRAITS_SIZE_1(b))
#define _ULOG_TRAITS_SIZE_3(a,b,c) (_ULOG_TRAITS_SIZE_2(a,b) + _ULOG_TRAITS_SIZE_1(c))
#define _ULOG_TRAITS_SIZE_4(a,b,c,d) (_ULOG_TRAITS_SIZE_3(a,b,c) + _ULOG_TRAITS_SIZE_1(d))

// ============================================================================
// Argmument counting helper macros
// ============================================================================
#define _ULOG_OVERLOAD(_0, _1, _2, _3, NAME, ...) NAME
#define _ULOG_CHOOSER(...) _ULOG_OVERLOAD(__VA_ARGS__, _ULOG3, _ULOG2, _ULOG1, _ULOG0)

// ============================================================================
// Final ULOG_ENQ_* macros for all combinations
// ============================================================================

// 0 bytes total
#define ULOG_ENQ_0(level, typecode, fmt) \
    do { \
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
        ); \
        ulog_detail_enqueue(id); \
    } while(0)

// 1 byte total - single uint8_t
#define ULOG_ENQ_1(level, typecode, fmt, b0) \
    do { \
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
        ); \
        ulog_detail_enqueue_1(id, b0); \
    } while(0)

// 2 bytes total - two uint8_t
#define ULOG_ENQ_1_1(level, typecode, fmt, b0, b1) \
    do { \
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
        ); \
        ulog_detail_enqueue_2(id, b0, b1); \
    } while(0)

// 2 bytes total - single uint16_t (split into bytes)
#define ULOG_ENQ_2(level, typecode, fmt, w0) \
    do { \
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
        ); \
        ulog_detail_enqueue_2(id, (uint8_t)(w0), (uint8_t)(w0 >> 8)); \
    } while(0)

// 3 bytes total - uint8_t + uint16_t
#define ULOG_ENQ_1_2(level, typecode, fmt, b0, w0) \
    do { \
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
        ); \
        ulog_detail_enqueue_3(id, b0, (uint8_t)(w0), (uint8_t)(w0 >> 8)); \
    } while(0)

// 3 bytes total - uint16_t + uint8_t
#define ULOG_ENQ_2_1(level, typecode, fmt, w0, b0) \
    do { \
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
        ); \
        ulog_detail_enqueue_3(id, (uint8_t)(w0), (uint8_t)(w0 >> 8), b0); \
    } while(0)

// 3 bytes total - three uint8_t
#define ULOG_ENQ_1_1_1(level, typecode, fmt, b0, b1, b2) \
    do { \
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
        ); \
        ulog_detail_enqueue_3(id, b0, b1, b2); \
    } while(0)

// 4 bytes total - uint8_t, uint8_t, uint16_t
#define ULOG_ENQ_1_1_2(level, typecode, fmt, b0, b1, w0) \
   do { \
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
      ); \
      ulog_detail_enqueue_4(id, b0, b1, (uint8_t)(w0), (uint8_t)(w0 >> 8)); \
   } while(0)

// 4 bytes total - uint8_t, uint16_t, uint8_t
#define ULOG_ENQ_1_2_1(level, typecode, fmt, b0, w0, b1) \
   do { \
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
      ); \
      ulog_detail_enqueue_4(id, b0, (uint8_t)(w0), (uint8_t)(w0 >> 8), b1); \
   } while(0)

// 4 bytes total - uint16_t, uint8_t, uint8_t
#define ULOG_ENQ_2_1_1(level, typecode, fmt, w0, b0, b1) \
      do { \
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
         ); \
         ulog_detail_enqueue_4(id, (uint8_t)(w0), (uint8_t)(w0 >> 8), b0, b1); \
      } while(0)

// 4 bytes total - single uint32_t/float
#define ULOG_ENQ_4(level, typecode, fmt, d0) \
    do { \
        register uint8_t id asm("r24"); \
        union { uint32_t u; float f; } conv = {0}; \
        if ((typecode & 0xFF) == ULOG_TYPE_TRAIT_FLOAT) conv.f = d0; \
        else conv.u = d0; \
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
        ); \
        ulog_detail_enqueue_4(id, (uint8_t)(conv.u), (uint8_t)(conv.u >> 8), \
                             (uint8_t)(conv.u >> 16), (uint8_t)(conv.u >> 24)); \
    } while(0)

// 4 bytes total - two uint16_t
#define ULOG_ENQ_2_2(level, typecode, fmt, w0, w1) \
    do { \
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
        ); \
        ulog_detail_enqueue_4(id, (uint8_t)(w0), (uint8_t)(w0 >> 8), \
                             (uint8_t)(w1), (uint8_t)(w1 >> 8)); \
    } while(0)

// 4 bytes total - four uint8_t
#define ULOG_ENQ_1_1_1_1(level, typecode, fmt, b0, b1, b2, b3) \
    do { \
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
        ); \
        ulog_detail_enqueue_4(id, b0, b1, b2, b3); \
    } while(0)

// ============================================================================
// Compile-time size-based macro dispatch
// ============================================================================

// Concatenation helpers
#define _ULOG_CONCAT(a, b) a##b
#define _ULOG_CONCAT2(a, b) _ULOG_CONCAT(a, b)
#define _ULOG_CONCAT3(a, b, c) _ULOG_CONCAT2(_ULOG_CONCAT(a, b), c)
#define _ULOG_CONCAT4(a, b, c, d) _ULOG_CONCAT2(_ULOG_CONCAT3(a, b, c), d)
#define _ULOG_CONCAT5(a, b, c, d, e) _ULOG_CONCAT2(_ULOG_CONCAT4(a, b, c, d), e)

// Generate ULOG_ENQ_X_X_X macro name from argument sizes
#define _ULOG_ENQ_NAME_1(s1) \
    _ULOG_CONCAT2(ULOG_ENQ_, s1)

#define _ULOG_ENQ_NAME_2(s1, s2) \
    _ULOG_CONCAT4(ULOG_ENQ_, s1, _, s2)

#define _ULOG_ENQ_NAME_3(s1, s2, s3) \
    _ULOG_CONCAT5(ULOG_ENQ_, s1, _, s2, _, s3)

#define _ULOG_ENQ_NAME_4(s1, s2, s3, s4) \
    _ULOG_CONCAT2(_ULOG_CONCAT5(ULOG_ENQ_, s1, _, s2, _), _ULOG_CONCAT3(s3, _, s4))

// ============================================================================
// Universal ULOG macro - compile-time dispatch only
// ============================================================================

#define ULOG(level, fmt, ...) \
    _ULOG_CHOOSER(__VA_ARGS__)(level, fmt, ##__VA_ARGS__)

// 0 arguments
#define _ULOG0(level, fmt) \
    ULOG_ENQ_0(level, 0, fmt)

// 1 argument
#define _ULOG1(level, fmt, a) \
    _ULOG_ENQ_NAME_1(_ULOG_TRAIT_SIZE(_ULOG_ARG_TRAIT(a)))( \
        level, _ULOG_TRAITS_1(a), fmt, a)

// 2 arguments
#define _ULOG2(level, fmt, a, b) \
    _ULOG_ENQ_NAME_2(_ULOG_TRAIT_SIZE(_ULOG_ARG_TRAIT(a)), \
                     _ULOG_TRAIT_SIZE(_ULOG_ARG_TRAIT(b)))( \
        level, _ULOG_TRAITS_2(a, b), fmt, a, b)

// 3 arguments
#define _ULOG3(level, fmt, a, b, c) \
    _ULOG_ENQ_NAME_3(_ULOG_TRAIT_SIZE(_ULOG_ARG_TRAIT(a)), \
                     _ULOG_TRAIT_SIZE(_ULOG_ARG_TRAIT(b)), \
                     _ULOG_TRAIT_SIZE(_ULOG_ARG_TRAIT(c)))( \
        level, _ULOG_TRAITS_3(a, b, c), fmt, a, b, c)

// 4 arguments
#define _ULOG4(level, fmt, a, b, c, d) \
    _ULOG_ENQ_NAME_4(_ULOG_TRAIT_SIZE(_ULOG_ARG_TRAIT(a)), \
                     _ULOG_TRAIT_SIZE(_ULOG_ARG_TRAIT(b)), \
                     _ULOG_TRAIT_SIZE(_ULOG_ARG_TRAIT(c)), \
                     _ULOG_TRAIT_SIZE(_ULOG_ARG_TRAIT(d)))( \
        level, _ULOG_TRAITS_4(a, b, c, d), fmt, \
        a, b, c, d)

#endif // __cplusplus

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

#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


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

// Forwarding prototypes
void ulog_detail_emit0(uint8_t id);
void ulog_detail_emit8(uint8_t id, uint8_t v0);
void ulog_detail_emit16(uint8_t id, uint16_t v);
void ulog_detail_emit32(uint8_t id, uint32_t v);

#define ULOG0(_level, fmt)                                                     \
do {                                                                          \
      asm volatile(                                                           \
         ".pushsection .logs,\"\",@progbits\n\t"                              \
         ".balign 256\n\t"                                                    \
         "1:\n\t"                                                             \
         ".byte %0\n\t"                                                       \
         ".long %1\n\t"                                                       \
         ".long 0\n\t"                                                        \
         ".asciz \"" __FILE__"\"\n\t"                                         \
         ".asciz \"" fmt "\"\n\t"                                             \
         ".popsection\n\t"                                                    \
         :: "i"(_level), "i"(__LINE__)                                        \
      );                                                                      \
      asm volatile(                                                           \
         "ldi r24, hi8(1b)\n\t"                                               \
         "call ulog_detail_emit0\n\t"                                         \
         ::: "r24"                                                            \
      );                                                                      \
   } while(0)

#define ULOG8(_level, fmt, value)                                              \
do {                                                                          \
      asm volatile(                                                           \
         ".pushsection .logs,\"\",@progbits\n\t"                              \
         ".balign 256\n\t"                                                    \
         "1:\n\t"                                                             \
         ".byte %0\n\t"                                                       \
         ".long %1\n\t"                                                       \
         ".long 0x10\n\t"                                                     \
         ".asciz \"" __FILE__"\"\n\t"                                         \
         ".asciz \"" fmt "\"\n\t"                                             \
         ".popsection\n\t"                                                    \
         :: "i"(_level), "i"(__LINE__)                                        \
      );                                                                      \
      asm volatile(                                                           \
         "ldi r24, hi8(1b)\n\t"                                               \
         "mov r22, %[value]\n\t"                                              \
         "call ulog_detail_emit8\n\t"                                         \
         :: [value] "r"(value)                                                \
         : "r24", "r22"                                                       \
      );                                                                      \
   } while(0)

#define ULOG16(_level, fmt, value)                                             \
do {                                                                          \
      asm volatile(                                                           \
         ".pushsection .logs,\"\",@progbits\n\t"                              \
         ".balign 256\n\t"                                                    \
         "1:\n\t"                                                             \
         ".byte %0\n\t"                                                       \
         ".long %1\n\t"                                                       \
         ".long 0x20\n\t"                                                     \
         ".asciz \"" __FILE__"\"\n\t"                                         \
         ".asciz \"" fmt "\"\n\t"                                             \
         ".popsection\n\t"                                                    \
         :: "i"(_level), "i"(__LINE__)                                        \
      );                                                                      \
      asm volatile(                                                           \
         "ldi r24, hi8(1b)\n\t"                                               \
         "movw r22, %[value]\n\t"                                             \
         "call ulog_detail_emit16\n\t"                                        \
         :: [value] "r"(value)                                                \
         : "r24", "r23", "r22"                                                \
      );                                                                      \
   } while(0)


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
  #define ULOG0_ERROR(text)            ULOG0(ULOG_LEVEL_ERROR, text)
  #define ULOG8_ERROR(text, val)       ULOG8(ULOG_LEVEL_ERROR, text, val)
  #define ULOG16_ERROR(text, v0, v1)   ULOG16(ULOG_LEVEL_ERROR, text, v0, v1)
#else
  #define ULOG0_ERROR(text)            do {} while (0)
  #define ULOG8_ERROR(text, val)       do {} while (0)
  #define ULOG16_ERROR(text, v0, v1)   do {} while (0)
#endif

#if ULOG_LEVEL >= ULOG_LEVEL_WARN
  #define ULOG0_WARN(text)             ULOG0(ULOG_LEVEL_WARN, text)
  #define ULOG8_WARN(text, val)        ULOG8(ULOG_LEVEL_WARN, text, val)
  #define ULOG16_WARN(text, v0, v1)    ULOG16(ULOG_LEVEL_WARN, text, v0, v1)
#else
  #define ULOG0_WARN(text)             do {} while (0)
  #define ULOG8_WARN(text, val)        do {} while (0)
  #define ULOG16_WARN(text, v0, v1)    do {} while (0)
#endif

#if ULOG_LEVEL >= ULOG_LEVEL_INFO
  #define ULOG0_INFO(text)             ULOG0(ULOG_LEVEL_INFO, text)
  #define ULOG8_INFO(text, val)        ULOG8(ULOG_LEVEL_INFO, text, val)
  #define ULOG16_INFO(text, v0, v1)    ULOG16(ULOG_LEVEL_INFO, text, v0, v1)
#else
  #define ULOG0_INFO(text)             do {} while (0)
  #define ULOG8_INFO(text, val)        do {} while (0)
  #define ULOG16_INFO(text, v0, v1)    do {} while (0)
#endif

#if ULOG_LEVEL >= ULOG_LEVEL_DEBUG0
  #define ULOG0_DEBUG(text)            ULOG0(ULOG_LEVEL_DEBUG0, text)
  #define ULOG8_DEBUG(text, val)       ULOG8(ULOG_LEVEL_DEBUG0, text, val)
  #define ULOG16_DEBUG(text, v0, v1)   ULOG16(ULOG_LEVEL_DEBUG0, text, v0, v1)
#else
  #define ULOG0_DEBUG(text)            do {} while (0)
  #define ULOG8_DEBUG(text, val)       do {} while (0)
  #define ULOG16_DEBUG(text, v0, v1)   do {} while (0)
#endif

#ifdef __cplusplus
}
#endif

#pragma once

#define TRACE_LEVEL_ERROR   0
#define TRACE_LEVEL_WARNING 1
#define TRACE_LEVEL_MILE    2
#define TRACE_LEVEL_INFO    3
#define TRACE_LEVEL_DEBUG   4

// Include the project trace config
#ifdef HAS_TRACE_CONFIG_FILE
#include "conf_trace.h"
#endif

// Default trace level is warning
#ifndef TRACE_LEVEL
#define TRACE_LEVEL TRACE_LEVEL_WARN
#endif

/**
 * Define the API
 */
#ifdef __cplusplus
extern "C"
#endif
void trace(const char *what, ...);

// Helper macros to check if the domain is allowed
#define TRACE_IMPL(level, domain, text, ...) \
    do { \
        if ((level) <= TRACE_LEVEL && (DOMAIN_##domain##_ENABLED)) { \
            trace(text, ##__VA_ARGS__); \
        } \
    } while (0)

// Define the TRACE macros
#if TRACE_LEVEL >= TRACE_LEVEL_ERROR
#define TRACE_ERROR(domain, text, ...) TRACE_IMPL(TRACE_LEVEL_ERROR, domain, text, ##__VA_ARGS__)
#else
#define TRACE_ERROR(domain, text, ...) do {} while (0)
#endif

#if TRACE_LEVEL >= TRACE_LEVEL_WARN
#define TRACE_WARN(domain, text, ...) TRACE_IMPL(TRACE_LEVEL_WARN, domain, text, ##__VA_ARGS__)
#else
#define TRACE_WARN(domain, text, ...) do {} while (0)
#endif

#if TRACE_LEVEL >= TRACE_LEVEL_MILE
#define TRACE_MILE(domain, text, ...) TRACE_IMPL(TRACE_LEVEL_MILE, domain, text, ##__VA_ARGS__)
#else
#define TRACE_MILE(domain, text, ...) do {} while (0)
#endif

#if TRACE_LEVEL >= TRACE_LEVEL_INFO
#define TRACE_INFO(domain, text, ...) TRACE_IMPL(TRACE_LEVEL_INFO, domain, text, ##__VA_ARGS__)
#else
#define TRACE_INFO(domain, text, ...) do {} while (0)
#endif

#if TRACE_LEVEL >= TRACE_LEVEL_DEBUG
#define TRACE_DEBUG(domain, text, ...) TRACE_IMPL(TRACE_LEVEL_DEBUG, domain, text, ##__VA_ARGS__)
#else
#define TRACE_DEBUG(domain, text, ...) do {} while (0)
#endif

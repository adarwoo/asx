#pragma once


/**
 * Tracing API
 */
#ifdef __cplusplus
extern "C"
#endif
#ifdef DEBUG
void trace(const char *what, ...);
#else
inline void trace(const char *what, ...) {}
#endif

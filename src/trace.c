#ifdef DEBUG
#include <trace.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include <timer.h>

#ifndef MAX_MESSAGE_SIZE
#  define MAX_MESSAGE_SIZE 32
#endif

#ifndef MAX_NUMBER_OF_MESSAGES
#  define MAX_NUMBER_OF_MESSAGES 32
#endif

#define LOGGER_BUFFER_SIZE (MAX_MESSAGE_SIZE * MAX_NUMBER_OF_MESSAGES)

#define MAX_MESSAGE_SIZE 32 // Max size of a single trace message

char trace_buffer[LOGGER_BUFFER_SIZE];
static uint16_t loggerIndex = 0;
static timer_count_t lastTimerCount = 0; // Store the last timer count for elapsed time calculation

void trace(const char *format, ...) {
   // Constants for the format
   const uint16_t timerLen = 5;    // 4 digits for elapsed time + 1 space
   const uint16_t maxTextLen = MAX_MESSAGE_SIZE - 6;

   // Calculate elapsed time since the last trace
   timer_count_t currentTimerCount = timer_get_count();
   timer_count_t elapsedTime = timer_get_count() - lastTimerCount;
   lastTimerCount = currentTimerCount;

   trace_buffer[loggerIndex++] = '[';
   snprintf(&trace_buffer[loggerIndex], timerLen, "%04lu", elapsedTime % 10000);

   if (elapsedTime > 9999) {
      trace_buffer[loggerIndex] = '+';
   }

   loggerIndex += 4;
   trace_buffer[loggerIndex++] = ']';
   va_list args;
   va_start(args, format);
   memset(&trace_buffer[loggerIndex], ' ', maxTextLen);
   vsnprintf(&trace_buffer[loggerIndex], maxTextLen, format, args);
   #ifdef SIM
   vnprintf(maxTextLen, format, args);
   #endif
   loggerIndex+=maxTextLen;

   // Check if the message fits in the remaining buffer space
   if (loggerIndex >= LOGGER_BUFFER_SIZE) {
      loggerIndex = 0;
   }

   // Write the padding '<' characters only after the most recent message
   for (uint16_t i = 0; i < MAX_MESSAGE_SIZE; i++) {
      trace_buffer[loggerIndex + i] = '<';
   }
}
#endif // def DEBUG
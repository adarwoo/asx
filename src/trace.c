#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstdarg>

#include <trace.h>

#include <timer.h>

#ifndef MAX_MESSAGE_SIZE
#  define MAX_MESSAGE_SIZE 32
#endif

#ifndef MAX_NUMBER_OF_MESSAGES
#  define MAX_NUMBER_OF_MESSAGES 32
#endif

#define LOGGER_BUFFER_SIZE (MAX_MESSAGE_SIZE * MAX_NUMBER_OF_MESSAGES)

#define MAX_MESSAGE_SIZE 32 // Max size of a single trace message

#define TRACE_END_CHAR '<'

char trace_buffer[LOGGER_BUFFER_SIZE];
static uint16_t loggerIndex = 0;
static timer_count_t lastTimerCount = 0; // Store the last timer count for elapsed time calculation

static void
#ifndef _WIN32
   __attribute__ ((section (".init5"), naked, used))
#endif
trace_init( void )
{
   memset(trace_buffer, TRACE_END_CHAR, LOGGER_BUFFER_SIZE);
}

static void printNumber(uint16_t n, char *) {

}

/**
 * Printf like function trace API. The format is as follow:
 * i=integer, u=unsigned, x=hex, s=string, c=char, b=bool
 */
void trace(const char *format, ...) {
   // Constants for the format
   const uint16_t timerLen = 5;    // 4 digits for elapsed time + 1 space
   const uint16_t maxTextLen = MAX_MESSAGE_SIZE - 6;

   // Calculate elapsed time since the last trace
   timer_count_t currentTimerCount = timer_get_count();
   timer_count_t elapsedTime = timer_get_count() - lastTimerCount;
   lastTimerCount = currentTimerCount;

   trace_buffer[loggerIndex++] = '[';
   utoa(elapsedTime % 10000, &trace_buffer[loggerIndex], 10);

   if (elapsedTime > 9999) {
      trace_buffer[loggerIndex] = '+';
   }

   loggerIndex += 4;
   trace_buffer[loggerIndex++] = ']';
   va_list args;
   va_start(args, format);

   // Iterate the string. Look out for %
   char c = trace_buffer[loggerIndex];

   while ((c = *format++) != '\0') {
      if (c == '%')  {
         c = *format++;
         char *pBuf = &trace_buffer[loggerIndex++];

         switch (c) {
         case 'b': {
            bool b = va_arg(args, int); // bool is promoted to int
            pBuf = b ? "1" : "0";
            ++loggerIndex;
            break;
         }
         case 'u': {
            unsigned int u = va_arg(args, unsigned int);
            utoa(u, pBuf, 10);
            loggerIndex += std::strlen(buffer);
            break;
         }
         case 'i': {
            unsigned int u = va_arg(args, unsigned int);
            itoa(u, pBuf, 10);
            loggerIndex += std::strlen(buffer);
            break;
         }
         case 'x': {
            unsigned int u = va_arg(args, unsigned int);
            utoa(u, pBuf, 16);
            loggerIndex += std::strlen(buffer);
            break;
         }
         case 's': {
            const char* s = va_arg(args, const char*);
            std::strncpy(pBuf, s, sizeof(trace_buffer) - loggerIndex - 1);
            loggerIndex += std::strlen(s);
            break;
         }
         default:
            trace_buffer[loggerIndex++] = c;
            break;
         }
      }
      else
      {
         trace_buffer[loggerIndex++] = c;
      }
   }

   memset(&trace_buffer[loggerIndex], ' ', maxTextLen);

   // Check if the message fits in the remaining buffer space
   if (loggerIndex >= LOGGER_BUFFER_SIZE) {
      loggerIndex = 0;
   }

   // Write the padding '<' characters only after the most recent message
   for (uint16_t i = 0; i < MAX_MESSAGE_SIZE; i++) {
      trace_buffer[loggerIndex + i] = TRACE_END_CHAR;
   }
}

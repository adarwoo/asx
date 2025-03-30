#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include <trace.h>

#include <timer.h>

#ifndef MAX_MESSAGE_SIZE
#  define MAX_MESSAGE_SIZE 32
#endif

#ifndef MAX_NUMBER_OF_MESSAGES
#  define MAX_NUMBER_OF_MESSAGES 32
#endif

#define LOGGER_BUFFER_SIZE (MAX_MESSAGE_SIZE * MAX_NUMBER_OF_MESSAGES)

/// Char used to fill the next line
#define TRACE_END_CHAR '<'

/// @brief Holds the whole logging traces in memory
char trace_buffer[LOGGER_BUFFER_SIZE];

/// @brief Where we currently are in the trace buffer
static uint8_t loggerIndex = 0;

// Store the last timer count for elapsed time calculation
static timer_count_t lastTimerCount = 0;

/// @brief Reset the buffer with all < char
static void
#ifndef _WIN32
   __attribute__ ((section (".init5"), naked, used))
#endif
trace_init( void )
{
   memset(trace_buffer, TRACE_END_CHAR, LOGGER_BUFFER_SIZE);
}


/**
 * Printf like function trace API. The format is as follow:
 * d=integer, u=unsigned, x=hex, s=string, c=char, b=bool
 */
void trace(const char *format, ...) {
   // Constants for the format
   uint16_t charsLeft = MAX_MESSAGE_SIZE - 6;

   // Convert number in this buffer
   static char number_buffer[MAX_MESSAGE_SIZE - 6] = {0};

   // Pointer to the buffer
   char *pTrace = &trace_buffer[loggerIndex * MAX_MESSAGE_SIZE];

   // Calculate elapsed time since the last trace
   timer_count_t currentTimerCount = timer_get_count();
   timer_count_t elapsedTime = timer_get_count() - lastTimerCount;
   lastTimerCount = currentTimerCount;

   // Format the string to receive the time
   memcpy(pTrace, "[    ]", 6);

   // The number could be 1 digit or 4 digits. If 1,2 and 3, we end
   utoa(elapsedTime % 10000, number_buffer, 10);

   // How many digits to copy right adjusted
   uint8_t len_time = strlen(number_buffer);
   memcpy(pTrace + 5 - len_time, number_buffer, len_time);

   // If the original number is big add a +
   if (elapsedTime > 9999) {
      *pTrace = '+';
   }

   // Advance the pointer
   pTrace += 6;

   // Process the string
   va_list args;
   va_start(args, format);
   char c;

   while ( (c = *format++) != '\0' && charsLeft > 0) {
      bool bCopyRequired = false;

      if (c == '%')  {
         c = *format++; // Skip

         switch (c) {
         case 'b': {
            bool b = va_arg(args, int); // bool is promoted to int
            pTrace = b ? "1" : "0";
            break;
         }
         case 'u': {
            unsigned int u = va_arg(args, unsigned int);
            utoa(u, number_buffer, 10);
            bCopyRequired = true;
            break;
         }
         case 'd': {
            unsigned int d = va_arg(args, unsigned int);
            itoa(d, number_buffer, 10);
            bCopyRequired = true;
            break;
         }
         case 'x': {
            unsigned int u = va_arg(args, unsigned int);
            utoa(u, number_buffer, 16);
            bCopyRequired = true;
            break;
         }
         case 's': {
            const char* s = va_arg(args, const char*);
            // Don't overwrite the last char which is already a 0
            strncpy(number_buffer, s, sizeof(number_buffer) - 1);
            bCopyRequired = true;
            break;
         }
         default:
            break;
         }

         if ( bCopyRequired ) {
            uint8_t countToCopy = strlen(number_buffer);

            // Add '...' if we're truncated
            if ( countToCopy > charsLeft ) {
               memcpy(pTrace, number_buffer, charsLeft);
               pTrace += charsLeft;
               pTrace -= 3;
               memcpy(pTrace, "...", 3); // Will not add a 0
               break;
            } else {
               // Copy at most charsLeft
               memcpy(pTrace, number_buffer, countToCopy);
               pTrace+=countToCopy;
               charsLeft-=countToCopy;
            }
         }
      }
      else
      {
         *pTrace++ = c;
         --charsLeft;
      }
   }

   // Pad the remaining byte with space
   while ( charsLeft-- ) {
      *pTrace++ = ' ';
   }

   // Check if the message fits in the remaining buffer space
   if (++loggerIndex >= LOGGER_BUFFER_SIZE) {
      loggerIndex = 0;
   }

   // Write the padding '<' characters only after the most recent message
   pTrace = &trace_buffer[loggerIndex * MAX_MESSAGE_SIZE];
   memset(pTrace, TRACE_END_CHAR, MAX_MESSAGE_SIZE);
}

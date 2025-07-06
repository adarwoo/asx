/**
 * @file
 * Alert API implementation
 * @author gax
 * @internal
 * @addtogroup service
 * @{
 * @addtogroup alert
 * @{
 */
#include <avr/wdt.h>

#include <assert.h>
#include <string.h>
#include <ioport.h>

// Need to include the configuration to determine the alert pin
#include <board.h>
#include <alert.h>
#include <trace.h>

#ifdef ALERT_RECORD
#   ifndef ALERT_RECORD_OFFSET
#      define ALERT_RECORD_OFFSET 0
#   endif
#endif

/**
 * Weak hook function: can be overridden elsewhere
 * Lights the alert LED if defined
 */
__attribute__((weak)) void alert_user_function(void) {
   // Dump to the debug pin
   #ifdef ALERT_OUTPUT_PIN
   ioport_vport_set_dir(ALERT_OUTPUT_PIN);
   ioport_vport_set_pin(ALERT_OUTPUT_PIN);
   #endif
}

/**
 * We need to initialize the alert API
 * For LED notification, we need to set the LED direction.
 */
static void
#ifndef _WIN32
   __attribute__ ((section (".init5"), naked, used))
#endif
alert_init( void )
{
#ifdef ALERT_OUTPUT_PIN
#endif
}

/**
 * General way on alerting of an assertion using whatever mechanism
 *  is available on the target.
 *
 * This function is best called through the macros alert an alert_and_stop.
 *
 * @param doAbort If true, the abort method is called to halt the cpu
 * @param line Line where to alert took place.
 * @param file Name of the file where the exception occurred.
 */
void alert_record( bool doAbort, int line, const char *file )
{
   cli();

   #ifdef DEBUG
   // Write the alert in the trace
   const char *last_slash = strrchr(file, '/'); // Find the last '/'
   trace("ALERT!%u %s", line, (last_slash) ? last_slash + 1 : file);
   #endif

   alert_user_function();

   // Output to stdout?
   #ifdef ALERT_TO_STDOUT
   printf_P("ALERT: %s, line %d\n", file, line);
   #endif

   // Stop or not
   if ( doAbort )
   {
      #ifdef DEBUG
         // Stop the watchdog for debug only so the program will hang rather
         //  than reset
         wdt_disable();
      #endif

      // Lock forever. Breaking the execution will end up here
      //  (or an interrupt), and the trace will point to the
      //  culprit.
      // The watchdog will then reset.
      for (;;)
      {
      }
   }

   sei();
}

 /**@}*/
 /**@} ---------------------------  End of file  --------------------------- */
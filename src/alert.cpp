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

#ifdef ALERT_RECORD
#   ifndef ALERT_RECORD_OFFSET
#      define ALERT_RECORD_OFFSET 0
#   endif
#endif

/**
 * Weak hook function: can be overridden elsewhere
 * Lights the alert LED if defined
 */
extern "C" __attribute__((weak)) void alert_user_function(void) {
   // Dump to the debug pin
   #ifdef ALERT_OUTPUT_PIN
   ioport_vport_set_dir(ALERT_OUTPUT_PIN);
   ioport_vport_set_pin(ALERT_OUTPUT_PIN);
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
extern "C" void alert_record( bool doAbort)
{
   alert_user_function();

   // Stop or not
   if ( doAbort ) {
      #ifdef DEBUG
         // Stop the watchdog for debug only so the program will hang rather
         //  than reset
         wdt_disable();
      #else
         // Kick the watchdog to give time to flush the logs
         wdt_reset();
      #endif

      // Flush ULOG buffers - this is blocking which is OK at this stage
      ulog_flush();

      // Lock forever. Breaking the execution will end up here
      //  (or an interrupt), and the trace will point to the
      //  culprit.
      // The watchdog will then reset.
      for (;;) {}
   }
}

 /**@}*/
 /**@} ---------------------------  End of file  --------------------------- */
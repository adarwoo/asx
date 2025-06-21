/**
 * Manages the watchdog
 * This code validates that the watchdog is working correctly by testing it once
 * on startup
 */

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>

#include <stdint.h>
#include <stdbool.h>

#include <alert.h>

// Use a magic number to detect a controlled reboot
#define COLD_RESET_OK_SIGNATURE 0xF0A5C3DE

/**
 * This functions serves 3 functions:
 *  1 - It tests the whole RAM by writing a pattern and reading back twice for each bytes
 *  2 - If validates the watchdog is operational by resetting the application once on power-up
 *  3 - It fills the RAM with 0xAA which allow monitoring the heap and stack use
 *
 * Note: The watchdog is tested if activated in the fuse
 */
void __attribute__ ((section (".init0"), naked, used))
   watchdog_ram_test()
{
   // The reset is a PowerOn reset ?
   if ( RSTCTRL.RSTFR & RSTCTRL_PORF_bm ) {
      // Write the whole RAM
      for (volatile uint8_t* p = (uint8_t *)RAMSTART; p < (uint8_t *)RAMEND; ++p) {
         *p = 0x55;
         *p ^= 0xFF;

         if (*p != 0xAA) {
            goto test_failed;
         }
      }

      // Check if the watchdog is activated
      if ( WDT.CTRLA == 0 ) {
         return;
      }

      // All good, stamp the memory with the marker
      *(uint32_t*)RAMSTART = COLD_RESET_OK_SIGNATURE;

      // Lock to trigger a watchdog reset
      // Note: The will lock if the watchdog is not running. This also tests for this
      while (true);
   }

   // Watchdog reset ?
   if ( RSTCTRL.RSTFR & RSTCTRL_WDRF_bm ) {
      // Following a memory and watchdog test?
      if ( *(uint32_t*)RAMSTART == COLD_RESET_OK_SIGNATURE ) {
         // Clear the memory marker
         *(uint32_t*)RAMSTART = 0;

         // Reset the reset flag - normal boot
         RSTCTRL.RSTFR = 0xFF; // more efficient than writting the bit
      }

      // The RSTCTRL WDRF flag is not reset - this is a real watchdog reset (crash)
      // The application must check the flag to start in safety mode

      // Return - will initialise .data and .bss and start
      return;
   }

test_failed:
   // This should never happen - but the RAM test failed
   // We should not start - the MCU is dead
   // Loop forever - but refresh the watchdog
   // During the test, the alert
   alert_user_function();

   while (true) {
      wdt_reset();
   }
}

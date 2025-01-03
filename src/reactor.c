/**
 * @addtogroup service
 * @{
 * @addtogroup reactor
 * @{
 *****************************************************************************
 * Implementation of the reactor pattern.
 * This reactor allow dealing with asynchronous events handled by interrupts
 *  within the time frame of the main application.
 * When no asynchronous operation take place, the micro-controller is put to
 *  sleep saving power.
 * The reactor cycle time can be monitored defining debug pins REACTOR_IDLE
 *  and REACTOR_BUSY
 * This version does the sorting by position and first come first served
 * At 20MHz - The reactor + timer takes 13us every 1ms of the CPU. (1.3%)
 * The reactor overhead is 5us + 1us to notify from an interrupt
 *****************************************************************************
 * @file
 * Implementation of the reactor API
 * @author software@arreckx.com
 * @internal
 */
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>  // for CHAR_BIT
#include <stdarg.h>

#include "interrupt.h"

#include "alert.h"
#include "debug.h"
#include "reactor.h"

#include "debug.h"

/**
 * @def REACTOR_MAX_HANDLERS
 * Maximum number of handlers for the reactor. This defines defaults
 *  to 8 and can be overridden in the board_config.h file if more are
 *  required.
 */
#define REACTOR_MAX_HANDLERS 32

// Lookup table for fast masking
static const uint8_t bit_shift_table[8] = {
   0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
};

/** Holds all reactor handlers with mapping to the reaction mask */
typedef struct
{
   reactor_handler_t handler;
   void *arg;
} reactor_item_t;

/** Keep an array of handlers whose position match the bit position of the handle */
static reactor_item_t _handlers[REACTOR_MAX_HANDLERS] = {0};

/** Lock new registrations once the reactor is started */
static bool _reactor_lock = false;

/** Notification registry */
#define _reactor_notifications *((uint32_t*)(&GPIO0))

/**
 * Initialize the reactor API
 * This is called prior to C++ static constructors
 */
static void
   __attribute__ ((section (".init5"), naked, used))
   _reactor_init(void)
{
   // Use a debug pin if available
   debug_init(REACTOR_IDLE);
   debug_init(REACTOR_BUSY);

   // Allow simplest sleep mode to resume very fast
   sleep_enable();
}

/**
 * Register a new reactor handler.
 * The handler is called once an interrupt or another reactor handler calls
 *  the notify function,
 * The priority determines which handlers are called first in a round-robin
 *  scheduler.
 * Providing the system has enough processing time, a handler should
 *  eventually be called.
 * However, low priority handler will suffer from more potential delay and
 *  jitter.
 *
 * @param handler Function to call when an event is ready for processing
 * @param priority Priority of the handler during round-robin scheduling
                   High priority handlers are handled first
 *
 */
reactor_handle_t reactor_register(const reactor_handler_t handler, reactor_priority_t priority)
{
   alert_and_stop_if(_reactor_lock != false);

   // Initialize variables depending on priority
   uint8_t start = (priority == reactor_prio_low) ? REACTOR_MAX_HANDLERS - 1 : 0;
   uint8_t end = (priority == reactor_prio_low) ? 0 : REACTOR_MAX_HANDLERS;
   int8_t step = (priority == reactor_prio_low) ? -1 : 1;

   for (uint8_t i=start; i!=end; i += step)
   {
      if (_handlers[i].handler == NULL)
      {
         _handlers[i].handler = handler;
         return i;
      }
   }

   // Make sure a valid slot was found
   alert_and_stop();

   return 0;
}

void reactor_null_notify_from_isr(reactor_handle_t handle)
{
   if ( handle != REACTOR_NULL_HANDLE )
   {
      uint8_t bit_shift = bit_shift_table[handle % 8];
      uint8_t *pNotif = (uint8_t*)&_reactor_notifications;
      uint8_t byte_index = handle / 8;

      pNotif[byte_index] |= bit_shift;
   }
}


/**
 * Interrupts are disabled for atomic operations
 * This function can be called from within interrupts
 */
void reactor_notify( reactor_handle_t handle, void *data )
{
   if ( handle != REACTOR_NULL_HANDLE )
   {
      irqflags_t flags = cpu_irq_save();

      _handlers[handle].arg = data;
      reactor_null_notify_from_isr(handle);

      cpu_irq_restore(flags);
   }
}

/**
 * Clear pending operations. This should be called in a critical section to prevent races.
 * @param handle Handle to clear.
 * @param ... More handles are accepted
 */
void reactor_clear(reactor_mask_t mask)
{
    _reactor_notifications &= ~(mask);
}

/** Process the reactor loop */
void reactor_run(void)
{
   // Set the watchdog which is reset by the reactor
   // If the timer is uses, the watchdog would be refreshed every 1ms, but otherwise, we don't know
   // There is no need for too aggressive timings
   wdt_enable(WDTO_1S);

   // Atomically read and clear the notification flags allowing more
   //  interrupt from setting the flags which will be processed next time round
   while (true)
   {
      debug_clear(REACTOR_BUSY);
      cli();

      if ( _reactor_notifications == 0 )
      {
         debug_set(REACTOR_IDLE);

         // The AVR guarantees that sleep is executed before any pending interrupts
         sei();
         sleep_cpu();
         debug_clear(REACTOR_IDLE);
      }
      else
      {
         // At least 1 flag set
         uint8_t handle = __builtin_ctzl(_reactor_notifications);

         // Flip the flag before calling - so it could be set again by the caller
         {
            uint8_t bit_shift = bit_shift_table[handle % 8];
            uint8_t *pNotif = (uint8_t*)&_reactor_notifications;
            uint8_t byte_index = handle / 8;

            pNotif[byte_index] ^= bit_shift;
         }

         sei();

         debug_set(REACTOR_BUSY);
         _handlers[handle].handler(_handlers[handle].arg);

         // Keep the system alive for as long as the reactor is calling handlers
         // We assume that if no handlers are called, the system is dead.
         wdt_reset();
      }
   };
}

 /**@}*/
 /**@}*/
 /**@} ---------------------------  End of file  --------------------------- */
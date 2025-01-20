#ifndef reactor_HAS_ALREADY_BEEN_INCLUDED
#define reactor_HAS_ALREADY_BEEN_INCLUDED
/**
 * @file
 * Reactor API declaration
 * @addtogroup service
 * @{
 * @addtogroup reactor
 * @{
 *****************************************************************************
 * Reactor API.
 * The reactor pattern allow to manage many asynchronous events
 *  within the same thread of processing.
 * This allow to process in the main function time and stack, events generated
 *  within interrupt code.
 * The events are prioritized such that the first handler that registers is
 *  always dealt with first.
 * This API is very simple, but deals with the complexity of atomically suspending
 *  the MPU in the main loop whilst processing interrupt outside of the interrupt
 *  context.
 * The interrupt do not need to hug the CPU time for long and simply notify the
 *  reactor that work is needed.
 * Therefore, all the work is done in the same context and stack avoiding
 *  nasty race conditions.
 * To use this API, simply register a handler with #reactor_register.
 * Then, if the main program needs to process the event, register a event handler
 *  using #reactor_register.
 * Finally, let the reactor loose once all the interrupts are up and running with
 *  #reactor_run.
 * To monitor the reactor, you can defined a REACTOR_BUSY debug pin. This pin is high
 *  when the reactor calls a handler.
 * @author software@arreckx.com
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/** Invalid reactor handle */
#define REACTOR_NULL_HANDLE 255

/** Standard priorities for the reactor */
typedef uint8_t reactor_priority_t;

#define reactor_prio_low 0
#define reactor_prio_high 1


/**
 * @typedef reactor_handle_t
 * A handle created by #reactor_register to use with #reactor_notify to
 *  tel the reactor to process the callback.
 */
typedef uint8_t reactor_handle_t;

/** Internal mask for notifications */
typedef uint32_t reactor_mask_t;

/** Callback type called by the reactor when an event has been logged */
typedef void (*reactor_handler_t)(void*);

/** Add a new reactor process */
reactor_handle_t reactor_register(
   const reactor_handler_t, reactor_priority_t);

/**
 * Notify a handler should be invoke next time the loop is processed
 * Interrupt safe. No lock here since this is processed in normal
 * (not interrupt) context.
 */
void reactor_notify( reactor_handle_t handle, void* );

/**
 * Clear pending operations
 * @param mask A mask created with reactor_mask_of
 * @param ... More handles as required
 */
void reactor_clear( reactor_mask_t mask );

/** Process the reactor loop */
void reactor_run(void);

/**
 * ISR version where no data is passed - the fastest notification in 6 cycles
 * Make sure to activate -flto to inline this function
 */
void reactor_null_notify_from_isr(reactor_handle_t handle);

/** Get the mask of a handler. The mask can be OR'd with other masks */
inline reactor_mask_t reactor_mask_of(reactor_handle_t handle) {
   uint32_t retval = 0;

   if ( handle != REACTOR_NULL_HANDLE ) {
      return (1L << handle);
   }

   return retval;
}


#ifdef __cplusplus
}
#endif

/** @} */
/** @} */
#endif /* ndef reactor_HAS_ALREADY_BEEN_INCLUDED */
#ifndef debug_h_HAS_ALREADY_BEEN_INCLUDED
#define debug_h_HAS_ALREADY_BEEN_INCLUDED
/**
 * @addtogroup service
 * @{
 * @addtogroup debug
 * @{
 *****************************************************************************
 * A set of macro to allow an API to use debug pins if allowed.
 * To enable a debug pin for a given API, simply define it in the
 * board configuration header as:
 * @code{.c}
 *    #define DEBUG_xxx IOPORT_CREATE_PIN(PORTC, 0)
 * @endcode
 * Where:
 * - xxx is the name of the API group such as REACTOR, FB etc..
 * The API only needs to make use of the debug API as if the pin
 *  had been defined.
 *     debug_set(REACTOR);
 * If the macro has not been defined, this will expand to nothing.
 * @file
 * Reactor API definition
 * @author software@arreckx.com
 */
#include "conf_board.h"
#include "cpp.h"

/** Define macro to check if the argument x expands to DEBUG_x */
#define IS_DEBUG_PORT(x) IS_PAREN( CAT(DEBUG_, x) (()) )

#ifdef __cplusplus
   /** Set the given pin, if it has been defined as DEBUG_x, an an output pin set to 0 */
   #define debug_init(x) IIF(IS_DEBUG_PORT(x))(Pin(DEBUG_ ## x).init(dir_t::out, value_t::low),{})
   /** Set the corresponding debug pin  */
   #define debug_set(x) IIF(IS_DEBUG_PORT(x))(Pin(DEBUG_ ## x).set(true),{})
   /** Clear the corresponding debug pin  */
   #define debug_clear(x) IIF(IS_DEBUG_PORT(x))(Pin(DEBUG_ ## x).clear(),{})
#else
   #include "ioport.h"

   /** Set the given pin, if it has been defined as DEBUG_x, an an output pin set to 0 */
   #define debug_init(x) IIF(IS_DEBUG_PORT(x))(ioport_set_pin_dir(DEBUG_ ## x, IOPORT_DIR_OUTPUT),{})
   /** Set the corresponding debug pin  */
   #define debug_set(x) IIF(IS_DEBUG_PORT(x))(ioport_set_pin_level(DEBUG_ ## x, true),{})
   /** Clear the corresponding debug pin  */
   #define debug_clear(x) IIF(IS_DEBUG_PORT(x))(ioport_set_pin_level(DEBUG_ ## x, false),{})
#endif

/**@}*/
/**@}*/
#endif /* ndef debug_h_HAS_ALREADY_BEEN_INCLUDED */
#ifndef alert_h_HAS_ALREADY_BEEN_INCLUDED
#define alert_h_HAS_ALREADY_BEEN_INCLUDED
/**
 * @addtogroup service
 * @{
 * @addtogroup alert
 * @{
 *****************************************************************************
 * Simple macros and functions which allow signalling the outside world of
 *  an internal assertion without crashing the AVR.
 * This API can be configured by the product to signal the assertion to the
 *  outside world through some I/Os or serial output.
 * This API allow the user to report to the outside that a internal
 *  fault has occurred.
 * The output mode goes from :
 *  - flashing an LED
 *  - writing to the internal EEPROM
 *  - writing to the UART
 * ... depending on the configuration.
 *****************************************************************************
 * @file
 * Alert reporting API header
 * @author software@arreckx.com
 */
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/************************************************************************/
/* Public API                                                           */
/************************************************************************/

/**
 * Prototype to the user alert function. A weak default implementation
 * is provided, which turn on the alert LED.
 * This can be overriden by the application.
 * This function should be used in all critical tests
 */
void alert_user_function(void);

/** Ready the alert stack */
// Now done in init5 void alert_init(void);

/** Each application must customize this function */
void alert_record(bool abort, int line, const char *file);

/** Raise and alert. This macro adds the line and file automatically */
#define alert() alert_record(false, __LINE__, __FILE__)

/** Raise an alert and stop. This macro adds the line and file automatically */
#define alert_and_stop() alert_record(true, __LINE__, __FILE__)

/**
 * Conditionally raise and alert and stop.
 * This macro adds the line and file automatically
 *
 * @param cond Condition evaluated as a boolean upon which to alert
 */
#define alert_and_stop_if(cond)               \
   if (cond) {                                \
      alert_record(true, __LINE__, __FILE__); \
   }

#ifdef __cplusplus
}
#endif

/**@}*/
/**@}*/
#endif /* ndef alert_h_HAS_ALREADY_BEEN_INCLUDED */

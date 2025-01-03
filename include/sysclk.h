/**
 * \file
 *
 * \brief Chip-specific system clock management functions
 *
 * Copyright (c) 2010-2018 Microchip Technology Inc. and its subsidiaries.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Subject to your compliance with these terms, you may use Microchip
 * software and any derivatives exclusively with Microchip products.
 * It is your responsibility to comply with third party license terms applicable
 * to your use of third party software (including open source software) that
 * may accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
 * INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
 * AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT WILL MICROCHIP BE
 * LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL
 * LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND WHATSOEVER RELATED TO THE
 * SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN ADVISED OF THE
 * POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT
 * ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY
 * RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *
 * \asf_license_stop
 *
 */
/*
 * Support and FAQ: visit <a href="https://www.microchip.com/support/">Microchip Support</a>
 */
#ifndef TINY2_SYSCLK_H_INCLUDED
#define TINY2_SYSCLK_H_INCLUDED

#include <avr/io.h>
#include <ccp.h>

// Include clock configuration for the project.
#include <conf_clock.h>

#ifdef __cplusplus
extern "C" {
#endif

// Use 20/16 MHz with no prescaling if config was empty.
#ifndef CONFIG_SYSCLK_SOURCE
# define CONFIG_SYSCLK_SOURCE    SYSCLK_SRC_RC20MHZ
#endif /* CONFIG_SYSCLK_SOURCE */

#ifndef CONFIG_SYSCLK_PSDIV
# define CONFIG_SYSCLK_PSDIV    SYSCLK_PSDIV_1
#endif /* CONFIG_SYSCLK_PSADIV */

//! \name System Clock Sources
//@{
//! Internal 20 MHz RC oscillator
#define SYSCLK_SRC_RC20MHZ   CLKCTRL_CLKSEL_OSC20M_gc
//! Fuse set to select this speed
#define SYSCLK_SRC_RC16MHZ   CLKCTRL_CLKSEL_OSC20M_gc
//! Internal 32768Hz ULP RC oscillator
#define SYSCLK_SRC_ULP32KHZ  CLKCTRL_CLKSEL_OSCULP32K_gc
//! External 32 KHz oscillator
#define SYSCLK_SRC_X32KHZ    CLKCTRL_CLKSEL_XOSC32K_gc
//! External oscillator
#define SYSCLK_SRC_XOSC      CLKCTRL_CLKSEL_EXTCLK_gc
//@}

//! \name Prescaler A Setting (relative to CLKsys)
//@{
#define SYSCLK_PSDIV_1      0     //!< Do not prescale
#define SYSCLK_PSDIV_2      CLKCTRL_PDIV_2X_gc | CLKCTRL_PEN_bm     //!< Prescale CLKper4 by 2
#define SYSCLK_PSDIV_4      CLKCTRL_PDIV_4X_gc | CLKCTRL_PEN_bm    //!< Prescale CLKper4 by 4
#define SYSCLK_PSDIV_8      CLKCTRL_PDIV_8X_gc | CLKCTRL_PEN_bm    //!< Prescale CLKper4 by 8
#define SYSCLK_PSDIV_16     CLKCTRL_PDIV_16X_gc | CLKCTRL_PEN_bm    //!< Prescale CLKper4 by 16
#define SYSCLK_PSDIV_32     CLKCTRL_PDIV_32X_gc | CLKCTRL_PEN_bm   //!< Prescale CLKper4 by 32
#define SYSCLK_PSDIV_64     CLKCTRL_PDIV_64X_gc | CLKCTRL_PEN_bm   //!< Prescale CLKper4 by 64
# define SYSCLK_PSDIV_6     CLKCTRL_PDIV_6X_gc  | CLKCTRL_PEN_bm    //!< Prescale CLKper4 by 6
# define SYSCLK_PSDIV_10    CLKCTRL_PDIV_10X_gc | CLKCTRL_PEN_bm    //!< Prescale CLKper4 by 10
# define SYSCLK_PSDIV_12    CLKCTRL_PDIV_12X_gc | CLKCTRL_PEN_bm    //!< Prescale CLKper4 by 12
# define SYSCLK_PSDIV_24    CLKCTRL_PDIV_24X_gc | CLKCTRL_PEN_bm    //!< Prescale CLKper4 by 24
# define SYSCLK_PSDIV_48    CLKCTRL_PDIV_48X_gc | CLKCTRL_PEN_bm    //!< Prescale CLKper4 by 48
//@}

/**
 * \name RTC clock source identifiers
 *
 * @{
 */

/** 1kHz from internal ULP oscillator. Low precision */
#define SYSCLK_RTCSRC_ULP     CLK_RTCSRC_ULP_gc
/** 1.024kHz from 32.768kHz crystal oscillator TOSC */
#define SYSCLK_RTCSRC_TOSC    CLK_RTCSRC_TOSC_gc
/** 1.024kHz from 32.768kHz internal RC oscillator */
#define SYSCLK_RTCSRC_RCOSC   CLK_RTCSRC_RCOSC_gc
/** 32.768kHz from crystal oscillator TOSC */
#define SYSCLK_RTCSRC_TOSC32  CLK_RTCSRC_TOSC32_gc
/** 32.768kHz from internal RC oscillator */
#define SYSCLK_RTCSRC_RCOSC32 CLK_RTCSRC_RCOSC32_gc
/** External clock on TOSC1 */
#define SYSCLK_RTCSRC_EXTCLK  CLK_RTCSRC_EXTCLK_gc


/**
 * \name Clock perscaler value
 */
#if CONFIG_SYSCLK_PSDIV == SYSCLK_PSDIV_1
#define SYSCLOCK_PRESCALE_VALUE 1
#elif CONFIG_SYSCLK_PSDIV == SYSCLK_PSDIV_2
#define SYSCLOCK_PRESCALE_VALUE 2
#elif CONFIG_SYSCLK_PSDIV == SYSCLK_PSDIV_4
#define SYSCLOCK_PRESCALE_VALUE 4
#elif CONFIG_SYSCLK_PSDIV == SYSCLK_PSDIV_8
#define SYSCLOCK_PRESCALE_VALUE 8
#elif CONFIG_SYSCLK_PSDIV == SYSCLK_PSDIV_16
#define SYSCLOCK_PRESCALE_VALUE 16
#elif CONFIG_SYSCLK_PSDIV == SYSCLK_PSDIV_32
#define SYSCLOCK_PRESCALE_VALUE 32
#elif CONFIG_SYSCLK_PSDIV == SYSCLK_PSDIV_64
#define SYSCLOCK_PRESCALE_VALUE 64
#elif CONFIG_SYSCLK_PSDIV == SYSCLK_PSDIV_6
#define SYSCLOCK_PRESCALE_VALUE 6
#elif CONFIG_SYSCLK_PSDIV == SYSCLK_PSDIV_10
#define SYSCLOCK_PRESCALE_VALUE 10
#elif CONFIG_SYSCLK_PSDIV == SYSCLK_PSDIV_12
#define SYSCLOCK_PRESCALE_VALUE 12
#elif CONFIG_SYSCLK_PSDIV == SYSCLK_PSDIV_24
#define SYSCLOCK_PRESCALE_VALUE 24
#elif CONFIG_SYSCLK_PSDIV == SYSCLK_PSDIV_48
#define SYSCLOCK_PRESCALE_VALUE 48
#else
#error Bad SYSCLK_PSDIR value
#endif

/**
 * Compute the system clock from macro
 */
#if CONFIG_SYSCLK_SOURCE == SYSCLK_SRC_RC20MHZ
   #define F_CPU (20000000/SYSCLOCK_PRESCALE_VALUE)
#elif CONFIG_SYSCLK_SOURCE == SYSCLK_SRC_RC16MHZ
   #define F_CPU (16000000/SYSCLOCK_PRESCALE_VALUE)
#elif (CONFIG_SYSCLK_SOURCE == SYSCLK_SRC_ULP32KHZ) || (CONFIG_SYSCLK_SOURCE == SYSCLK_SRC_RC32KHZ)
   #define F_CPU (32768/SYSCLOCK_PRESCALE_VALUE)
#elif CONFIG_SYSCLK_SOURCE == SYSCLK_SRC_XOSC
  #ifndef CONFIG_SYSCLK_EXT_FREQ
    #error Need to specify the external clock frequency
  #endif

  #define F_CPU (CONFIG_SYSCLK_EXT_FREQ/SYSCLOCK_PRESCALE_VALUE)
#endif


/** @} */

#ifndef __ASSEMBLY__

/**
 * \name Querying the system clock and its derived clocks
 */
//@{

/**
 * \brief Return the current rate in Hz of the main system clock
 *
 * \todo This function assumes that the main clock source never changes
 * once it's been set up, and that PLL0 always runs at the compile-time
 * configured default rate. While this is probably the most common
 * configuration, which we want to support as a special case for
 * performance reasons, we will at some point need to support more
 * dynamic setups as well.
 *
 * \return Frequency of the main system clock, in Hz.
 */
static inline uint32_t sysclk_get_main_hz(void)
{
	switch (CONFIG_SYSCLK_SOURCE) {
	case SYSCLK_SRC_RC20MHZ:
		return 2000000UL;
	case SYSCLK_SRC_ULP32KHZ:
   case SYSCLK_SRC_X32KHZ:
		return 32768UL;
#ifdef BOARD_XOSC_HZ
	case SYSCLK_SRC_XOSC:
		return BOARD_XOSC_HZ;
#endif
	default:
		//unhandled_case(CONFIG_SYSCLK_SOURCE);
		return 0;
	}
}


/**
 * \brief Return the current rate in Hz of clk_PER2.
 *
 * This clock can run up to two times faster than the CPU clock.
 *
 * \return Frequency of the clk_PER2 clock, in Hz.
 */
static inline uint32_t sysclk_get_cpu_hz(void)
{
	switch (CONFIG_SYSCLK_PSDIV) {
	case SYSCLK_PSDIV_1:
      return sysclk_get_main_hz();
	case SYSCLK_PSDIV_2:
   	return sysclk_get_main_hz() / 2;
	case SYSCLK_PSDIV_4:
   	return sysclk_get_main_hz() / 4;
	case SYSCLK_PSDIV_8:
	   return sysclk_get_main_hz() / 8;
	case SYSCLK_PSDIV_16:
	   return sysclk_get_main_hz() / 16;
	case SYSCLK_PSDIV_32:
	   return sysclk_get_main_hz() / 32;
	case SYSCLK_PSDIV_64:
	   return sysclk_get_main_hz() / 64;
	case SYSCLK_PSDIV_6:
	   return sysclk_get_main_hz() / 6;
	case SYSCLK_PSDIV_10:
   	return sysclk_get_main_hz() / 10;
	case SYSCLK_PSDIV_12:
   	return sysclk_get_main_hz() / 12;
	case SYSCLK_PSDIV_24:
	   return sysclk_get_main_hz() / 24;
	case SYSCLK_PSDIV_48:
	   return sysclk_get_main_hz() / 48;
	default:
		//unhandled_case(CONFIG_SYSCLK_PSBCDIV);
		return 0;
	}
}
//! \name Enabling and disabling synchronous clocks
//@{

//! \name System Clock Source and Prescaler configuration
//@{

/**
 * \brief Set system clock prescaler configuration
 *
 * This function will change the system clock prescaler configuration to
 * match the parameters.
 *
 * \note The parameters to this function are device-specific.
 *
 * \param psdiv The prescaler setting
 */
static inline void sysclk_set_prescalers(uint8_t psdiv)
{
	ccp_write_io((uint8_t *)&CLKCTRL.MCLKCTRLB, psdiv);
}

/**
 * \brief Change the source of the main system clock.
 *
 * \param src The new system clock source. Must be one of the constants
 * from the <em>System Clock Sources</em> section.
 */
static inline void sysclk_set_source(uint8_t src)
{
	ccp_write_io((uint8_t *)&CLKCTRL.MCLKCTRLA, src);
}

/**
 * \brief Lock the system clock configuration
 *
 * This function will lock the current system clock source and prescaler
 * configuration, preventing any further changes.
 */
static inline void sysclk_lock(void)
{
	ccp_write_io((uint8_t *)&CLKCTRL.MCLKLOCK, CLKCTRL_LOCK_bm);
}

//@}
/** @} */

//! \name System Clock Initialization
//@{

extern void sysclk_init(void);

//@}

#endif /* !__ASSEMBLY__ */

//! @}

#ifdef __cplusplus
}
#endif

#endif /* TINY2_SYSCLK_H_INCLUDED */

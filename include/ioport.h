/**
 * \file
 *
 * \brief Common IOPORT service main header file for AVR, UC3 and ARM
 *        architectures.
 *
 * Copyright (c) 2012-2020 Microchip Technology Inc. and its subsidiaries.
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
#ifndef IOPORT_H
#define IOPORT_H

#include "compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup ioport_group Common IOPORT API
 *
 * See \ref ioport_quickstart.
 *
 * This is common IOPORT service for GPIO pin configuration and control in a
 * standardized manner across the MEGA, MEGA_RF, XMEGA, UC3 and ARM devices.
 *
 * Port pin control code is optimized for each platform, and should produce
 * both compact and fast execution times when used with constant values.
 *
 * \section dependencies Dependencies
 * This driver depends on the following modules:
 * - \ref sysclk_group for clock speed and functions.
 * @{
 */

/**
 * \def IOPORT_CREATE_PIN(port, pin)
 * \brief Create IOPORT pin number
 *
 * Create a IOPORT pin number for use with the IOPORT functions.
 *
 * \param port IOPORT port (e.g. PORTA, PA or PIOA depending on chosen
 *             architecture)
 * \param pin IOPORT zero-based index of the I/O pin
 */
#define IOPORT_CREATE_PIN(port, pin) ((IOPORT_ ## port) * 8 + (pin))
#define PinDef(portDigit, pin) IOPORT_CREATE_PIN(PORT ## portDigit, pin)
#ifdef SIM
#define IOPORT_BASE_ADDRESS SIM_OFFSET(0x400)
#else
#define IOPORT_BASE_ADDRESS 0x400
#endif
#define IOPORT_VBASE_ADDRESS 0x0000
#define IOPORT_PORT_OFFSET  0x20
#define IOPORT_VPORT_OFFSET  0x4
#define IOPORT_PORTA  0
#define IOPORT_PORTB  1
#define IOPORT_PORTC  2

// Macros to use pin through VPORT_t
#define VPORT_OUT_OFFSET    0x02  // Offset of OUT register in each VPORT

/** \brief IOPORT pin directions */
enum ioport_direction {
	IOPORT_DIR_INPUT,  /*!< IOPORT input direction */
	IOPORT_DIR_OUTPUT, /*!< IOPORT output direction */
};

/** \brief IOPORT levels */
enum ioport_value {
	IOPORT_PIN_LEVEL_LOW,  /*!< IOPORT pin value low */
	IOPORT_PIN_LEVEL_HIGH, /*!< IOPORT pin value high */
};

/** \brief IOPORT edge sense modes */
enum ioport_sense {
	IOPORT_SENSE_BOTHEDGES, /*!< IOPORT sense both rising and falling edges */
	IOPORT_SENSE_FALLING,   /*!< IOPORT sense falling edges */
	IOPORT_SENSE_RISING,    /*!< IOPORT sense rising edges */
	IOPORT_SENSE_LEVEL_LOW, /*!< IOPORT sense low level  */
	IOPORT_SENSE_LEVEL_HIGH,/*!< IOPORT sense High level  */
   IOPORT_SENSE_DISABLE,
};

typedef uint8_t ioport_mode_t;
typedef uint8_t ioport_pin_t;
typedef uint8_t ioport_port_t;
typedef uint8_t ioport_port_mask_t;

__always_inline static ioport_pin_t ioport_create_pin(ioport_port_t port, uint8_t pin)
{
   return (port * 8) + pin;
}

__always_inline static ioport_port_t ioport_pin_to_port_id(ioport_pin_t pin)
{
   return pin >> 3;
}

__always_inline static PORT_t *arch_ioport_port_to_base(ioport_port_t port)
{
   return (PORT_t *)((uintptr_t)IOPORT_BASE_ADDRESS +
   (port * IOPORT_PORT_OFFSET));
}

__always_inline static VPORT_t *arch_ioport_port_to_vbase(ioport_port_t port)
{
   return (VPORT_t *)((uintptr_t)IOPORT_VBASE_ADDRESS +
   (port * IOPORT_VPORT_OFFSET));
}

__always_inline static void ioport_set_pin(ioport_pin_t pin)
{
   ioport_port_t port = ioport_pin_to_port_id(pin);
   VPORT_t *vport = arch_ioport_port_to_vbase(port);

   vport->OUT |= (1 << ioport_pin_to_port_id(pin));
}

__always_inline static void ioport_clear_pin(ioport_pin_t pin)
{
   ioport_port_t port = ioport_pin_to_port_id(pin);
   uint8_t bit = pin & 0x07;  // same as (pin % 8), but faster
   VPORT_t *vport = arch_ioport_port_to_vbase(port);

   vport->OUT &= ~(1 << bit);
}

__always_inline static PORT_t *ioport_pin_to_base(ioport_pin_t pin)
{
   return arch_ioport_port_to_base(ioport_pin_to_port_id(pin));
}

__always_inline static ioport_port_mask_t arch_ioport_pin_to_mask(
ioport_pin_t pin)
{
   return 1U << (pin & 0x07);
}

__always_inline static ioport_port_mask_t ioport_pin_to_index(
ioport_pin_t pin)
{
   return (pin & 0x07);
}

__always_inline static void arch_ioport_init(void)
{
}

__always_inline static void arch_ioport_enable_port(ioport_port_t port,
ioport_port_mask_t mask)
{
   PORT_t *base = arch_ioport_port_to_base(port);
   volatile uint8_t *pin_ctrl = &base->PIN0CTRL;

   uint8_t flags = cpu_irq_save();

   for (uint8_t i = 0; i < 8; i++) {
      if (mask & arch_ioport_pin_to_mask(i)) {
         pin_ctrl[i] &= ~PORT_ISC_gm;
      }
   }

   cpu_irq_restore(flags);
}

__always_inline static void arch_ioport_enable_pin(ioport_pin_t pin)
{
   PORT_t *base = ioport_pin_to_base(pin);
   volatile uint8_t *pin_ctrl
   = (&base->PIN0CTRL + ioport_pin_to_index(pin));

   uint8_t flags = cpu_irq_save();

   *pin_ctrl &= ~PORT_ISC_gm;

   cpu_irq_restore(flags);
}

__always_inline static void arch_ioport_disable_port(ioport_port_t port,
ioport_port_mask_t mask)
{
   PORT_t *base = arch_ioport_port_to_base(port);
   volatile uint8_t *pin_ctrl = &base->PIN0CTRL;

   uint8_t flags = cpu_irq_save();

   for (uint8_t i = 0; i < 8; i++) {
      if (mask & arch_ioport_pin_to_mask(i)) {
         pin_ctrl[i] |= PORT_ISC_INPUT_DISABLE_gc;
      }
   }

   cpu_irq_restore(flags);
}

__always_inline static void arch_ioport_disable_pin(ioport_pin_t pin)
{
   PORT_t *base = ioport_pin_to_base(pin);
   volatile uint8_t *pin_ctrl
   = (&base->PIN0CTRL + ioport_pin_to_index(pin));

   uint8_t flags = cpu_irq_save();

   *pin_ctrl |= PORT_ISC_INPUT_DISABLE_gc;

   cpu_irq_restore(flags);
}

__always_inline static void arch_ioport_set_port_mode(ioport_port_t port,
ioport_port_mask_t mask, ioport_mode_t mode)
{
   PORT_t *base = arch_ioport_port_to_base(port);
   volatile uint8_t *pin_ctrl = &base->PIN0CTRL;
   uint8_t new_mode_bits = (mode & ~PORT_ISC_gm);

   uint8_t flags = cpu_irq_save();

   for (uint8_t i = 0; i < 8; i++) {
      if (mask & arch_ioport_pin_to_mask(i)) {
         pin_ctrl[i]
         = (pin_ctrl[i] &
         PORT_ISC_gm) | new_mode_bits;
      }
   }

   cpu_irq_restore(flags);
}

__always_inline static void arch_ioport_set_pin_mode(ioport_pin_t pin,
ioport_mode_t mode)
{
   PORT_t *base = ioport_pin_to_base(pin);
   volatile uint8_t *pin_ctrl
   = (&base->PIN0CTRL + ioport_pin_to_index(pin));

   uint8_t flags = cpu_irq_save();

   *pin_ctrl &= PORT_ISC_gm;
   *pin_ctrl |= mode;

   cpu_irq_restore(flags);
}

__always_inline static void arch_ioport_set_port_dir(ioport_port_t port,
ioport_port_mask_t mask, enum ioport_direction dir)
{
   PORT_t *base = arch_ioport_port_to_base(port);

   if (dir == IOPORT_DIR_OUTPUT) {
      base->DIRSET = mask;
      } else if (dir == IOPORT_DIR_INPUT) {
      base->DIRCLR = mask;
   }
}

__always_inline static void arch_ioport_set_pin_dir(ioport_pin_t pin,
enum ioport_direction dir)
{
   PORT_t *base = ioport_pin_to_base(pin);

   if (dir == IOPORT_DIR_OUTPUT) {
      base->DIRSET = arch_ioport_pin_to_mask(pin);
      } else if (dir == IOPORT_DIR_INPUT) {
      base->DIRCLR = arch_ioport_pin_to_mask(pin);
   }
}

__always_inline static void arch_ioport_set_pin_level(ioport_pin_t pin,
bool level)
{
   PORT_t *base = ioport_pin_to_base(pin);

   if (level) {
      base->OUTSET = arch_ioport_pin_to_mask(pin);
      } else {
      base->OUTCLR = arch_ioport_pin_to_mask(pin);
   }
}

__always_inline static void arch_ioport_set_port_level(ioport_port_t port,
ioport_port_mask_t mask, enum ioport_value level)
{
   PORT_t *base = arch_ioport_port_to_base(port);
   if (level) {
      base->OUTSET |= mask;
      base->OUTCLR &= ~mask;
      } else {
      base->OUTSET &= ~mask;
      base->OUTCLR |= mask;
   }
}

__always_inline static bool arch_ioport_get_pin_level(ioport_pin_t pin)
{
   PORT_t *base = ioport_pin_to_base(pin);

   return base->IN & arch_ioport_pin_to_mask(pin);
}

__always_inline static ioport_port_mask_t arch_ioport_get_port_level(
ioport_port_t port, ioport_port_mask_t mask)
{
   PORT_t *base = arch_ioport_port_to_base(port);

   return base->IN & mask;
}

__always_inline static void arch_ioport_toggle_pin_level(ioport_pin_t pin)
{
   PORT_t *base = ioport_pin_to_base(pin);

   base->OUTTGL = arch_ioport_pin_to_mask(pin);
}

__always_inline static void arch_ioport_toggle_port_level(ioport_port_t port,
ioport_port_mask_t mask)
{
   PORT_t *base = arch_ioport_port_to_base(port);

   base->OUTTGL = mask;
}

__always_inline static void arch_ioport_set_pin_sense_mode(ioport_pin_t pin,
enum ioport_sense pin_sense)
{
   PORT_t *base = ioport_pin_to_base(pin);
   volatile uint8_t *pin_ctrl
   = (&base->PIN0CTRL + ioport_pin_to_index(pin));

   uint8_t flags = cpu_irq_save();

   *pin_ctrl &= ~PORT_ISC_gm;
   *pin_ctrl |= (pin_sense & PORT_ISC_gm);

   cpu_irq_restore(flags);
}

__always_inline static void arch_ioport_set_port_sense_mode(ioport_port_t port,
ioport_port_mask_t mask, enum ioport_sense pin_sense)
{
   PORT_t *base = arch_ioport_port_to_base(port);
   volatile uint8_t *pin_ctrl = &base->PIN0CTRL;
   uint8_t new_sense_bits = (pin_sense & PORT_ISC_gm);

   uint8_t flags = cpu_irq_save();

   for (uint8_t i = 0; i < 8; i++) {
      if (mask & arch_ioport_pin_to_mask(i)) {
         pin_ctrl[i]
         = (pin_ctrl[i] &
         ~PORT_ISC_gm) | new_sense_bits;
      }
   }

   cpu_irq_restore(flags);
}


/**
 * \brief Initializes the IOPORT service, ready for use.
 *
 * This function must be called before using any other functions in the IOPORT
 * service.
 */
static inline void ioport_init(void)
{
	arch_ioport_init();
}

/**
 * \brief Enable an IOPORT pin, based on a pin created with \ref
 * IOPORT_CREATE_PIN().
 *
 * \param pin  IOPORT pin to enable
 */
static inline void ioport_enable_pin(ioport_pin_t pin)
{
	arch_ioport_enable_pin(pin);
}

/**
 * \brief Enable multiple pins in a single IOPORT port.
 *
 * \param port IOPORT port to enable
 * \param mask Mask of pins within the port to enable
 */
static inline void ioport_enable_port(ioport_port_t port,
		ioport_port_mask_t mask)
{
	arch_ioport_enable_port(port, mask);
}

/**
 * \brief Disable IOPORT pin, based on a pin created with \ref
 *        IOPORT_CREATE_PIN().
 *
 * \param pin IOPORT pin to disable
 */
static inline void ioport_disable_pin(ioport_pin_t pin)
{
	arch_ioport_disable_pin(pin);
}

/**
 * \brief Disable multiple pins in a single IOPORT port.
 *
 * \param port IOPORT port to disable
 * \param mask Pin mask of pins to disable
 */
static inline void ioport_disable_port(ioport_port_t port,
		ioport_port_mask_t mask)
{
	arch_ioport_disable_port(port, mask);
}

/**
 * \brief Set multiple pin modes in a single IOPORT port, such as pull-up,
 * pull-down, etc. configuration.
 *
 * \param port IOPORT port to configure
 * \param mask Pin mask of pins to configure
 * \param mode Mode masks to configure for the specified pins (\ref
 * ioport_modes)
 */
static inline void ioport_set_port_mode(ioport_port_t port,
		ioport_port_mask_t mask, ioport_mode_t mode)
{
	arch_ioport_set_port_mode(port, mask, mode);
}

/**
 * \brief Set pin mode for one single IOPORT pin.
 *
 * \param pin IOPORT pin to configure
 * \param mode Mode masks to configure for the specified pin (\ref ioport_modes)
 */
static inline void ioport_set_pin_mode(ioport_pin_t pin, ioport_mode_t mode)
{
	arch_ioport_set_pin_mode(pin, mode);
}

/**
 * \brief Reset multiple pin modes in a specified IOPORT port to defaults.
 *
 * \param port IOPORT port to configure
 * \param mask Mask of pins whose mode configuration is to be reset
 */
static inline void ioport_reset_port_mode(ioport_port_t port,
		ioport_port_mask_t mask)
{
	arch_ioport_set_port_mode(port, mask, 0);
}

/**
 * \brief Reset pin mode configuration for a single IOPORT pin
 *
 * \param pin IOPORT pin to configure
 */
static inline void ioport_reset_pin_mode(ioport_pin_t pin)
{
	arch_ioport_set_pin_mode(pin, 0);
}

/**
 * \brief Set I/O direction for a group of pins in a single IOPORT.
 *
 * \param port IOPORT port to configure
 * \param mask Pin mask of pins to configure
 * \param dir Direction to set for the specified pins (\ref ioport_direction)
 */
static inline void ioport_set_port_dir(ioport_port_t port,
		ioport_port_mask_t mask, enum ioport_direction dir)
{
	arch_ioport_set_port_dir(port, mask, dir);
}

/**
 * \brief Set direction for a single IOPORT pin.
 *
 * \param pin IOPORT pin to configure
 * \param dir Direction to set for the specified pin (\ref ioport_direction)
 */
static inline void ioport_set_pin_dir(ioport_pin_t pin,
		enum ioport_direction dir)
{
	arch_ioport_set_pin_dir(pin, dir);
}

/**
 * \brief Set an IOPORT pin to a specified logical value.
 *
 * \param pin IOPORT pin to configure
 * \param level Logical value of the pin
 */
static inline void ioport_set_pin_level(ioport_pin_t pin, bool level)
{
	arch_ioport_set_pin_level(pin, level);
}

/**
 * \brief Set a group of IOPORT pins in a single port to a specified logical
 * value.
 *
 * \param port IOPORT port to write to
 * \param mask Pin mask of pins to modify
 * \param level Level of the pins to be modified
 */
static inline void ioport_set_port_level(ioport_port_t port,
		ioport_port_mask_t mask, enum ioport_value level)
{
	arch_ioport_set_port_level(port, mask, level);
}

/**
 * \brief Get current value of an IOPORT pin, which has been configured as an
 * input.
 *
 * \param pin IOPORT pin to read
 * \return Current logical value of the specified pin
 */
static inline bool ioport_get_pin_level(ioport_pin_t pin)
{
	return arch_ioport_get_pin_level(pin);
}

/**
 * \brief Get current value of several IOPORT pins in a single port, which have
 * been configured as an inputs.
 *
 * \param port IOPORT port to read
 * \param mask Pin mask of pins to read
 * \return Logical levels of the specified pins from the read port, returned as
 * a mask.
 */
static inline ioport_port_mask_t ioport_get_port_level(ioport_pin_t port,
		ioport_port_mask_t mask)
{
	return arch_ioport_get_port_level(port, mask);
}

/**
 * \brief Toggle the value of an IOPORT pin, which has previously configured as
 * an output.
 *
 * \param pin IOPORT pin to toggle
 */
static inline void ioport_toggle_pin_level(ioport_pin_t pin)
{
	arch_ioport_toggle_pin_level(pin);
}

/**
 * \brief Toggle the values of several IOPORT pins located in a single port.
 *
 * \param port IOPORT port to modify
 * \param mask Pin mask of pins to toggle
 */
static inline void ioport_toggle_port_level(ioport_port_t port,
		ioport_port_mask_t mask)
{
	arch_ioport_toggle_port_level(port, mask);
}

/**
 * \brief Set the pin sense mode of a single IOPORT pin.
 *
 * \param pin IOPORT pin to configure
 * \param pin_sense Edge to sense for the pin (\ref ioport_sense)
 */
static inline void ioport_set_pin_sense_mode(ioport_pin_t pin,
		enum ioport_sense pin_sense)
{
	arch_ioport_set_pin_sense_mode(pin, pin_sense);
}

/**
 * \brief Set the pin sense mode of a multiple IOPORT pins on a single port.
 *
 * \param port IOPORT port to configure
 * \param mask Bitmask if pins whose edge sense is to be configured
 * \param pin_sense Edge to sense for the pins (\ref ioport_sense)
 */
static inline void ioport_set_port_sense_mode(ioport_port_t port,
		ioport_port_mask_t mask,
		enum ioport_sense pin_sense)
{
	arch_ioport_set_port_sense_mode(port, mask, pin_sense);
}

/**
 * \brief Convert a pin ID into a bitmask mask for the given pin on its port.
 *
 * \param pin IOPORT pin ID to convert
 * \retval Bitmask with a bit set that corresponds to the given pin ID in its port
 */
static inline ioport_port_mask_t ioport_pin_to_mask(ioport_pin_t pin)
{
	return arch_ioport_pin_to_mask(pin);
}


#ifdef __cplusplus
}
#endif

#endif /* IOPORT_H */

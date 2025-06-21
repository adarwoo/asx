/**
 * @file
 * @{
 * mem.c
 * Poor's man dynamic allocation
 * Allow allocation of memory such as malloc, but as one way only (no free.)
 * Note: Check watchdog.c which does a RAM test and fills the RAM with 0xAA.
 *
 * @date: 19/05/2024 20:32:49
 * @author: software@arreckx.com
 * @}
 */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <avr/io.h>

#include "alert.h"

// These are added by the linker as 'information'
extern char __heap_start;
extern char __heap_end;


/** @def HEAP_MIN_STACK_SIZE
 * Reduce the heap size by at least this amount
 */
#ifndef HEAP_MIN_STACK_SIZE
#define HEAP_MIN_STACK_SIZE 64
#endif


/** @def HEAP_STACK_GUARD
 * Number of bytes to keep clear from the stack
 * When allocating, the allocator makes sure there is at least this
 * amount of un-touched memory to the current stack
 */
#ifndef HEAP_STACK_GUARD
#define HEAP_STACK_GUARD 32
#endif

/** Start of the heap as a 16-bit number */
#define HEAP_START ((uint16_t)&__heap_start)

/** Size of the heap as a number */
#define HEAP_SIZE (RAMEND - (uint16_t)&__heap_start - HEAP_MIN_STACK_SIZE)

/** Store the address to return for the next element */
static char *_heap_allocation_next_block = &__heap_start;

/**
 * Allocate some memory from the heap (what's left of the RAM passed the bss and data).
 * The memory content is cleared to all 0.
 * A check is done to detect if there is a risk a collision with the stack
 *  by checking there is at least HEAP_STACK_GUARD further unallocated bytes.
 * @return A pointer to the allocated block of memory initialized to 0
 */
void *mem_calloc(size_t __nele, size_t __size)
{
   size_t block_size = __nele * __size;

   char *retval = _heap_allocation_next_block;
   _heap_allocation_next_block += block_size;

   // Make sure the memory is un-allocated, including the guard
   char *check = retval;
   char *last = _heap_allocation_next_block + HEAP_STACK_GUARD;

   do
   {
      // Make sure the memory is untouched
      alert_and_stop_if(*check != 0xaa);
   } while ( ++check != last);

   // Zero the memory
   memset(retval, 0, block_size);

   return retval;
}

   /**
 * @addtogroup service
 * @{
 * @addtogroup piezzo
 * @{
 *****************************************************************************
 * Implementation of the piezzo API.
 * This API allows driving sounds and tunes to a piezzo or speaker.
 * It uses 1 of the TCB timers/
 * Note: By default a tune or single notes can be played.
 * It is possible to play a single note, which then plays over the tune.
 * This is used for transient sounds, or alarms.
 * When the single frequency stops, the tune recovers (the tune is not paused)
 *****************************************************************************
 * @file
 * Implementation of the piezzo API
 * @author software@arreckx.com
 * @internal
 */
#include <ctype.h>
#include <stdint.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include "board.h"
#include "ioport.h"
#include "reactor.h"
#include "timer.h"
#include "piezzo.h"

#include "conf_piezzo.h"

/************************************************************************/
/* Defines                                                              */
/************************************************************************/

/** @def PIEZZO_PRIO
 * Override the reactor priority of the digital output handler
 */
#ifndef PIEZZO_PRIO
#define PIEZZO_PRIO reactor_prio_high
#endif

#ifndef PIEZZO_TCB_NUMBER
#  define PIEZZO_TCB_NUMBER 0
#endif

#ifndef TIMER_TCB_NUMBER
#  define TIMER_TCB_NUMBER 1
#endif

#if PIEZZO_TCB_NUMBER == 0
#  define PIEZZO_TCB TCB0
#else
#  define PIEZZO_TCB TCB1
#endif

#if PIEZZO_TCB_NUMBER == TIMER_TCB_NUMBER
#  error Conflicting TIMERB allocation
#endif


/** Duration of a full note at 1 beat per minutes in ms (4(full) * 60(seconds) * 1000(ms)) */
#define TEMPO_FULL_NOTE_PERIOD 240000UL

/************************************************************************/
/* Local types                                                          */
/************************************************************************/

/** Internal state machine for parsing the notes */
typedef enum
{
   state_note,
   state_alteration,
   state_octave_shift,
   state_duration,
   state_space,
   state_done
} note_parsing_state_t;

struct piezzo_s
{
   /** Number of shift of the octave from the reference note (the lowest octave) */
   uint8_t octave_shift;

   /** Duration in ms of a full note at the given tempo */
   uint16_t tempo_full_period;

   /** Note duration in pow(2), so 0 is a full, and 2 a quaver */
   uint8_t note_duration_pow_number;

   /** If true, slur the notes and skip the added pause between notes */
   bool slur;

   /** Store the slur for the next note */
   bool slur_next;

   /** Calculated period for the note at the lowest octave */
   uint16_t ref_duration;

   /** Duration in timer count of the note or the rest */
   timer_count_t duration;

   /** Points to the character of the next note text description */
   const char *next_note;
} piezzo;

/**
 * Lookup table to convert a note to the PWM value.
 * The rows corresponds to flats, regular and sharps
 * The columns to C, D, E, F, G, A, B
 */
uint16_t _note_to_pwm[3][7] = {
    //     C      D      E      F      G      A      B
    /* Flat  */ {20248, 18039, 16071, 15169, 13514, 12039, 10726},
    /* Reg.  */ {19111, 17026, 15169, 14317, 12755, 11364, 10124},
    /* Sharp */ {18039, 16071, 14317, 13514, 12039, 10726, 9556},
};

/** Handle for the timing reactor */
reactor_handle_t react_piezzo;

/** Timer handler for stopping a tone */
reactor_handle_t react_tone_stop;

/** Instance of the current active timer or 0 if no timer is armed */
timer_instance_t timer_instance = 0;

/** Set to true to indicate a tone is playing rather than a tune */
bool playing_tone = false;

/** Playing tone on top of a tune */
uint16_t playing_tone_recovery_value = 0;

/*****************************************************************************/
/* Local methods                                                             */
/*****************************************************************************/

/** Helper to return the TC compare value */
static inline uint16_t _get_cmp_value(void)
{
   return piezzo.ref_duration >> piezzo.octave_shift;
}

static inline void _set_timer_compare_period(uint16_t new_tc_value)
{
   PIEZZO_TCB.CNT = 0;
   PIEZZO_TCB.CCMP = new_tc_value;

   // Enable the compare
   PIEZZO_TCB.CTRLB |= TCB_CCMPEN_bm;
}

static inline void _stop_timer_compare(void)
{
   PIEZZO_TCB.CTRLB &= ~TCB_CCMPEN_bm;
}

/** Internal parse a single note and change all global variables */
static inline void _parse_next_note()
{
   note_parsing_state_t state = state_note;
   size_t note_index = 0;
   size_t alt_index = 0;

   while (state != state_done)
   {
      char c = *(piezzo.next_note);

      if (c == '\0')
      {
         break;
      }

      switch (state)
      {
      case state_note:
         if (c >= 'A' && c <= 'G')
         {
            note_index = (c - 'A' + 5) % 7;
            state = state_alteration;
         }
         else if (c == 'R')
         {
            piezzo.ref_duration = 0;
            state = state_duration;
         }
         break;
      case state_alteration:
         if (c == 'b')
         {
            alt_index = 0;
         }
         else if (c == 'd')
         {
            alt_index = 2;
         }
         else
         {
            alt_index = 1;
            --piezzo.next_note;
         }
         state = state_octave_shift;
         break;
      case state_octave_shift:
         if (c == ',')
         {
            if (piezzo.octave_shift)
            {
               --piezzo.octave_shift;
            }
         }
         else if (c == '\'')
         {
            ++piezzo.octave_shift;
            piezzo.octave_shift %= 5;
         }
         else
         {
            --piezzo.next_note;

            piezzo.ref_duration = _note_to_pwm[alt_index][note_index];
            state = state_duration;
         }
         break;
      case state_duration:
         if (isdigit(c))
         {
            uint8_t shift_duration_by = c - '0';
            piezzo.duration = piezzo.tempo_full_period >> shift_duration_by;
         }
         else
         {
            --piezzo.next_note;
         }

         state = state_space;
         break;
      case state_space:
         piezzo.slur_next = (c == '~');
         state = state_done;
         break;
      default:
         break;
      }

      // Advance to the next char in every case (including to skip the space or slur)
      ++piezzo.next_note;
   }
}

/** Internal to handle the tune */
void _play_next_note(void *arg)
{
   static uint16_t last_tc_value = 0;
   static uint16_t new_tc_value = 0;

   if (*(piezzo.next_note) != '\0')
   {
      piezzo.slur = piezzo.slur_next;

      _parse_next_note();

      new_tc_value = _get_cmp_value();
      playing_tone_recovery_value = new_tc_value;

      if (last_tc_value != new_tc_value || piezzo.slur == false)
      {
         if (!playing_tone)
         {
            if (new_tc_value)
            {
               _set_timer_compare_period(new_tc_value);
            }
            else
            {
               _stop_timer_compare();
            }
         }
      }

      last_tc_value = new_tc_value;

      // Re-arm the timer
      timer_instance = timer_arm(
         react_piezzo, timer_get_count_from_now(piezzo.duration), 0, NULL);
   }
   else
   {
      playing_tone_recovery_value = 0;

      if (!playing_tone)
      {
         _stop_timer_compare();
      }
   }
}

/** Called to stop a single tone*/
static void _stop_tone(void *arg)
{
   playing_tone = false;

   if ( playing_tone_recovery_value )
   {
      _set_timer_compare_period(playing_tone_recovery_value);
   }
   else
   {
      _stop_timer_compare();
   }

   playing_tone_recovery_value = 0;
}


/************************************************************************/
/* Public API                                                                     */
/************************************************************************/

void piezzo_init(void)
{
   // Ready the PWM
#ifndef _WIN32
   // Use the Timer type A to drive the piezzo transistor directly
   PIEZZO_TCB.CTRLA = TCB_CLKSEL_DIV1_gc | TCB_ENABLE_bm;
   PIEZZO_TCB.CTRLB = TCB_CNTMODE_INT_gc;

   // Set the WO.x as output
   #if PIEZZO_TCB_NUMBER == 0
      PORTA_DIRSET = _BV(5);
   #else
      PORTA_DIRSET = _BV(3);
   #endif

#endif
   // This timer does not have a PWM output when using all 16bits.

   // Create the reactor handler
   react_piezzo = reactor_register(_play_next_note, PIEZZO_PRIO);

   react_tone_stop = reactor_register(_stop_tone, PIEZZO_PRIO);
}

/**
 * Play one note or a whole music sheet
 * Any previous play are canceled right away to make room for this piezzo.
 * The tune is played once and only once.
 *
 * @param _tempo The number of quarter notes to play per minutes from 40 to 255.
 * @param music A music string containing the tune or a single note
 *              The content of the string must remain valid for the duration of the piezzo.
 * @example Set the string to:
 *          "C3 R C E G E G E D R D F A2~A3 B G E B G E C' R B, C'~C1"
 */
void piezzo_play(uint8_t tempo, const char *music)
{
   piezzo.next_note = music;
   piezzo.octave_shift = 2; // Mid octave to start with
   piezzo.tempo_full_period = (TEMPO_FULL_NOTE_PERIOD / tempo);
   piezzo.note_duration_pow_number = 2; // Assume a Quaver to start with
   // Duration of a quarter note
   piezzo.duration = piezzo.tempo_full_period >> piezzo.note_duration_pow_number;
   piezzo.slur_next = false;

   // Stop ongoing tune
   if (timer_instance)
   {
      timer_cancel(timer_instance);
   }

   // Start a new timer for the first note
   _play_next_note(NULL);
}

/**
 * Stop the piezzo
 */
void piezzo_stop(void)
{
   // Stop ongoing tune
   if (timer_instance)
   {
      timer_cancel(timer_instance);
   }

   _stop_timer_compare();
}

/**
 * Play a single tone on top of whatever is playing right now
 * This tone takes over for the given duration.
 * If the duration is 0, plays for ever/
 */
void piezzo_start_tone(uint16_t pwm_value, timer_count_t duration)
{
   // Restart the timer with the new value
   _set_timer_compare_period(pwm_value);

   // If a duration is given, start a timer
   if ( duration )
   {
      timer_arm(react_tone_stop, timer_get_count_from_now(duration), 0, 0);
   }

   playing_tone = true;
}

/** Stop playing the tone */
void piezzo_stop_tone(void)
{
   _stop_tone(NULL);
}


/**@}*/
/**@}*/
/**@} ---------------------------  End of file  --------------------------- */
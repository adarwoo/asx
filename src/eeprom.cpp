/**
 * Implementation of the eeprom API
 */
#include <asx/eeprom.hpp>
#include <asx/reactor.hpp>

#include <avr/io.h>
#include <avr/interrupt.h>

#include <array>

#include <alert.h>

namespace asx
{
   namespace eeprom
   {
      // Reactor handle
      auto react_on_eeprom_ready = asx::reactor::bind(Operation::on_eeprom_ready);

      /**
       * \brief Non-Volatile Memory Execute Command
       *
       * This function sets the CCP register before setting the CMDEX bit in the
       * NVMCTRL.CTRLA register.
       *
       * \note The correct NVM command must be set in the NVMCTRL.CMD register before
       *       calling this function.
       */
      void issue_cmd(uint8_t page, uint8_t command)
      {
         // Calculate page address
         uint16_t address = (uint16_t)(EEPROM_START + page * EEPROM_PAGE_SIZE);

         wait_til_ready();

         // Set address
         NVMCTRL.ADDRH = (address >> 8) & 0xFF;
         NVMCTRL.ADDRL = address & 0xFF;

         ccp_write_spm((uint8_t *)&NVMCTRL.CTRLA, command);
      }

      // AVR bitshift requires n iteration
      static inline const std::array<uint8_t, 8> bits_mask{
          0b01111111,
          0b00111111,
          0b00011111,
          0b00001111,
          0b00000111,
          0b00000011,
          0b00000001,
          0b00000000};

      // Manages eeprom operations to eliminate busy waits
      // An internal queue of the max number of pages is created
      void Operation::request_operation()
      {
         operations.push(static_cast<Operation *>(this));

         // Activate the interrupt
         // If the eeprom is aleady available, the interrupt will fire right away
         // Otherwise, the reactor will claim that time for other use and the interrupt will
         //  fire to indicate it is ready to accept more operations.
         NVMCTRL.INTCTRL |= NVMCTRL_EEREADY_bm;
      }

      // Reactor handler
      void Operation::on_eeprom_ready()
      {
         operations.pop()->do_operation();
      }

      Counter::Counter(uint8_t PAGE) : counter{0}, page{PAGE}, bankpos{0xff}, bitpos{0}, bytepos{0}
      {
         // Locate as good bank within the page
         // If several banks found, pick the one with the highest count
         for (uint8_t i = 0; i < bank_count; ++i)
         {
            bank_t *pBank = get_bank0_ptr() + i;

            if (pBank->counter == ~(pBank->not_counter))
            {
               if (pBank->counter >= counter)
               {
                  counter = pBank->counter;
                  bankpos = i;
               }
            }
         }

         // Has a bank been identified?
         if (bankpos == 0xff)
         {
            // First time!
            bankpos = 0;

            // Format the eeprom page
            get_bank0_ptr()->counter = 0;
            memset(get_bank0_ptr()->bit_bank, 0xff, bytes_in_bank);
            get_bank0_ptr()->not_counter = 0xffffffff;
            erase_and_write_page(page);
         }
         else
         {
            // Count the bits
            for (bytepos = 0; bytepos < bytes_in_bank; ++bytepos)
            {
               uint8_t value = get_bank_ptr()->bit_bank[bytepos];

               if (value == 0)
               {
                  counter += 8;
                  continue;
               }
               
               if (value != 0xff)
               {
                  for (bitpos = 0; bitpos < 8; ++bitpos)
                  {
                     if (value == bits_mask[bitpos])
                     {
                        counter += bitpos;
                        break;
                     }
                  }

                  // Not possible - so format the byte
                  // We've may have gained/lost up to 4 minutes in the process - but saved the counter
                  if (bitpos == 8)
                  {
                     counter -= 4; // Take an average
                     bitpos = 0;
                     get_bank_ptr()->bit_bank[bytepos] = 0xff;
                     write_page(page);
                  }
               }
               
               break;
            }               
         }
         
         if (bytepos == bytes_in_bank )
         {
            // The previous bank is full
            bytepos = 0;
            bitpos = 0;
            bankpos = (bankpos + 1) % bank_count;
            op = Op::new_bank_set_new;
            do_operation();
         }
      }

      /***
       * Increment the counter by one.
       * This method should be fast. The eeprom write cycle will happen in the
       * background.
       * If several count take place simultaneously, a delay may occur
       */
      void Counter::increment()
      {
         ++counter;
         ++bitpos;

         if (bitpos == 8)
         {
            bitpos = 0;

            if (++bytepos == bytes_in_bank)
            {
               bytepos = 0;

               // Bank is full = use a new bank
               // We do not check if the bank is erased, as the init function guarantees it
               bankpos = (bankpos + 1) % bank_count;

               op = Op::new_bank_set_new;
            }
            else
            {
               op = Op::update_bits;
            }
         }
         else
         {
            op = Op::update_bits;
         }

         request_operation();
      }

      // Called indirectly from the reactor when the eeprom is ready to accept new operations
      void Counter::do_operation()
      {
         bank_t *bankptr = get_bank_ptr();

         switch (op)
         {
         case Op::update_bits:
            bankptr->bit_bank[bytepos] = bits_mask[bitpos];
            write_page(page); // No erase - just write!
            break;
         case Op::new_bank_set_new:
            bankptr->counter = counter;
            memset((void *)bankptr->bit_bank, 0xff, bytes_in_bank);
            bankptr->not_counter = ~counter;
            erase_and_write_page(page);
            break;

         default:
            break;
         }
      }

   } // namespace eeprom
} // namespace asx

ISR(NVMCTRL_EE_vect)
{
   // This is a level interrupt - unless disabled, it will be called again an again
   NVMCTRL.INTCTRL = 0;
   // Clear the flag
   NVMCTRL.INTFLAGS = NVMCTRL_EEREADY_bm;
   asx::eeprom::react_on_eeprom_ready();
}

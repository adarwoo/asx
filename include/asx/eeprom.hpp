#pragma once
// MIT License
//
// Copyright (c) 2025 software@arreckx.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#include <avr/io.h>

#include <cstdint>
#include <cstring>
#include <array>
#include <ccp.h>

#include <asx/utils.hpp>

/**
 * EEprom management
 * The API supports 32bits persistent counters and arbitrary data storage.
 * The data storage is protected by a checksum, and is automatically formatted
 * The counter reside in eeprom proper, whereas the data in the user row page.
 *
 * Example:
 *  auto count_minutes = asx::eeprom::Counter{0}; // Use page 0
 *  count_minutes::increment();
 *  auto c = count_minutes::get_count();
 *
 *  // Store arbitrary data. Create a struct to organise the data
 *  struct MyData {
 *      enum parity_t : uint8_t { none, odd, even };
 *      uint8_t address;
 *      uint16_t baud;
 *      uint8_t stopbits;
 *      parity_t parity;
 *
 *      bool invert[3];
 *      bool default_pos[3];
 *      uint16_t watchdog;
 *  };
 *
 * // Specialise a Storage of MyData. Pass-in default values in the ctor
 * static auto my_data = asx::eeprom::Storage<MyData> {
 *   .address = 44,
 *   .baud = 9600,
 *   .stopbits = 1,
 *   .parity = MyData::none,
 *   .invert = {0, 0, 0},
 *   .default_pos = {0, 0, 0},
 *   .watchdog = 0
 * };
 *
 * // Read the actual values from the eeprom
 * auto device_id = my_data.address;
 *
 * // To change the storage, request for change passing a reactor handler
 * mydata.baud = 115200;
 * mydata.update(); // The function returns immediatly
 * // Note: Further changes will be taken into account until the data is written
 */

namespace asx
{
   namespace eeprom
   {
      using addr_t = uint16_t;

      // Consts
      static constexpr auto bank_size = 16;
      static constexpr auto bank_count = EEPROM_PAGE_SIZE / bank_size;
      static constexpr auto page_count = EEPROM_SIZE / EEPROM_PAGE_SIZE;
      static constexpr auto bytes_in_bank = bank_size - 2 * sizeof(uint32_t);
      static constexpr auto bits_in_bank = bytes_in_bank * 8;

      /** @return true if the eeprom is busy */
      static inline bool is_busy()
      {
         return NVMCTRL.STATUS & NVMCTRL_EEBUSY_bm;
      }

      /**
       * \brief Wait for any NVM access to finish.
       *
       * This function is blocking and waits for any NVM access to finish.
       * Use this function before any NVM accesses, if you are not certain that
       * any previous operations are finished yet.
       */
      static inline void wait_til_ready()
      {
         do
         {
            // Block execution while waiting for the NVM to be ready
         } while (is_busy());
      }

      /**
       * \brief Non-Volatile Memory Execute Command
       *
       * This function sets the CCP register before setting the CMDEX bit in the
       * NVMCTRL.CTRLA register.
       *
       * \note The correct NVM command must be set in the NVMCTRL.CMD register before
       *       calling this function.
       */
      void issue_cmd(uint8_t page, uint8_t command);

      static inline void write_page(uint8_t page)
      {
         issue_cmd(page, NVMCTRL_CMD_PAGEWRITE_gc);
      }

      static inline void erase_page(uint8_t page)
      {
         issue_cmd(page, NVMCTRL_CMD_PAGEERASE_gc);
      }

      static inline void erase_and_write_page(uint8_t page)
      {
         issue_cmd(page, NVMCTRL_CMD_PAGEERASEWRITE_gc);
      }

      static inline void erase()
      {
         ccp_write_spm((uint8_t *)&NVMCTRL.CTRLA, NVMCTRL_CMD_EEERASE_gc);
      }

      // Manages eeprom operations to eliminate busy waits
      // An internal queue of the max number of pages is created
      class Operation
      {
      protected:
         /** Request an operation on the eeprom when the eeprom must be ready */
         void request_operation();
         virtual void do_operation() = 0;

      public: // For the interrupt only
         ///< Reactor handler
         static void on_eeprom_ready();

      private:
         // Allow for as many counters, user row
         inline static FixedPtrQueue<Operation, page_count + 1> operations{};
      };

      /*
       * New strategy
       * This implements a 32-bits counter
       * The storage is fixed to 64 bytes per counter made of 4 16 bytes banks
       * The principle is as follow:
       * 1 - Look for the current bank
       *  -> Check valid bank
       *    -> 1st byte != 0xff and opposite DWord [14] in bank == ~counter
       *  -> If no bank - format bank 0
       *  -> If 2, use the biggest and format the other
       *  -> If only 1, use it
       * 2 - On increment, only 1 bit is changed
       *  -> When all bits are 0, write in the other bank the new count, then format the current
       * The should get 16 millions cycles.
       *
       */
      class Counter : public Operation
      {
         struct bank_t
         {
            uint32_t counter;
            uint8_t bit_bank[bytes_in_bank];
            uint32_t not_counter;
         };

         uint32_t counter;
         uint8_t page;
         uint8_t bankpos;
         uint8_t bitpos;  // 0-7
         uint8_t bytepos; // 0-bytes_in_bank

         enum class Op : uint8_t
         {
            idle,
            update_bits,
            new_bank_set_new,
            new_bank_erase_old
         } op = Op::idle;

         inline uint16_t get_current_page_address() const
         {
            return EEPROM_START + EEPROM_PAGE_SIZE * page;
         }

         inline bank_t *get_bank0_ptr() const
         {
            return (bank_t *)get_current_page_address();
         }

         bank_t *get_bank_ptr() const
         {
            return get_bank0_ptr() + bankpos;
         }

      public:
         Counter(uint8_t PAGE);

         /***
          * Increment the counter by one.
          * This method should be fast. The eeprom write cycle will happen in the
          * background.
          * If several count take place simultaneously, a delay may occur
          */
         void increment();

         uint32_t get_count()
         {
            return counter;
         }

      protected:
         virtual void do_operation() override;
      };

      /**
       * Storage into the user row of user data
       * There are no banks for this data. The eeprom is small and an update cycle is 4ms.
       * Either the risk is acceptable, or the powersupply should guarantee 4ms hole.
       * The VLM is activated to prevent a write with a dwingling power.
       */
      template <typename T, unsigned DATA_VERSION = 0>
      class Storage : public T, public Operation
      {
#define pEEData ((uint8_t *)(&USERROW))
#define fletcher16_ptr ((uint16_t *)(&USERROW.USERROW30))
         static constexpr auto size = sizeof(T);

         static_assert(sizeof(T) < USER_SIGNATURES_SIZE - 2, "User row too small for the requested data");

         static uint16_t calc_fletcher16(uint8_t *pData)
         {
            uint16_t sum1 = DATA_VERSION;
            uint16_t sum2 = 0xff;

            for (uint8_t index = 0; index < size; ++index)
            {
               sum1 = (sum1 + pData[index]) % 255;
               sum2 = (sum2 + sum1) % 255;
            }

            return (sum2 << 8) | sum1;
         }

      public:
         /**
          * Ready the storage using the user_data as the default value, and overwritting if
          * the eeprom content is valid.
          * Access to the NVStorage is assumed free at this stage
          */
         explicit Storage(const T &initial_data) : T{initial_data}
         {
            // Check the eeprom content
            uint16_t fl16 = calc_fletcher16(pEEData);

            // Update the content
            if (*fletcher16_ptr != fl16)
            {
               const T *pDefault = static_cast<T *>(this);

               memcpy(pEEData, (void *)pDefault, size);
               *fletcher16_ptr = calc_fletcher16((uint8_t *)pDefault);
               NVMCTRL.ADDR = (uint16_t)(&USERROW);
               wait_til_ready();
               ccp_write_spm((uint8_t *)&NVMCTRL.CTRLA, NVMCTRL_CMD_PAGEERASEWRITE_gc);
            }
            else
            {
               memcpy((void *)static_cast<T *>(this), (void *)pEEData, size);
            }
         }

         // Delete default constructor
         Storage() = delete;

         // Delete copy constructor
         Storage(const Storage &) = delete;

         // Delete move constructor
         Storage(Storage &&) = delete;

         // Delete copy assignment operator
         Storage &operator=(const Storage &) = delete;

         // Delete move assignment operator
         Storage &operator=(Storage &&) = delete;

         T &operator=(const T& from) {
            memcpy((void *)static_cast<T *>(this), (void *)&from, size);
            return *this;
         }

         void update()
         {
            request_operation();
         }

      protected:
         // Called to make eeprom operation - guaranting the eeprom is ready
         virtual void do_operation() override
         {
            const T *pDefault = static_cast<T *>(this);

            memcpy(pEEData, (void *)pDefault, size);
            *fletcher16_ptr = calc_fletcher16((uint8_t *)pDefault);
            NVMCTRL.ADDR = (uint16_t)(&USERROW);
            ccp_write_spm((uint8_t *)&NVMCTRL.CTRLA, NVMCTRL_CMD_PAGEERASEWRITE_gc);
         }
      };

   } // namespace eeprom
} // namespace asx
#include <avr/interrupt.h>
#include <asx/i2c_master.hpp>
#include <alert.h>
#include "board.h"


namespace asx {
   namespace i2c {
      void Master::init(const unsigned long bus_speed_hz) {
         TWI0.MBAUD = calc_baud(bus_speed_hz);
         TWI0.MCTRLB |= TWI_FLUSH_bm;
         TWI0.MCTRLA = TWI_RIEN_bm | TWI_WIEN_bm | TWI_ENABLE_bm;
         TWI0.MSTATUS = TWI_BUSSTATE_IDLE_gc;

         react_on_complete = reactor::bind(on_complete);

         status = status_code_t::STATUS_OK;
      }

      void Master::write_handler() {
         if (addr_count < pkg->addr_length) {
            const uint8_t *const data = pkg->addr;
            TWI0.MDATA = data[addr_count++];
         } else if (data_count < pkg->length) {

            if (read) {
               // Send repeated START condition (Address|R/W=1)
               TWI0.MADDR |= 0x01;
            } else {
               const uint8_t *const data = pkg->buffer;
               TWI0.MDATA = data[data_count++];
            }
         } else {
            // Send STOP condition to complete the transaction
            TWI0.MCTRLB = TWI_MCMD_STOP_gc;
            status = status_code_t::STATUS_OK;

            // Notify the reactor
            react_on_complete(status_code_t::STATUS_OK);
         }
      }

      /**
       * \internal
       *
       * \brief TWI master read interrupt handler.
       *
       *  This is the master read interrupt handler that takes care of
       *  reading bytes from the TWI slave.
       */
      void Master::read_handler() {
         if (data_count < pkg->length) {
            uint8_t *const data = pkg->buffer;

            data[data_count++] = TWI0.MDATA;

            /* If there is more to read, issue ACK and start a byte read.
            * Otherwise, issue NACK and STOP to complete the transaction.
            */
            if (data_count < pkg->length) {
               TWI0.MCTRLB = TWI_MCMD_RECVTRANS_gc;
            } else {
               TWI0.MCTRLB = TWI_ACKACT_bm | TWI_MCMD_STOP_gc;
               status = status_code_t::STATUS_OK;

               react_on_complete(status_code_t::STATUS_OK);
            }
         } else {
            /* Issue STOP and buffer overflow condition. */
            TWI0.MCTRLB = TWI_MCMD_STOP_gc;
            status = status_code_t::ERR_NO_MEMORY;

            react_on_complete(status_code_t::STATUS_OK);
         }
      }

      /// @brief Initiate a i2c transfer (read/write/read+write)
      /// The function initiate the transfer and return immediately.
      /// The reactor handle in the package is notified when the transfer is complete
      /// @param package The package holding the information about the transfer
      /// @param _read If true, reads, else write or write+read
      void Master::transfer(Package &package, bool _read) {
         pkg = &package;
         addr_count = 0;
         data_count = 0;
         read = _read;

         uint8_t const chip = (pkg->chip) << 1;

         // This API uses a reactor - so no collision should ever occur
         alert_and_stop_if( not is_idle() );

         // Writing the MADDR register kick starts things
         if (pkg->addr_length || (false == read)) {
            TWI0.MADDR = chip;
         } else if (read) {
            TWI0.MADDR = chip | 0x01;
         }
      }

      void Master::interrupt_handler() {
         uint8_t const master_status = TWI0.MSTATUS;

         if (master_status & TWI_ARBLOST_bm) {
            TWI0.MSTATUS = master_status | TWI_ARBLOST_bm;
            TWI0.MCTRLB = TWI_MCMD_STOP_gc;
            status = status_code_t::ERR_BUSY;

            react_on_complete(status_code_t::ERR_BUSY);
         } else if ((master_status & TWI_BUSERR_bm) || (master_status & TWI_RXACK_bm)) {
            TWI0.MCTRLB = TWI_MCMD_STOP_gc;
            status = status_code_t::ERR_IO_ERROR;

            react_on_complete(status_code_t::ERR_IO_ERROR);
         } else if (master_status & TWI_WIF_bm) {
            write_handler();
         } else if (master_status & TWI_RIF_bm) {
            read_handler();
         } else {
            status = status_code_t::ERR_PROTOCOL;

            react_on_complete(status_code_t::ERR_PROTOCOL);
         }
      }

      void Master::check_pending() {
         // Must be idle
         if ( is_idle() ) {
            auto next = requests.pop();
            next();
         }
      }
   }
}

extern "C"
{
    // Interrupt handler
    ISR(TWI0_TWIM_vect) {
        asx::i2c::Master::interrupt_handler();
    }
}
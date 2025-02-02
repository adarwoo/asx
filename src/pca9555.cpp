#include <asx/pca9555.hpp>

namespace asx {
   namespace i2c {
      namespace {
         ///< Package
         static i2c::Package package;
      }

      PCA9555::PCA9555(uint8_t _chip) {
         chip = base_chip_address | _chip;
         package.buffer = buffer;
      }

      void PCA9555::read(uint8_t count, command_t op, CompleteCb cb) {
         package.chip = chip;
         package.addr[0] = (uint8_t)op;
         package.addr_length = 1;
         package.length = count; // Number of bytes to read
         package.on_complete = cb;

         Master::transfer(package, true);
      }

      // Set a value and read
      void PCA9555::transfer(command_t op, uint16_t value, CompleteCb cb) {
         package.chip = chip;
         package.addr[0] = (uint8_t)op;
         package.addr[1] = value >> 8;
         package.addr[2] = value & 0xff;
         package.addr_length = 3;
         package.on_complete = cb;
         package.length = 0;

         Master::transfer(package);
      }

      // Set a value and read
      void PCA9555::transfer(command_t op, uint8_t value, CompleteCb cb) {
         package.chip = chip;
         package.addr[0] = (uint8_t)op;
         package.addr[1] = value;
         package.addr_length = 2;
         package.on_complete = cb;
         package.length = 0;

         Master::transfer(package);
      }
   }
}

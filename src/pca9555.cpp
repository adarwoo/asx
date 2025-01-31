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

      void PCA9555::read(command_t op, reactor::Handle react) {
         package.chip = chip;
         package.addr[0] = (uint8_t)op;
         package.addr_length = 1;
         package.length = 1; // Number of bytes to read
         package.react_on_complete = react;
         Master::transfer(package, true);
      }

      // Set a value and read
      void PCA9555::transfer(command_t op, uint16_t value, reactor::Handle react) {
         package.chip = chip;
         package.addr[0] = (uint8_t)op;
         package.addr[1] = value >> 8;
         package.addr[2] = value & 0xff;
         package.addr_length = 3;
         package.react_on_complete = react;

         Master::transfer(package);
      }

      // Set a value and read
      void PCA9555::transfer(command_t op, uint8_t value, reactor::Handle react) {
         package.chip = chip;
         package.addr[0] = (uint8_t)op;
         package.addr[1] = value;
         package.addr_length = 2;
         package.react_on_complete = react;

         Master::transfer(package);
      }
   }
}

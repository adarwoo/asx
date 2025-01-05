#pragma once
#include <asx/i2c_master.hpp>

namespace asx {
   namespace i2c {
      class PCA9555 {
         enum class operation_t : uint8_t {
            conf_dir,
            conf_pol,
            conf_output,
         };

         enum class command_t : uint8_t
         {
            read = 0,
            write = 2,
            set_polarity = 4,
            set_dir = 6,
         };

         static constexpr auto base_chip_address = uint8_t{0b0100000};

         ///< Common Buffer to receive data
         static inline uint8_t buffer[2] = {};

         ///< Address of the device
         uint8_t chip;

      public:
         PCA9555(uint8_t _chip);

         void read(reactor::Handle react = reactor::null);

         void set_value(uint16_t value, reactor::Handle react = reactor::null) {
            transfer(command_t::write, value, react);
         }

         void set_dir(uint16_t dir, reactor::Handle react = reactor::null) {
            transfer(command_t::set_dir, dir, react);
         }

         void set_pol(uint16_t pol, reactor::Handle react = reactor::null) {
            transfer(command_t::set_polarity, pol, react);
         }

         ///< Access the common read buffer
         static uint16_t get_value() {
            return buffer[1]<<8 | buffer[0];
         }
      private:
         // Set a value and read
         void transfer(command_t op, uint16_t value, reactor::Handle react);
      };
   }
}

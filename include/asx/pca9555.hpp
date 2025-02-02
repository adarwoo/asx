#pragma once
#include <asx/i2c_master.hpp>
#include <type_traits>

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
            read0 = 0,
            read1 = 1,
            write0 = 2,
            write1 = 3,
            set_pol0 = 4,
            set_pol1 = 5,
            set_dir0 = 6,
            set_dir1 = 7,
         };

         static constexpr auto base_chip_address = uint8_t{0b0100000};

         ///< Common Buffer to receive data
         static inline uint8_t buffer[2] = {};

         ///< Address of the device
         uint8_t chip;

      public:
         PCA9555(uint8_t _chip);

         void read(CompleteCb cb=nullptr) {
            read(2, command_t::read0, cb);
         }

         void set_value(uint16_t value, CompleteCb cb=nullptr) {
            transfer(command_t::write0, value, cb);
         }

         void set_dir(uint16_t dir, CompleteCb cb=nullptr) {
            transfer(command_t::set_dir0, dir, cb);
         }

         void set_pol(uint16_t pol, CompleteCb cb=nullptr) {
            transfer(command_t::set_pol0, pol, cb);
         }

         template<unsigned PORT>
         void read(CompleteCb cb=nullptr) {
            static_assert(PORT==0 or PORT==1, "Invalid port");
            if constexpr ( PORT == 0 ) {
               read(1, command_t::read0, cb);
            } else {
               read(1, command_t::read1, cb);
            }
         }

         template<unsigned PORT>
         void set_value(uint8_t value, CompleteCb cb=nullptr) {
            static_assert(PORT==0 or PORT==1, "Invalid port");
            if constexpr ( PORT == 0 ) {
               transfer(command_t::write0, value, cb);
            } else {
               transfer(command_t::write1, value, cb);
            }
         }

         template<unsigned PORT>
         void set_dir(uint8_t dir, CompleteCb cb=nullptr) {
            static_assert(PORT==0 or PORT==1, "Invalid port");
            if constexpr ( PORT == 0 ) {
               transfer(command_t::set_dir0, dir, cb);
            } else {
               transfer(command_t::set_dir1, dir, cb);
            }
         }

         template<unsigned PORT>
         void set_pol(uint8_t pol, CompleteCb cb=nullptr) {
            static_assert(PORT==0 or PORT==1, "Invalid port");
            if constexpr ( PORT == 0 ) {
               transfer(command_t::set_pol0, pol, cb);
            } else {
               transfer(command_t::set_pol1, pol, cb);
            }
         }

         template <typename T>
         T get_value() {
            static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t>,
               "Only uint8_t or uint16_t are supported.");
            if constexpr (std::is_same_v<T, uint8_t>) {
               return buffer[0];
               } else { // T is uint16_t
               return (static_cast<uint16_t>(buffer[0]) << 8) | buffer[1];
            }
         }

      private:
         // Set a value and read
         void transfer(command_t op, uint16_t value, CompleteCb cb=nullptr);
         void transfer(command_t op, uint8_t value, CompleteCb cb=nullptr);

         void read(uint8_t count, command_t op, CompleteCb cb=nullptr);
      };
   }
}

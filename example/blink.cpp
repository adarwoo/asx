#include <chrono> // For the ""s
#include <asx/reactor.hpp>
#include <asx/eeprom.hpp>

#include <conf_board.h>

using namespace asx;
using namespace asx::ioport;
using namespace std::chrono;

// Keep a persistent counter
auto count_blink = asx::eeprom::Counter{0}; // Use page 0

// Called by the reactor every second
auto flash_led() -> void {
   Pin(MY_LED).toggle();
   count_blink.increment();
}


// Store arbitrary data. Create a struct to organise the data
struct MyData {
   enum parity_t : uint8_t { none, odd, even };
   uint8_t address;
   uint16_t baud;
   uint8_t stopbits;
   parity_t parity;
   
   bool invert[3];
   bool default_pos[3];
   uint16_t watchdog;
};

static auto my_data = asx::eeprom::Storage<MyData>({
   .address = 44,
   .baud = 9600,
   .stopbits = 1,
   .parity = MyData::none,
   .invert = {0, 0, 0},
   .default_pos = {0, 0, 0},
   .watchdog = 0
});

// Initialise all and go
auto main() -> int {
   Pin(MY_LED).init(dir_t::out, value_t::high);

   reactor::bind(flash_led).repeat(1s);
   reactor::run();
}

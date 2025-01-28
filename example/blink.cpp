#include <chrono> // For the ""s
#include <asx/reactor.hpp>

#include <conf_board.h>

using namespace asx;
using namespace asx::ioport;


// Called by the reactor every second
auto flash_led() -> void {
   Pin(MY_LED).toggle();
}


// Initialise all and go
auto main() -> int {
   Pin(MY_LED).init(dir_t::out, value_t::high);

   using namespace std::chrono;
   reactor::bind(flash_led).repeat(1s);
   reactor::run();
}

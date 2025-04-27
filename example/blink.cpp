/**
 * Demonstrates the asx library with the reactor and the timer
 * The compiled code in release mode is <2Kb.
 * This includes the reactor and the timer library.
 */
#include <chrono> // For the operator ""s

#include <asx/ioport.hpp>
#include <asx/reactor.hpp>

#include <conf_board.h>

using namespace asx;
using namespace asx::ioport;
using namespace std::chrono;


// Initialise all and go
auto main() -> int {
   Pin(MY_LED).init(dir_t::out, value_t::high);

   reactor::bind([]{Pin(MY_LED).toggle();}).repeat(1s);
   reactor::run();
}

# asx
<b>ASX</b> is a core library for building C and C++ application for the ATTiny2 family.

ASX comes standard with:
<ul>
   <li>C libraries remeniscent of the ASF library from Atmel</li>
   <li>Modern TMP C++ libraries</li>
   <li>A C/C++ reactor engine, to execute asynchronous tasks</li>
   <li>A C/C++ timer service, integrated to the reactor</li>
   <li>The boost sml library for incredible state machines</li>
   <li>The standard C++ library ported to the AVR</li>
   <li>Memory allocation support, and memory fencing</li>
   <li>A logger, with 0 cost logger for release builds</li>
   <li>A C++ modbus library, with ICD compiler written in Python</li>
   <li>A built-in build environment using Docker. Works on Linux and WSL2</li>
   <li>Makefile to build in docker, Windows native (including in AVR studio)</li>
</ul>

<h2>The reactor engine</h2>
The reactor engine allows binding arbitry functions to have them called in the 'main' processing space (as opposed to inside interrupt context).
With the reactor, asynchronous events can be managed collaboratively without the need for synchronisation nor the risk of race condition.
The reactor loop takes 17us to evaluate the next handler, and the notification (to promote a reactor), takes 1us.

The reactor is the ideal companion to your interrupts. Interrupt should simply extract the the data, and notify the reactor (ring the bell!) passing the data along.

The actually handling of the data takes place in the main loop.

The downside of the reactor is the lack of control over the latency.
To ease the problem, the reactor has priorities, so reactor with higher importance gets served first.

The worse case latency is always going to be the longest possible handler.

So the reactor works nicely when handling is quick.
For functions that take up more time, the handling should be sliced.

The reactor integrates a timer, so repeating reactors or delayed are possible.
Timer can also be safely stopped without the fear of a race conditions.

<h2> Other words </h2>
Finally, the ASX framework comes with all the goodies from the ASF project (which Microchip did not port to the Tiny2 familly which are very close to the XMega) as well as the full standard C++ library.

It is designed to compile with a C++17 or C++20 compiler.

Example code:

```C++
#include <chrono> // For the ""s
#include <asx/reactor.hpp>

#include <conf_board.h>

using namespace asx;
using namespace asx::ioport;
using namespace std::chrono;

// Called by the reactor every second
auto flash_led() -> void {
   Pin(MY_LED).toggle();
}

// Initialise all and go
auto main() -> int {
   Pin(MY_LED).init(dir_t::out, value_t::high);

   reactor::bind(flash_led).repeat(1s);
   reactor::run();
}
```
  
   
    
  

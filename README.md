# asx
<b>ASX</b> is a core library for building C and C++ application for the ATTiny2 family.
It is based on the reactor pattern.
The core library is less than 2KB of code.
With ASX, use elements of the standard C++ library, like type_traits, chrono, array at
next to no cost.

ASX comes standard with:
<ul>
   <li>C libraries remeniscent of the ASF library from Atmel</li>
   <li>Modern Template Meta Programming C++ libraries</li>
   <li>A C/C++ reactor engine, to execute asynchronous tasks</li>
   <li>A C/C++ timer service, integrated to the reactor</li>
   <li>The boost sml library for incredible state machines</li>
   <li>The standard C++ library ported to the AVR</li>
   <li>Memory allocation support, and memory fencing</li>
   <li>ULOG an almost 0 cost trace library (Flash, RAM and Time) - yet fully featured!</li>
   <li>A C++ modbus library, with ICD compiler written in Python</li>
   <li>A built-in build environment using Docker. Works on Linux and WSL2</li>
   <li>Makefile to build in docker, Windows native (including in AVR studio)</li>
</ul>

<h2>The reactor engine</h2>
The reactor engine allows binding arbitry functions to have them called in the 'main' processing space (as opposed to interrupt context).
With the reactor, asynchronous events can be managed collaboratively without the need for synchronisation nor the risk of race condition.
The reactor loop takes 17us to evaluate the next handler, and the notification (to promote a reactor), takes 1us.

The reactor is the ideal companion to your interrupts. Interrupt should simply extract the the data, and notify the reactor (ring the bell!) passing the data along.

The actually handling of the data takes place in the main loop.

The downside of the reactor is the lack of control over the latency.
But no worries!
1. The reactor has priorities, so a reactor handler with higher importance gets served first
2. Handlers can yield time to other handlers, so long handling can be split to allow for a higher level handler to kick in

The worse case latency is always going to be the longest possible.
The reactor can drive a debug pin to analyse which handlers may take too long.
In the rare cases a handler is taking too long at the wrong time, it's processing should be sliced using the yield function.

The reactor integrates a timer, so repeating reactors or delayed are possible.
Timer can also be safely stopped without the fear of a race conditions.

<h2> Other words </h2>
Finally, the ASX framework comes with all the goodies from the ASF project (which Microchip did not port to the Tiny2 familly which are very close to the XMega) as well as the full standard C++ library.

It is designed to compile with a C++20 compiler, but comes with a Docker image which includes everything required.

<h3> Working in Microchip studio </h3>
To use with Atmel/Microchip studio, the toolchain will require upgrading from the aging 5.4! to a 14.0 or better.
Check https://github.com/ZakKemble/avr-gcc-build/releases for the release binaries.
Simply download the pre-build Windows image and place in
<pre>C:\Program Files (x86)\Atmel\Studio\7.0\toolchain\avr8</pre>
Rename the folder 'avr8-gnu-toolchain' as 'avr8-gnu-toolchain-5.4'
...and create a link 'avr8-gnu-toolchain' pointing to the toolchain.

Example code:

```C++
#include <chrono>          // For the ""s
#include <asx/ioport.hpp>  // Include before conf_board
#include <asx/reactor.hpp> // Reactor API
#include <asx/ulog.hpp>

#include "conf_board.h"

using namespace asx;
using namespace asx::ioport;
using namespace std::chrono;

// Initialise all and go
auto main() -> int {
   ULOG_MILE("Starting the test application!");
   Pin(MY_LED).init(ioport::dir_t::out, ioport::value_t::high);

   reactor::bind([]{ULOG_TRACE("Blink!"); Pin(MY_LED).toggle();}).repeat(1s);
   reactor::run();
}
```

The code is <2Kb when compiled in release mode, and the main block is 82bytes - including the log!

Check the example directory which includes a make file

<h1>How to build</h1>
A docker/podman container image build file is provided for the build.
The makefile creates the image and builds using it.
So GNU Make is requires, as well as Docker or Podman.

For the version control, gitman is used to handle dependencies.
To try out the blink example, you need to have gitman installed.

<h2>Building in Windows with WSL</h2>
If you are using Visual Studio Code, start a WSL terminal:

```sh
# Install gitman
pipx install gitman
# Get ASX
git clone https://github.com/adarwoo/asx
gitman update
# Make sure Docker-CE/podman is running in Windows!
cd example
make
```


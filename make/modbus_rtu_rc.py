#!/usr/bin/env python3
"""
Define the slave commands

The modbus dictionary take 2 kw callbacks and devices
The device key takes an address give following @ symbol writen in decimal or hexadecimal with 0x preceding

The callbacks is a dictionary of callback function name and their list of arguments.
Each argument must be of any of the following type:
  u8, u16, u32 : Unsigned integer of the given length
  s8, s16, s32 : Signed
  f32 : Floats
A tuple of type and variable name can be provided.

The devices part is a list of modbus commands expressed as tuples.
The tuple is made of:
  A modbus RTU type command. Must be one of the following:
        read_coils (0x01)
        read_discrete_inputs     (0x02)
        read_holding_registers   (0x03)
        read_input_registers     (0x04)
        write_single_coil	     (0x05)
        write_single_register    (0x06)
        write_multiple_coils     (0x0f)
        write_multiple_registers (0x10)
        read_write_multiple_registers  (0x17)

  Then an number of constructed type object. Each object can take the following arguments:
     A single value
     A range (2 values, from inclusive, to exclusive)
     A list (possible values)
     None - to allow any value

   Warning: As the tree is constructed, each command must have a unique path. Overlapping path will
    generate a compile error.

   Finally, the name of the callback. This name must exists in the callbacks section.
   The parameters are converted to the host representation and cast.
   The size is checked during cast. A range 0-0x200 cannot be cast to an 8-bit.
"""
import re

TEMPLATE_CODE_MASTER="""#pragma once
/**
 * This file was generated to create a state machine for processing
 * uart data used for a modbus RTU.
 */
#include <cstdint>

#include <ulog.h>
#include <asx/modbus_rtu_master.hpp>

namespace @NAMESPACE@ {
    // All callbacks registered
    @PROTOTYPES@

    // All states to consider
    enum class state_t : uint8_t {
        IGNORE = 0,
        ERROR = 1,
        BAD_REQUEST, // The slave indicates an error
        BAD_REQUEST__CRC,
        BAD_REQUEST_CONFIRMED,
        @ENUMS@
    };

    class Datagram {
        using error_t = asx::modbus::error_t;

        ///< Adjusted buffer to only receive the largest amount of data possible
        inline static uint8_t buffer[@BUFSIZE@];
        ///< Number of characters in the buffer
        inline static uint8_t cnt;
        ///< Number of characters to send
        inline static uint8_t frame_size;
        ///< Error code
        inline static error_t error;
        ///< State
        inline static state_t state;
        ///< CRC for the datagram
        inline static asx::modbus::Crc crc{};
        ///< Expected reply address
        inline static uint8_t expected_address;
        ///< Expected reply op code
        inline static uint8_t expected_command;

        static inline auto ntoh(const uint8_t offset) -> uint16_t {
            return (static_cast<uint16_t>(buffer[offset]) << 8) | static_cast<uint16_t>(buffer[offset + 1]);
        }

        static inline auto ntohl(const uint8_t offset) -> uint32_t {
            return
                (static_cast<uint32_t>(buffer[offset]) << 24) |
                (static_cast<uint32_t>(buffer[offset+1]) << 16) |
                (static_cast<uint32_t>(buffer[offset+2]) << 8) |
                static_cast<uint16_t>(buffer[offset+3]);
        }

    public:
        // Status of the datagram
        enum class status_t : uint8_t {
            GOOD_FRAME = 0,
            NOT_FOR_ME = 1,
            BAD_CRC = 2
        };

        static void reset() noexcept {
            cnt=0;
            crc.reset();
            error = error_t::ok;
            state = state_t::DEVICE_ADDRESS;
        }

        static status_t get_status() noexcept {
            if (state == state_t::IGNORE) {
                return status_t::NOT_FOR_ME;
            }

            return crc.check() ? status_t::GOOD_FRAME : status_t::BAD_CRC;
        }

        static void process_char(const uint8_t c) noexcept {
            ULOG_DEBUG0("Processing char: 0x{:2x} at position {}", c, cnt);

            if (state == state_t::IGNORE) {
                return;
            }

            // Compute the CRC on the go
            crc(c);

            // Keep count
            ++cnt;

            switch(state) {
            case state_t::ERROR:
                break;
            @CASES@
            case state_t::BAD_REQUEST:
                state = state_t::BAD_REQUEST__CRC;
                break;
            case state_t::BAD_REQUEST__CRC:
                if ( cnt == 5 ) {
                    state = state_t::BAD_REQUEST_CONFIRMED;
                }
                break;
            default:
                error = error_t::illegal_data_value;
                state = state_t::ERROR;
                break;
            }

            if (state != state_t::ERROR) {
                // Store the frame
                buffer[cnt-1] = c; // Store the data
            }
        }

        template<typename T>
        static void pack(const T& value) noexcept {
            if constexpr ( sizeof(T) == 1 ) {
                buffer[cnt++] = value;
            } else if constexpr ( sizeof(T) == 2 ) {
                buffer[cnt++] = value >> 8;
                buffer[cnt++] = value & 0xff;
            } else if constexpr ( sizeof(T) == 4 ) {
                buffer[cnt++] = value >> 24;
                buffer[cnt++] = value >> 16 & 0xff;
                buffer[cnt++] = value >> 8 & 0xff;
                buffer[cnt++] = value & 0xff;
            }
        }

        static void pack(const asx::modbus::command_t cmd) noexcept {
            buffer[cnt++] = static_cast<uint8_t>(cmd);
        }

        /** Called when a T3.5 has been detected, in a good sequence */
        static error_t process_reply() noexcept {
            auto retval = error_t::ok;

            switch(state) {
            @CALLBACKS@
            case state_t::BAD_REQUEST_CONFIRMED:
                // Make sure the error is compatible
                if ( buffer[2] > 0 && buffer[2] < static_cast<uint8_t>(error_t::unknown_error) ) {
                    retval = static_cast<error_t>(buffer[2]);
                } else {
                    retval = error_t::unknown_error;
                }
                break;
            default:
                retval = error_t::ignore_frame;
                break;
            }
            return retval;
        }

        /** Called when a T3.5 has been detected, in a good sequence */
        static void ready_request() noexcept {
            // Add the CRC
            crc.reset();
            auto _crc = crc.update(std::string_view{(char *)buffer, cnt});
            buffer[cnt++] = _crc & 0xff;
            buffer[cnt++] = _crc >> 8;
        }

        static std::string_view get_buffer() noexcept {
            // Return the buffer ready to send
            return std::string_view{(char *)buffer, cnt};
        }

        static void initiate_transmit(uint8_t slave_addr, asx::modbus::command_t cmd) noexcept {
            cnt = 0;
            expected_address = buffer[cnt++] = slave_addr;
            expected_command = buffer[cnt++] = static_cast<uint8_t>(cmd);
        }
    }; // struct Processor
} // namespace modbus"""

TEMPLATE_CODE_SLAVE="""#pragma once
/**
 * This file was generated to create a state machine for processing
 * uart data used for a modbus RTU. It should be included by
 * the modbus_rtu_slave.cpp file only which will create a full rtu slave device.
 */
#include <cstdint>
#include <ulog.h>
#include <asx/modbus_rtu_slave.hpp>

namespace @NAMESPACE@ {
    // All callbacks registered
    @PROTOTYPES@

    // All states to consider
    enum class state_t : uint8_t {
        IGNORE = 0,
        ERROR = 1,
        @ENUMS@
    };

    // Code 43 / 14 object category
    enum class object_code_t : uint8_t {
        BASIC_DEVICE_IDENTIFICATION = 0x01,
        REGULAR_DEVICE_IDENTIFICATION = 0x02,
        EXTENDED_DEVICE_IDENTIFICATION = 0x03,
        SPECIFIC_DEVICE_IDENTIFICATION = 0x04
    };


    class Datagram {
        using error_t = asx::modbus::error_t;

        @DEVICE_ADDRESS@
        ///< Adjusted buffer to only receive the largest amount of data possible
        inline static uint8_t buffer[@BUFSIZE@];
        ///< Number of characters in the buffer
        inline static uint8_t cnt;
        ///< Number of characters to send
        inline static uint8_t frame_size;
        ///< Error code
        inline static error_t error;
        ///< State
        inline static state_t state;
        ///< CRC for the datagram
        inline static asx::modbus::Crc crc{};

        static inline auto ntoh(const uint8_t offset) -> uint16_t {
            return (static_cast<uint16_t>(buffer[offset]) << 8) | static_cast<uint16_t>(buffer[offset + 1]);
        }

        static inline auto ntohl(const uint8_t offset) -> uint32_t {
            return
                (static_cast<uint32_t>(buffer[offset]) << 24) |
                (static_cast<uint32_t>(buffer[offset+1]) << 16) |
                (static_cast<uint32_t>(buffer[offset+2]) << 8) |
                static_cast<uint16_t>(buffer[offset+3]);
        }

    public:
        // Status of the datagram
        enum class status_t : uint8_t {
            GOOD_FRAME = 0,
            NOT_FOR_ME = 1,
            BAD_CRC = 2
        };

        @set_device_address@
        static void reset() noexcept {
            cnt=0;
            crc.reset();
            error = error_t::ok;
            state = state_t::DEVICE_ADDRESS;
        }

        static status_t get_status() noexcept {
            if (state == state_t::IGNORE) {
                return status_t::NOT_FOR_ME;
            }

            return crc.check() ? status_t::GOOD_FRAME : status_t::BAD_CRC;
        }

        static void process_char(const uint8_t c) noexcept {
            ULOG_DEBUG0("Processing char: 0x{:2x} at position {}", c, cnt);

            if (state == state_t::IGNORE) {
                return;
            }

            crc(c);

            if (state != state_t::ERROR) {
                // Store the frame
                buffer[cnt++] = c; // Store the data
            }

            switch(state) {
            case state_t::ERROR:
                break;
            @CASES@
            default:
                error = error_t::illegal_data_value;
                state = state_t::ERROR;
                break;
            }
        }

        static void reply_error( error_t err ) noexcept {
            buffer[1] |= 0x80;
            buffer[2] = (uint8_t)err;
            cnt = 3;
        }

        template<typename T>
        static void pack(const T& value) noexcept {
            if constexpr ( sizeof(T) == 1 ) {
                buffer[cnt++] = value;
            } else if constexpr ( sizeof(T) == 2 ) {
                buffer[cnt++] = value >> 8;
                buffer[cnt++] = value & 0xff;
            } else if constexpr ( sizeof(T) == 4 ) {
                buffer[cnt++] = value >> 24;
                buffer[cnt++] = value >> 16 & 0xff;
                buffer[cnt++] = value >> 8 & 0xff;
                buffer[cnt++] = value & 0xff;
            }
        }

        static void pack(const char *v) noexcept {
            auto length = strlen(v);
            memcpy(&buffer[cnt], v, length);
            cnt += length;
        }

        static inline void set_size(uint8_t size) {
            cnt = size;
        }

        /** Called when a T3.5 has been detected, in a good sequence */
        static void ready_reply() noexcept {
            frame_size = cnt; // Store the frame size
            cnt = 2; // Points to the function code
            @READY_REPLY_CALLBACK@

            switch(state) {
            case state_t::IGNORE:
                break;
            @INCOMPLETE@
                error = error_t::illegal_data_value;
            case state_t::ERROR:
                buffer[1] |= 0x80; // Mark the error
                buffer[2] = (uint8_t)error; // Add the error code
                cnt = 3;
                break;
            @CALLBACKS@
            default:
                break;
            }

            // If the cnt is 2 - nothing was changed in the buffer - return it as is
            if ( cnt == 2 ) {
                // Framesize includes the previous CRC which still holds valid
                cnt = frame_size;
            } else {
                // Add the CRC
                crc.reset();
                auto _crc = crc.update(std::string_view{(char *)buffer, cnt});
                buffer[cnt++] = _crc & 0xff;
                buffer[cnt++] = _crc >> 8;
            }
        }

        static std::string_view get_buffer() noexcept {
            // Return the buffer ready to send
            return std::string_view{(char *)buffer, cnt};
        }
    }; // struct Processor

    @SLAVE_ID_FUNCTION@

    @SLAVE_READ_ID_REQUEST@

    inline void on_diagnostics() {}
} // namespace @NAMESPACE@"""

# Regex to check the device address (and extract it)
DEVICE_ADDR_RE = re.compile(r'device@((?:0x)?([0-9a-fA-F]+))')

# Regex pattern for a valid C function name
VALID_C_FUNCTION_NAME = re.compile(r'^[a-zA-Z_][a-zA-Z0-9_]*$')

# Indent by
INDENT = " " * 4

class Integral:
    """Base class for integral types."""
    bits = None

    def __init__(self, i):
        """ Set the value """
        self.value = i

    @property
    def size(self):
        """Return the size in bytes of the integral type."""
        return self.bits // 8

class _8bits(Integral):
    bits = 8
    ctype = "uint8_t"

class _16bits(Integral):
    bits = 16
    ctype = "uint16_t"

class _32bits(Integral):
    bits = 32
    ctype = "uint32_t"

class Matcher:
    """Base Matcher class for different integral types."""

    def __init__(self, *args, **kwargs):
        if len(args) == 0 or (len(args) == 1 and args[0] is None):
            self.value = None
        elif len(args) == 1:
            value = args[0]

            if isinstance(value, list):
                for _value in value:
                    self.check(_value)
                self.value = value # Assign the list
            else:
                self.value = self.cast(args[0])
        elif len(args) == 2:
            self.value = Range(self.cast(args[0]), self.cast(args[1]))
        else:
            raise ValueError(f"Invalid arguments for {self.__class__.__name__}")

        if "alias" in kwargs:
            self.alias = kwargs["alias"]
        else:
            self.alias = None

        self.pos = None # To be set later

    def __eq__(self, other):
        if not isinstance(other, Matcher):
            return False  # Not comparable with non-Matcher types

        # Compare class types
        if self.__class__ != other.__class__:
            return False

        # Compare the `value` attribute
        if type(self.value) != type(other.value):
            return False

        if self.value != other.value:
            return False

        # If all checks pass, the objects are equal
        return True

    def cast(self, value):
        """Check and return the value if valid, otherwise raise an error."""
        if self.check(value):
            return value
        raise ValueError(f"Cannot cast the value {value}")

    def check(self, value):
        """Check if the value is valid (to be implemented in subclasses)."""
        raise NotImplementedError

    def fits(self, item):
        """ @return False if the item size is fitting with the given matcher """
        v = item(0)

        # Is it big enough!
        if v.size >= self.size:
            return True

        # 2 cases left 8 for a 16
        if isinstance(self.value, Range):
            # For the range, we need to check if the min and max are fitting
            return v.min <= self.value._from and v.max >= self.value._to
        elif isinstance(self.value, list):
            # Make sure all values for within the min-max range
            for t in self.value:
                if t < v.min or t > v.max:
                    return False

        return True

    def __repr__(self):
        return f"{self.ctype}({self.value})"

    def to_code(self):
        if isinstance(self.value, Range):
            if self.value._from == 0 and isinstance(self, UnsignedMatcher):
                return f"c <= {self.value._to}"
            return f"c >= {self.value._from} and c <= {self.value._to}"
        elif isinstance(self.value, list):
            return " || ".join(f"c == {hex(value)}" for value in self.value)
        elif self.value == None:
            return None
        else:
            return f"c == {self.value}"

class Range:
    """Simple Range class to hold value ranges."""
    def __init__(self, from_value, to_value):
        self._from = from_value
        self._to = to_value
    def __repr__(self):
        return f"[{self._from}-{self._to}]"

    def __eq__(self, other):
        return self._from == other._from and self._to == other._to

class UnsignedMatcher(Matcher):
    """Matcher for unsigned integral types."""
    def check(self, value):
        return isinstance(value, int) and (0 <= value < (1 << self.bits))
    @property
    def max(self):
        return (1 << self.bits) - 1
    @property
    def min(self):
        return 0

class SignedMatcher(Matcher):
    """Matcher for signed integral types."""
    def check(self, value):
        return isinstance(value, int) and (-(1 << (self.bits - 1)) <= value < (1 << (self.bits - 1)))
    @property
    def max(self):
        return (1 << (self.bits - 1)) - 1
    @property
    def min(self):
        return -(1 << (self.bits - 1))

# Concrete Matcher classes for various types
class u8(UnsignedMatcher, _8bits): pass
class u16(UnsignedMatcher, _16bits): pass
class u32(UnsignedMatcher, _32bits): pass
class s8(SignedMatcher, _8bits): pass
class s16(SignedMatcher, _16bits): pass
class s32(SignedMatcher, _32bits): pass
class f32(Matcher, _32bits):
    def check(self, value):
        return isinstance(value, float)
class Crc(UnsignedMatcher, _16bits):
    _bits = -16 # Negative for little endian
    def to_code(self):
        return "true"
class RuntimeDeviceAddress(UnsignedMatcher, _8bits):
    def to_code(self):
        return "c == device_address";

READ_COILS                    = u8(0x01, alias="READ_COILS")
READ_DISCRETE_INPUTS          = u8(0x02, alias="READ_DISCRETE_INPUTS")
READ_HOLDING_REGISTERS        = u8(0x03, alias="READ_HOLDING_REGISTERS")
READ_INPUT_REGISTERS          = u8(0x04, alias="READ_INPUT_REGISTERS")
WRITE_SINGLE_COIL             = u8(0x05, alias="WRITE_SINGLE_COIL")
WRITE_SINGLE_REGISTER         = u8(0x06, alias="WRITE_SINGLE_REGISTER")
WRITE_MULTIPLE_COILS          = u8(0x0F, alias="WRITE_MULTIPLE_COILS")
WRITE_MULTIPLE_REGISTERS      = u8(0x10, alias="WRITE_MULTIPLE_REGISTERS")
READ_WRITE_MULTIPLE_REGISTERS = u8(0x17, alias="READ_WRITE_MULTIPLE_REGISTERS")
REPORT_SLAVE_ID               = u8(0x11, alias="REPORT_SLAVE_ID")
ENCAPSULATED_INTERFACE_TRANSPORT = u8(0x2B, alias="ENCAPSULATED_INTERFACE_TRANSPORT")
CUSTOM                        = u8(0x65, alias="CUSTOM")

READ_DEVICE_IDENTIFICATION    = u8(0x2B, alias="READ_DEVICE_IDENTIFICATION")

# MEI Object Codes
VENDOR_NAME = 0x00
PRODUCT_CODE = 0x01
MAJOR_MINOR_REVISION = 0x02
VENDOR_URL = 0x03
PRODUCT_NAME = 0x04
MODEL_NAME = 0x05
USER_APPLICATION_NAME = 0x06
PRIVATE_OBJECTS_0 = 0x80
PRIVATE_OBJECTS_1 = 0x81
PRIVATE_OBJECTS_2 = 0x82
PRIVATE_OBJECTS_3 = 0x83
PRIVATE_OBJECTS_4 = 0x84
PRIVATE_OBJECTS_5 = 0x85
PRIVATE_OBJECTS_6 = 0x86
PRIVATE_OBJECTS_7 = 0x87
# Non MEI object code
SLAVE_ID = 0xFF07

# MEI Classification Codes
BASIC_DEVICE_IDENTIFICATION = 0x01
REGULAR_DEVICE_IDENTIFICATION = 0x02
EXTENDED_DEVICE_IDENTIFICATION = 0x03
SPECIFIC_DEVICE_IDENTIFICATION = 0x04

# Group of MEI Object Codes
MEI_OBJECT_CATEGORY = {
    VENDOR_NAME:           BASIC_DEVICE_IDENTIFICATION,
    PRODUCT_CODE:          BASIC_DEVICE_IDENTIFICATION,
    MAJOR_MINOR_REVISION:  BASIC_DEVICE_IDENTIFICATION,
    VENDOR_URL:            REGULAR_DEVICE_IDENTIFICATION,
    PRODUCT_NAME:          REGULAR_DEVICE_IDENTIFICATION,
    MODEL_NAME:            REGULAR_DEVICE_IDENTIFICATION,
    USER_APPLICATION_NAME: REGULAR_DEVICE_IDENTIFICATION,
    PRIVATE_OBJECTS_0:     EXTENDED_DEVICE_IDENTIFICATION,
    PRIVATE_OBJECTS_1:     EXTENDED_DEVICE_IDENTIFICATION,
    PRIVATE_OBJECTS_2:     EXTENDED_DEVICE_IDENTIFICATION,
    PRIVATE_OBJECTS_3:     EXTENDED_DEVICE_IDENTIFICATION,
    PRIVATE_OBJECTS_4:     EXTENDED_DEVICE_IDENTIFICATION,
    PRIVATE_OBJECTS_5:     EXTENDED_DEVICE_IDENTIFICATION,
    PRIVATE_OBJECTS_6:     EXTENDED_DEVICE_IDENTIFICATION,
    PRIVATE_OBJECTS_7:     EXTENDED_DEVICE_IDENTIFICATION,
}


class Transition:
    """ Represents a test which triggers a callback or a transition """
    def __init__(self, matcher, next_state):
        self.matcher = matcher
        self.next = next_state
        self.set_crc = False

    def is_crc(self):
        return self.next is not None and isinstance(self.next.ops, Operation)

    def to_code(self, indent):
        tab = INDENT * indent
        opening = close = ""
        test = self.matcher.to_code()
        has_test = test is not None

        if has_test:
            opening += f"if ( {test} ) {{\n{tab}{INDENT}"
            close += f"\n{tab}}}"
        else:
            tab = ""

        return has_test, f"{opening}state = state_t::{self.next.name};{close}"

class TransitionGroup:
    """ Holds a group of matchers of the same type """
    def __init__(self, integral, pos):
        self.integral = integral
        self.pos = pos
        self.transitions = []

    def to_code(self, indent):
        tab = INDENT * indent
        retval = str()
        next_flag = False
        size = self.integral.size
        extra_indent = 1 if size > 1 else 0
        extra = INDENT * extra_indent
        test_cnt = 0
        data = str()

        # Skip if CRC - we don't compute the CRC
        if not self.transitions[0].next.name.startswith('RDY_TO_CALL'):
            if size == 2: # Redefine c
                data = f"{tab}{extra}auto c = ntoh(cnt-2);\n\n"
            elif size == 4:
                data = f"{tab}{extra}auto c = ntohl(cnt-4);\n\n"

            for matcher in self.transitions:
                if next_flag:
                    retval += " else "
                else:
                    retval += tab+extra

                next_flag = True
                has_test, to_append = matcher.to_code(indent+extra_indent)
                retval += to_append
                test_cnt += 1 if has_test else 0

            if test_cnt > 0:
                retval = data + retval + f" else {{"

                if self.pos == 0:
                    retval += f"\n{tab}{extra}{INDENT}error = error_t::ignore_frame;"
                    retval += f"\n{tab}{extra}{INDENT}state = state_t::IGNORE;"
                elif self.pos == 1:
                    retval += f"\n{tab}{extra}{INDENT}error = error_t::illegal_function_code;"
                    retval += f"\n{tab}{extra}{INDENT}state = state_t::ERROR;"
                else:
                    retval += f"\n{tab}{extra}{INDENT}error = error_t::illegal_data_value;"
                    retval += f"\n{tab}{extra}{INDENT}state = state_t::ERROR;"
            else:
                pass

            if size == 1:
                return retval
        else:
            t=self.transitions[0]
            return f"{tab}if ( cnt == {self.pos+size} ) {{\n{tab}{INDENT}state = state_t::{t.next.name};"

        if ( test_cnt ):
            return f"{tab}if ( cnt == {self.pos+size} ) {{\n{retval}\n{INDENT}{tab}}}"

        return f"{tab}if ( cnt == {self.pos+size} ) {{\n{retval}"

class Operation:
    def __init__(self, name, prototype, chain):
        self.name = name
        self.prototype = prototype
        self.chain = chain

    def to_code(self):
        # Check the prototype to see if we need to pass the buffer data
        # Create from the end
        values_str = []
        chain = [] + self.chain # Force a deep copy
        nargs = len(self.prototype)

        for pos, param in enumerate(reversed(self.prototype)):
            if type(param) == tuple:
                # Skip the name
                param, param_name = param
                param_name = f"'{param_name}'"
            else:
                param_name = f"argument at position {nargs - pos}"

            chain_item = chain.pop() # Pop the last element (and remove from length computation)
            offset = sum(item.size for item in chain if isinstance(item, Integral))

            param_size = param(0).size

            # Make sure the size if compatible
            if not chain_item.fits(param):
                raise ParsingException(f"Cannot fit {chain_item} into {param_name} of type {param.ctype} in {self.name}")

            # Compute offset for casts
            offset += chain_item.size - param_size

            if param_size == 1:
                values_str.insert(0, f"buffer[{offset}]")
            elif param_size == 2:
                values_str.insert(0, f"ntoh({offset})")
            elif param_size == 4:
                values_str.insert(0, f"ntohl({offset})")

        # Add the params (the list is ordered)
        return f"{self.name}({', '.join(values_str)});"

class NoOperation():
    def to_code(self):
        return "// Reply is ignored"

class State:
    """ State in the processing of incomming bytes """
    def __init__(self, name, pos=0, mode="slave"):
        self.name = name
        self.transition = []
        self.pos = pos
        self.mode = mode

    def add(self, transition):
        """ Append a new transition to the current state """
        self.transition.append(transition)

    def next(self, alias):
        """ @return the name of the next state """
        alias = alias if alias else str(len(self.transition)+1)
        return self.name + "_" + alias

    def __add__(self, suffix):
        return State(self.name + "_" + suffix, self.pos+1)

    def is_final(self):
        return len(self.transition) and isinstance(Operation, self.transition[0])

    def has(self, matcher):
        for transition in self.transition:
            if transition.matcher == matcher:
                return True
        return False

    def get_next_state_of(self, matcher):
        """ Given a matcher, return its transitioning state """
        for transition in self.transition:
            if transition.matcher == matcher:
                return transition.next
        assert(False)

    def to_code_case(self, indent):
        return f"{INDENT * indent}case state_t::{self.name}:\n"

    def to_code(self, indent):
        # Group the transitions into groups by type
        transition_groups = {}
        tab = INDENT * indent
        retval = str()

        if self.pos == 0 and self.mode == "master":
            retval += f"{tab}{INDENT}".join(["",
                "// The address must match the address just send and still in the buffer\n",
                "if ( c != buffer[0] ) {\n",
                "    error = error_t::ignore_frame;\n",
                "    state = state_t::IGNORE;\n",
                "    break;\n",
                "}\n"]
            )

        if self.pos == 1 and self.mode == "master":
            retval += f"{tab}{INDENT}".join(["",
                "// The command must match the command just sent\n",
                "if ( c == (0x80 | buffer[1]) ) { // Bit 7 indicate an error\n",
                "   state = state_t::BAD_REQUEST;\n",
                "   break;\n",
                "} else if ( c != buffer[1] ) {\n",
                "   state = state_t::ERROR;\n",
                "   break;\n",
                "}\n\n"]
            )

        for transition in self.transition:
            group = transition_groups.setdefault(
                transition.matcher.__class__,
                TransitionGroup(transition.matcher, self.pos)
            )

            group.transitions.append(transition)

        for tg in transition_groups.values():
            retval += tg.to_code(indent+1)

        if '{' in retval:
            return retval + f"\n{tab}{INDENT}}}\n{tab}{INDENT}break;\n"

        return retval + f"\n{tab}{INDENT}break;\n"


class OperationState(State):
    def __init__(self, op, name, pos=0):
        super().__init__(name, pos)
        self.op = op

    """ A state which leads to an operation """
    def to_code(self, indent):
        tab = INDENT * indent
        return f"{tab}{self.op.to_code()}\n{tab}break;\n"

class ParsingException(Exception):
    pass

class CodeGenerator:
    """ Creates the C++ code to parse the modbus data """
    def __init__(self, tree):
        self.counter = 0
        self.states = []
        self.callbacks = {}
        # Compute the maximum message size
        self.max_buf_size = 0
        # Buffer index
        self.buffer_index = 0
        # Specific callbacks
        self.on_ready_reply_callback = None
        # Device ID. None for dynamic
        self.device_address = None

        # The identification is optional and yields support for commands 17 and 43/14
        self.identification = tree.get("identification", {})

        # Device ID
        self.slave_id = tree.get("slave_id", 0xFF)

        if "callbacks" not in tree:
            raise ParsingException("Callbacks are required")

        self.process_callbacks(tree["callbacks"])

        for key, value in tree.items():
            if re.match(r"^device(@\d+)?$", key):
                for cmd in value:
                    self.max_buf_size = max(self.max_buf_size,
                        sum(item.size for item in cmd if isinstance(item, Integral))
                    )

        # Add space for the device address, the command and the CRC
        self.max_buf_size += 4

        # Overwrite with the configuration
        self.max_buf_size = max(self.max_buf_size, tree.get("buffer_size", 0))

        # Get the type of generation
        self.mode = tree.get("mode", "slave")

        if self.mode not in ["slave", "master"]:
            raise ParsingException("The mode must be 'master' or 'slave'")

        # Set the namespace
        self.namespace = tree.get("namespace", "slave")

        if "on_received" in tree:
            self.on_ready_reply_callback = tree["on_received"]

        self.conformity_level = 0

        if self.mode == "slave" and PRODUCT_CODE in self.identification:
            # Find a node in tree whose key starts with device@
            device_key = next((key for key in tree.keys() if key.startswith("device@")), None)
            if device_key:
               device = tree[device_key]
            else:
                if "device" not in tree:
                    raise ParsingException("identification requires a 'device' node")
                device = tree["device"]

            # Insert the report slave ID function
            device.append(
                (
                    u8(0x11, alias="REPORT_SLAVE_ID"),
                    "on_report_slave_id"
                )
            )

            # Add the callback for the report slave ID
            self.callbacks["on_report_slave_id"] = []

            for keys in self.identification.keys():
                if keys not in MEI_OBJECT_CATEGORY:
                    raise ParsingException(f"Invalid identification key {keys} in {self.mode} mode")
                self.conformity_level = max(self.conformity_level, MEI_OBJECT_CATEGORY[keys])

            # Insert the report slave ID function
            device.append(
                (
                    u8(0x2B, alias="ENCAPSULATED_INTERFACE_TRANSPORT"),
                    u8(0x0E, alias="READ_DEVICE_IDENTIFICATION"),
                    u8(0x01, 0x03, alias="READ_DEVICE_ID_CODE"),
                    u8(alias="OBJECT_ID"),
                    "on_read_device_identification"
                )
            )

            # Add the callback for the read device identification
            self.callbacks["on_read_device_identification"] = [
                (u8, "device_id"),
                (u8, "object_id"),
            ]

            # Insert the diagnostic function
            device.append(
                (
                    u8(0x08, alias="DIAGNOSTICS"),
                    u16(alias="SUBFUNCTION"),
                    u16(alias="DATA"),
                    "on_diagnostics"
                )
            )

            # Add the callback for the diagnostic
            self.callbacks["on_diagnostics"] = []

        self.process_devices(tree)

    def new_state(self, new_state_name, pos):
        """ Add a new state transition """
        # Make sure the name is unique
        names = {state.name for state in self.states if state.name.startswith(new_state_name)}

        count = 1
        alt_name = new_state_name
        while alt_name in names:
            alt_name = new_state_name + "_" + str(count)
            count+=1

        new_state = State(alt_name, pos, self.mode)
        self.states.append(new_state)
        return new_state

    def generate_code(self):
        placeholders = {
            "DEVICE_ADDRESS" : self.get_device_address(2),
            "set_device_address" : self.set_device_address(2),
            "BUFSIZE" : str(self.max_buf_size),
            "NAMESPACE" : self.namespace,
            "ENUMS" : self.get_enums_text(2),
            "CASES" : self.get_cases_text(3),
            "CALLBACKS" : self.get_callbacks_text(2),
            "INCOMPLETE": self.get_incomplete_text(2),
            "PROTOTYPES" : self.get_prototypes(1),
            "READY_REPLY_CALLBACK": self.get_ready_reply_callback(1),
            "SLAVE_ID_FUNCTION": self.get_report_slave_id_function(1),
            "SLAVE_READ_ID_REQUEST": self.get_read_device_identification(1),
        }

        # Function to replace each placeholder
        def replace_placeholder(match):
            linestart = match.group(1)
            placeholder = match.group(2)
            endl = match.group(3) or ""

            # Call the corresponding method based on the placeholder name
            return linestart + placeholders[placeholder].strip() + endl

        template = TEMPLATE_CODE_SLAVE if self.mode == "slave" else TEMPLATE_CODE_MASTER

        return re.sub(r"(\s*)@(.*?)@(\n?)", replace_placeholder, template )

    def get_device_address(self, indent):
        tab = INDENT * indent

        if self.device_address is None:
            return f"""///< Runtime ID. Set-up before starting the modbus\n{tab}inline static uint8_t device_address = 255;"""

        return f"""///< Device ID\n{tab}static constexpr auto device_address = uint8_t{{{self.device_address}}};"""

    def set_device_address(self, indent):
        if self.device_address is None:
            tab = INDENT * indent

            return "///< Set the device address\n" + \
                f"{tab}static inline void set_device_address(uint8_t new_address) {{\n" + \
                f"{tab}{INDENT}device_address = new_address;\n" + \
                f"{tab}}}"

        return ""

    def get_enums_text(self, indent):
        tab = INDENT * indent

        return ",\n".join(f"{tab}{state.name}" for state in self.states)

    def get_ready_reply_callback(self, indent):
        if self.on_ready_reply_callback:
            return f"{self.on_ready_reply_callback}(std::string_view{{(char *)buffer, cnt}});"
        return ""

    def get_cases_text(self, indent):
        state_code = str()

        for state in self.states:
            if isinstance(state, OperationState):
                continue
            state_code += state.to_code_case(indent)
            state_code += state.to_code(indent)

        # Create the default cases
        for state in self.states:
            if isinstance(state, OperationState):
                state_code += state.to_code_case(indent)

        return state_code

    def get_incomplete_text(self, indent):
        state_code = str()

        for state in self.states:
            if not isinstance(state, OperationState):
                state_code += state.to_code_case(indent+1)

        return state_code

    def get_callbacks_text(self, indent):
        state_code = str()

        for state in self.states:
            if isinstance(state, OperationState):
                state_code += state.to_code_case(indent+1)
                state_code += state.to_code(indent+2)

        return state_code

    def get_prototypes(self, indent):
        tab = INDENT * indent
        retval = str()

        for name, proto in self.callbacks.items():
            retval += f"{tab}void {name}("

            for cnt, param in enumerate(proto):
                if isinstance(param, tuple):
                    # Skip the name
                    param, param_name = param
                    param_name = " " + param_name
                else:
                    param_name = ""

                comma = ", " if len(proto) - cnt > 1 else ""

                retval += f"{param(0).ctype}{param_name}{comma}"

            retval += ");\n"

        if self.on_ready_reply_callback:
            retval += f"{tab}void {self.on_ready_reply_callback}(std::string_view);";

        return retval

    def process_callbacks(self, callback_list):
        """ Create a lookup for all the devices including param names """
        for cb, proto in callback_list.items():
            # Check the cb name is C
            if not VALID_C_FUNCTION_NAME.match(cb):
                raise ParsingException("Callback name is not a valid C function name")

            self.callbacks[cb] = proto

    def process_devices(self, tree):
        """ Start the process with grouping all the devices """
        current_state = self.new_state("DEVICE_ADDRESS", 0)

        for key, value in tree.items():
            if key == "device": # No ID attached - runtime ID
                device_state = self.new_state(f"DEVICE", 1)
                address_matcher = RuntimeDeviceAddress(alias=device_state.name)
                current_state.add(Transition(address_matcher, device_state))
                for command in value:
                    self.process_sequence(address_matcher, device_state, command)

            if key.startswith("device@"):
                match = re.search(DEVICE_ADDR_RE, key)

                if not match:
                    raise ParsingException("Malformed device address")

                self.device_address = int(match.group(1), 0)

                if self.device_address > 254:
                    raise ParsingException("device address must be <= 254")

                device_state = self.new_state(f"DEVICE_{self.device_address}", 1)
                address_matcher = u8(self.device_address, alias=device_state.name)
                current_state.add(Transition(address_matcher, device_state))

                for command in value:
                    self.process_sequence(address_matcher, device_state, command)

    def process_sequence(self, address_matcher, state, cmd):
        """ Given a single sequence, create the states and transitions """
        pos = 1 # First byte
        callback = cmd[-1]

        if type(callback) is not str:
            cmd += ("NOTHING",)
        elif callback not in self.callbacks:
            raise ParsingException(f"Unknown callback {callback}: Callback must be declared first")

        for index, matcher in enumerate(cmd):
            # Size of the matcher
            pos += matcher.size
            matcher.pos = pos  # Set the position at which the matcher matches

            if state.has(matcher):
                state = state.get_next_state_of(matcher)
            else:
                if isinstance(cmd[index+1], str): # Command to follow?
                    command_name = cmd[-1] # Grab the command name

                    # Add the CRC state
                    next_state = self.new_state(state.next("_" + command_name.upper() + "__CRC"), state.pos + matcher.size)
                    to_crc_transition = Transition(matcher, next_state)
                    to_crc_transition.set_crc = True
                    state.add(to_crc_transition)
                    state = next_state

                    # Add the final transition before making the call to the callback
                    if command_name != "NOTHING":
                        op = Operation(command_name, self.callbacks[command_name], [address_matcher] + list(cmd[:-1]))
                    else:
                        op = NoOperation()

                    next_state = OperationState(op, "RDY_TO_CALL__" + command_name.upper(), 0)
                    self.states.append(next_state)
                    crc_matcher = Crc(None)
                    state.add(Transition(crc_matcher, next_state))
                    break
                else:
                    next_state = self.new_state(state.next(matcher.alias), state.pos + matcher.size)
                    state.add(Transition(matcher, next_state))
                    state = next_state

    def get_report_slave_id_function(self, level):
        # We need the product code as a minimum
        if self.conformity_level == 0:
            return ""

        # Create the id using the MEI objects
        id = self.identification[PRODUCT_CODE]

        if MODEL_NAME in self.identification:
            id += f"_{self.identification[MODEL_NAME]}"

        # Placeholder for actual implementation
        tab = INDENT * level

        return f"{tab}".join(["",
            "\n/** Answer command 17 - Report slave id */\n",
            "inline void on_report_slave_id() {\n",
            "    Datagram::set_size(2); // Reset the count to 2 (ID + code)\n",
            f"    Datagram::pack<uint8_t>({len(id)+2}); // Byte count\n",
            f"    Datagram::pack<uint8_t>({self.slave_id}); // slave ID\n",
            "    Datagram::pack<uint8_t>(0xFF); // Status OK\n",
            f"    Datagram::pack(\"{id}\"); // Function code\n",
            "}"]
        )

    def get_read_device_identification(self, level):
        # We need the product code as a minimum
        if self.conformity_level == 0:
            return ""

        t0 = INDENT * (level)
        t1 = INDENT * (level+1)
        t2 = INDENT * (level+2)

        def pack(code):
            """ Helper function to pack the key into the code """
            data = self.identification.get(code, "")

            return t2.join(["",
                f"Datagram::pack<uint8_t>(0x{key:02x}); // Object code\n",
                f"Datagram::pack<uint8_t>({len(data)}); // Length of the object\n",
                f"Datagram::pack(\"{data}\");\n"]
            )

        # Placeholder for actual implementation
        retval = t0.join(["",
            "/** Answer command 43/14 */\n",
            " inline void on_read_device_identification(uint8_t device_id, uint8_t object_id) {\n",
            "    Datagram::set_size(4); // Reset the count to 4 (addr/func/mei_type/DevId)\n",
            f"    Datagram::pack<uint8_t>({self.conformity_level}); // Conformity level\n",
            "    Datagram::pack<uint8_t>(0); // No more to follow\n\n"
            "    Datagram::pack<uint8_t>(0); // Next object ID\n\n"
        ])

        all_objects = {}
        device_id_count = {}

        for key, identification in MEI_OBJECT_CATEGORY.items():
            if identification <= self.conformity_level and key in self.identification:
                all_objects.setdefault(identification, []).append(pack(key))
                device_id_count.setdefault(identification, 0)
                device_id_count[identification] += 1

        if self.conformity_level == BASIC_DEVICE_IDENTIFICATION:
            retval += t1.join(["",
                "Datagram::pack<uint8_t>(0x03); // 3 objects\n",
                *all_objects[BASIC_DEVICE_IDENTIFICATION]
            ])
        elif self.conformity_level == REGULAR_DEVICE_IDENTIFICATION:
            retval += t1.join(["",
                "if (device_id == 1) { // Device ID 1 has a fixed number of objects\n",
                "   Datagram::pack<uint8_t>(0x03); // 3 objects\n",
                "} else {\n",
                f"   Datagram::pack<uint8_t>({3+device_id_count[REGULAR_DEVICE_IDENTIFICATION]}); // {3+device_id_count[REGULAR_DEVICE_IDENTIFICATION]} objects\n",
                "}\n\n",
                "if (device_id == 1) {\n",
                *all_objects[BASIC_DEVICE_IDENTIFICATION],
                "} else {\n",
                *all_objects[REGULAR_DEVICE_IDENTIFICATION],
                "}\n",
            ])
        elif self.conformity_level == EXTENDED_DEVICE_IDENTIFICATION:
            l1c = device_id_count[BASIC_DEVICE_IDENTIFICATION]
            l2c = device_id_count[REGULAR_DEVICE_IDENTIFICATION]
            l3c = device_id_count[EXTENDED_DEVICE_IDENTIFICATION]

            retval += t1.join(["",
                f"if (device_id == 1) {{ // Device ID 1 has a fixed number of objects\n",
                f"   Datagram::pack<uint8_t>({l1c}); // {l1c} objects\n",
                "} else if (device_id == 2) {\n",
                f"   Datagram::pack<uint8_t>({l1c+l2c}); // {l1c} + {l2c} objects\n",
                "} else {\n",
                f"   Datagram::pack<uint8_t>({l1c+l2c+l3c}); // {l1c} +  {l2c} + {l3c} objects\n",
                "}\n\n",
                "if (device_id >= 1) {\n"
            ])

            retval += "".join(["", *all_objects[BASIC_DEVICE_IDENTIFICATION]])
            retval += t1 + "}\n\n"
            retval += t1 + "if (device_id >= 2) {\n"
            retval += "".join(["", *all_objects[REGULAR_DEVICE_IDENTIFICATION]])
            retval += t1 + "}\n\n"
            retval += t1 + "if (device_id == 3) {\n"
            retval += "".join(["", *all_objects[EXTENDED_DEVICE_IDENTIFICATION]])
            retval += t1 + "}\n"

        retval += t0.join(["","}\n"])

        return retval

if __name__ == "__main__":
    print("Call the using file")

class Modbus:
    def __init__(self, modbus):
        self.modbus = modbus
        self.main()

    def main(self):
        import argparse
        parser = argparse.ArgumentParser(description="Generate code for Modbus.")
        parser.add_argument('-o', '--output', type=str, help='Output file name')
        parser.add_argument("-t", "--tab-size", type=int, default=4, choices=range(0, 9),
            help="Set the tab size (0-8). Defaults to 4.)"
        )
        args = parser.parse_args()

        # Override the tab size
        global INDENT
        INDENT = args.tab_size * ' '

        try:
            gen = CodeGenerator(self.modbus)
            generated_code = gen.generate_code()
        except ParsingException as e:
            print( "Error: " + str(e) )
        else:
            if args.output:
                with open(args.output, 'w') as f:
                    f.write(generated_code)
            else:
                print(generated_code)

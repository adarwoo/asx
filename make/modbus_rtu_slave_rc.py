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
   The parameters are converted to the host reprentation and cast.
   The size is checked during cast. A range 0-0x200 cannot be cast to an 8-bit.
"""
import re

TEMPLATE_CODE_MASTER="""/**
 * This file was generated to create a state machine for processing
 * uart data used for a modbus RTU.
 */
#include <logger.h>
#include <stdint.h>
#include <asx/modbus_rtu.hpp>

namespace @NAMESPACE@ {
    // All callbacks registered
    @PROTOTYPES@

    // All states to consider
    enum class state_t : uint8_t {
        IGNORE = 0,
        ERROR = 1,
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
            LOG_TRACE("DGRAM", "Char: 0x%.2x, index: %d, state: %d", c, cnt, (uint8_t)state);

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

        /** Called when a T3.5 has been detected, in a good sequence */
        static void process_reply() noexcept {
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

        static void initiate_transmit(uint8_t slave_addr, asx::modbus::command_t cmd) noexcept {
            cnt = 0;
            expected_address = buffer[cnt++] = slave_addr;
            expected_command = buffer[cnt++] = reinterpret_cast<uint8_t>(cmd);
        }
    }; // struct Processor
} // namespace modbus"""

TEMPLATE_CODE_SLAVE="""/**
 * This file was generated to create a state machine for processing
 * uart data used for a modbus RTU. It should be included by
 * the modbus_rtu_slave.cpp file only which will create a full rtu slave device.
 */
#include <logger.h>
#include <stdint.h>
#include <asx/modbus_rtu.hpp>

namespace @NAMESPACE@ {
    // All callbacks registered
    @PROTOTYPES@

    // All states to consider
    enum class state_t : uint8_t {
        IGNORE = 0,
        ERROR = 1,
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
            LOG_TRACE("DGRAM", "Char: 0x%.2x, index: %d, state: %d", c, cnt, (uint8_t)state);

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
} // namespace modbus"""

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

READ_COILS                    = u8(0x01, alias="READ_COILS")
READ_DISCRETE_INPUTS          = u8(0x02, alias="READ_DISCRETE_INPUTS")
READ_HOLDING_REGISTERS        = u8(0x03, alias="READ_HOLDING_REGISTERS")
READ_INPUT_REGISTERS          = u8(0x04, alias="READ_INPUT_REGISTERS")
WRITE_SINGLE_COIL             = u8(0x05, alias="WRITE_SINGLE_COIL")
WRITE_SINGLE_REGISTER         = u8(0x06, alias="WRITE_SINGLE_REGISTER")
WRITE_MULTIPLE_COILS          = u8(0x0F, alias="WRITE_MULTIPLE_COILS")
WRITE_MULTIPLE_REGISTERS      = u8(0x10, alias="WRITE_MULTIPLE_REGISTERS")
READ_WRITE_MULTIPLE_REGISTERS = u8(0x17, alias="READ_WRITE_MULTIPLE_REGISTERS")
CUSTOM                        = u8(0x65, alias="CUSTOM")


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

class State:
    """ State in the processing of incomming bytes """
    def __init__(self, name, pos=0):
        self.name = name
        self.transition = []
        self.pos = pos

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

        if "callbacks" not in tree:
            raise ParsingException("Callbacks are required")

        self.process_callbacks(tree["callbacks"])

        for key, value in tree.items():
            if key.startswith("device"):
                for cmd in value:
                    self.max_buf_size = max(self.max_buf_size,
                        sum(item.size for item in cmd if isinstance(item, Integral))
                    )

        # Add space for the device address, the command and the CRC
        self.max_buf_size += 4
        # Overwrite with the configuration
        self.max_buf_size = max(self.max_buf_size, tree.get("buffer_size", 0))

        # Set the namespace
        self.namespace = tree.get("namespace", "slave")

        if "on_received" in tree:
            self.on_ready_reply_callback = tree["on_received"]

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

        new_state = State(alt_name, pos)
        self.states.append(new_state)
        return new_state

    def generate_code(self):
        placeholders = {
            "BUFSIZE" : str(self.max_buf_size),
            "NAMESPACE" : self.namespace,
            "ENUMS" : self.get_enums_text(2),
            "CASES" : self.get_cases_text(3),
            "CALLBACKS" : self.get_callbacks_text(2),
            "INCOMPLETE": self.get_incomplete_text(2),
            "PROTOTYPES" : self.get_prototypes(1),
            "READY_REPLY_CALLBACK": self.get_ready_reply_callback(1),
        }

        # Function to replace each placeholder
        def replace_placeholder(match):
            linestart = match.group(1)
            placeholder = match.group(2)
            endl = match.group(3) or ""

            # Call the corresponding method based on the placeholder name
            return linestart + placeholders[placeholder].strip() + endl

        return re.sub(r"(\s*)@(.*?)@(\n?)", replace_placeholder, TEMPLATE_CODE)

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
            if key.startswith("device@"):
                match = re.search(DEVICE_ADDR_RE, key)
                if not match:
                    raise ParsingException("Malformed device address")
                device_number = int(match.group(1), 0)
                if device_number > 255:
                    raise ParsingException("device address must be < 256")

                device_state = self.new_state(f"DEVICE_{device_number}", 1)
                address_matcher = u8(device_number, alias=device_state.name)
                current_state.add(Transition(address_matcher, device_state))

                for command in value:
                    self.process_sequence(address_matcher, device_state, command)

    def process_sequence(self, address_matcher, state, cmd):
        """ Given a single sequence, create the states and transitions """
        pos = 1 # First byte
        callback = cmd[-1]
        if callback not in self.callbacks:
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

                    if command_name not in self.callbacks:
                        raise ParsingException(f"Cmd {command_name} does not have a prototype")

                    # Add the CRC state
                    next_state = self.new_state(state.next("_" + command_name.upper() + "__CRC"), state.pos + matcher.size)
                    to_crc_transition = Transition(matcher, next_state)
                    to_crc_transition.set_crc = True
                    state.add(to_crc_transition)
                    state = next_state

                    # Add the final transition before making the call to the callback
                    op = Operation(command_name, self.callbacks[command_name], [address_matcher] + list(cmd[:-1]))
                    next_state = OperationState(op, "RDY_TO_CALL__" + command_name.upper(), 0)
                    self.states.append(next_state)
                    crc_matcher = Crc(None)
                    state.add(Transition(crc_matcher, next_state))
                    break
                else:
                    next_state = self.new_state(state.next(matcher.alias), state.pos + matcher.size)
                    state.add(Transition(matcher, next_state))
                    state = next_state

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

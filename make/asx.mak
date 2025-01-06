# Rules for the ASX library
# Available modules are:
# reactor    - C/C++ reactor engine
# timer      - C/C++ timer support. 'reactor' added.
# mem        - Support for dynamic allocation (malloc and new) + stack fill
# uart       - C++ UART support
# modbus_rtu - C++ modbus support
# hw_timer   - C++ hardware timer support
# i2c_slave  - C/C++ i2c slave support
# i2c_master - C/C++ i2c master support

# Directories relative to TOP
STDCXX_DIR:=libstdc++

VPATH+=$(ASX_DIR)

ASX_PATH:=$(if $(strip $(ASX_DIR)), $(addsuffix /,$(patsubst %/,%,$(filter-out $(TOP), $(ASX_DIR)))))

# Manage dependencies
ifneq ($(filter timer,$(ASX_USE)),)
ASX_USE+=reactor
endif

ifneq ($(filter modbus_rtu,$(ASX_USE)),)
ASX_USE+=hw_timer reactor timer uart logger
endif

ifneq ($(filter i2c_master,$(ASX_USE)),)
ASX_USE+=reactor timer logger
endif

# Append ASX code files
SRCS+=\
   $(ASX_PATH)src/builtin.cpp \
   $(ASX_PATH)src/_ccp.s \
   $(ASX_PATH)src/sysclk.c \
   $(ASX_PATH)src/alert.c \
   $(if $(filter timer,$(ASX_USE)), $(ASX_PATH)src/timer.c, ) \
   $(if $(filter reactor,$(ASX_USE)), $(ASX_PATH)src/reactor.c, ) \
   $(if $(filter mem,$(ASX_USE)), $(ASX_PATH)src/mem.c, ) \
   $(if $(filter uart,$(ASX_USE)), $(ASX_PATH)src/uart.cpp, ) \
   $(if $(filter modbus_rtu,$(ASX_USE)), $(ASX_PATH)src/modbus_rtu.cpp, ) \
   $(if $(filter hw_timer,$(ASX_USE)), $(ASX_PATH)src/hw_timer.cpp, ) \
   $(if $(filter eeprom,$(ASX_USE)), $(ASX_PATH)src/eeprom.cpp, ) \
   $(if $(filter i2c_master,$(ASX_USE)), $(ASX_PATH)src/i2c_master.cpp, ) \
   $(if $(filter pca9555,$(ASX_USE)), $(ASX_PATH)src/pca9555.cpp, ) \
   $(if $(filter piezzo,$(ASX_USE)), $(ASX_PATH)src/piezzo.c, ) \

ifneq ($(filter modbus_rtu,$(ASX_USE)),)
ASX_USE+=hw_timer reactor timer uart logger
endif

ifneq ($(filter logger,$(ASX_USE)),)
	ifdef SIM
		SRCS+=\
			${LOGGER_DIR}/src/logger_common.c \
			${LOGGER_DIR}/src/logger_cxx.cpp \
			${LOGGER_DIR}/src/logger_init_unix.cpp \
			${LOGGER_DIR}/src/logger_os_linux.cpp \
			${LOGGER_DIR}/src/logger_os_linux_thread.cpp \
			${LOGGER_DIR}/src/logger_trace_stack_linux.cpp
	endif
	INCLUDE_DIRS+=$(ASX_DIR)/logger/include
endif

INCLUDE_DIRS+=\
	$(ASX_DIR)/include \
	$(ASX_DIR)/import/boost/include \
	$(ASX_DIR)/import/libstdc++/include \

ifndef SIM
INCLUDE_DIRS+=$(TOP)/$(STDCXX_DIR)/include
endif

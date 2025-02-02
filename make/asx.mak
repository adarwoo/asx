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

# Will contain the list of modules after the expansion
ASX_MODULES:=

# Recursive function to resolve dependencies and source files
define resolve_deps
$(strip \
  $(if $1, \
    $(foreach dep,$1, \
      $(if $(filter $(dep),$(ASX_MODULES)), \
        , \
        $(eval ASX_MODULES += $(dep)) \
        $(if $(DEPOF_$(dep)), \
          $(foreach item,$(DEPOF_$(dep)), \
            $(if $(filter %.c %.cpp,$(item)), \
              $(ASX_PATH)src/$(item), \
              $(call resolve_deps,$(item)) \
            ) \
          ), \
          $(error Module $(dep) is not defined in DEPOF_$(dep)) \
        ) \
      ) \
    ) \
  ) \
)
endef

# Define dependencies and source files for each module
DEPOF_timer       := reactor timer.c
DEPOF_modbus_rtu  := hw_timer reactor timer uart logger modbus_rtu.cpp
DEPOF_pca9555     := i2c_master pca9555.cpp
DEPOF_i2c_master  := reactor timer logger i2c_master.cpp
DEPOF_reactor     := reactor.c
DEPOF_hw_timer    := hw_timer.cpp
DEPOF_uart        := uart.cpp
DEPOF_logger      := logger
DEPOF_piezzo      := piezzo.c
DEPOF_eeprom      := eeprom.cpp

ASX_FILES 			:= $(sort $(call resolve_deps,$(ASX_USE)))

# Append ASX code files
SRCS+=\
   $(ASX_PATH)src/builtin.cpp \
   $(ASX_PATH)src/_ccp.s \
   $(ASX_PATH)src/sysclk.c \
   $(ASX_PATH)src/alert.c \
	$(ASX_PATH)src/mem.c \
	$(ASX_FILES)

ifneq ($(filter logger,$(ASX_MODULES)),)
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

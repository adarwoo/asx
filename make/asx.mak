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

VPATH+=$(ASX_DIR) $(ULOG_DIR)

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
              src/$(item), \
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
DEPOF_modbus_rtu  := hw_timer reactor timer uart modbus_rtu.cpp ulog
DEPOF_pca9555     := i2c_master pca9555.cpp
DEPOF_i2c_master  := reactor timer i2c_master.cpp
DEPOF_reactor     := reactor.c
DEPOF_hw_timer    := hw_timer.cpp
DEPOF_uart        := uart.cpp
DEPOF_piezzo      := piezzo.c
DEPOF_eeprom      := eeprom.cpp
DEPOF_ulog        := ulog.c ulog_asx.cpp reactor uart

ASX_FILES 			:= $(sort $(call resolve_deps,$(ASX_USE)))

# Append ASX code files
SRCS+=\
  $(ASX_PATH)src/sysclk.c \
  $(ASX_PATH)src/builtin.cpp \
  $(ASX_PATH)src/alert.cpp \
  $(ASX_FILES)

ifndef SIM
SRCS+=\
  $(ASX_PATH)src/_ccp.s \
  $(ASX_PATH)src/mem.c \
  $(ASX_PATH)src/watchdog.c \

endif

INCLUDE_DIRS+=\
  $(ASX_DIR)/include \
  $(ASX_DIR)/import/boost/include \
  $(ASX_DIR)/import/ulog/include \

ifndef SIM
INCLUDE_DIRS+=\
  $(ASX_DIR)/import/libstdc++/include
endif

# End of asx/make/asx.mak
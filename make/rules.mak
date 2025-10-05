.PHONY: clean all shell

# Check if running inside Docker
SWITCH_TO_CONTAINER:=$(if $(wildcard /.dockerenv_asx),no,yes)

# For AVR Studio, build
ifdef ToolchainDir
SWITCH_TO_CONTAINER:=no
endif

ifeq ($(SWITCH_TO_CONTAINER),yes)
# Find the directory of this file (b.mak)
CURRENT_MAKEFILE := $(lastword $(MAKEFILE_LIST))
ASX_DIR := $(dir $(abspath $(CURRENT_MAKEFILE)/..))

# Define a space and non-breakable space
space := $(empty) $(empty)

# Not in Docker, respawn inside Docker
CONTAINER_RUN_CMD := $(ASX_DIR)buildenv make
CMD_VARS=$(strip \
	$(foreach var,$(.VARIABLES), $(if $(filter command line, $(origin $(var))),$(var)=$($(var)))))

all $(filter-out all shell prog, $(MAKECMDGOALS)):
	@$(CONTAINER_RUN_CMD) $@ $(CMD_VARS)

shell:
	@$(ASX_DIR)/buildenv

build_type ?= $(if $(NDEBUG),Release,Debug)
BUILD_DIR       ?= $(build_type)
AVRDUDE := /mnt/e/opt/avr-dude/avrdude.exe
PROGRAMMER := atmelice_updi
PORT := usb

prog : $(BUILD_DIR)/$(BIN).hex
	@echo Programming $(BIN). Reset the device after programming.
	$(MUTE)$(AVRDUDE) -p $(ARCH) -c $(PROGRAMMER) -P $(PORT) -U flash:w:$(BUILD_DIR)/$(BIN).hex:i
else

# By default, build for the AVR target. Export sim to build a simulator
ifdef ToolchainDir
target := studio
else
target := $(if $(SIM),sim,avr)
endif

# Reference all from the solution
VPATH=$(TOP)

ASX_DIR ?= $(TOP)/asx

include $(ASX_DIR)/make/$(target).mak
include $(ASX_DIR)/make/asx.mak

build_type ?= $(if $(NDEBUG),Release,Debug)
MUTE  ?= $(if $(VERBOSE),@set -x;,@)

# Default tools
CC    ?= gcc
CXX   ?= g++
RC    ?= make/rc.py
SIZE  ?= size
ECHO  ?= echo
MKDIR ?= mkdir -p
SREC_CAT ?= srec_cat

BUILD_DIR       ?= $(build_type)

# Work out the size of the flash using make functions only
FLASH_END := \
	$(if $(findstring attiny32,$(ARCH)),0x7FFE, \
		$(if $(findstring attiny16,$(ARCH)),0x3FFE, \
			$(if $(findstring attiny8,$(ARCH)),0x1FFE, \
				$(if $(findstring attiny4,$(ARCH)),0x0FFE, \
					$(if $(findstring attiny2,$(ARCH)),0x07FE, \
						$(error Unknown arch $(ARCH)))))))

# Pre-processor flags for C, C++ and assembly
CPPFLAGS        += $(foreach p, $(INCLUDE_DIRS), -I$(p)) -D$(if $(NDEBUG),NDEBUG,DEBUG)=1 -DCRC_AT=$(strip $(FLASH_END))

# Flags for the compilation of C files
CFLAGS          += -Wall -gdwarf-2

# Flags for the compilation of C++ files
CXXFLAGS        += $(CFLAGS) -std=c++20 -fno-exceptions -fno-rtti -fext-numeric-literals

# Assembler flags
ASFLAGS         += -Wa,-gdwarf-2 -x assembler-with-cpp -Wa,-g

# Flag for the linker
LDFLAGS         += -gdwarf-2

# Dependencies creation flags
DEPFLAGS         = -MT $@ -MMD -MP -MF $(BUILD_DIR)/$*.d
POSTCOMPILE      = mv -f $(BUILD_DIR)/$*.Td $(BUILD_DIR)/$*.d && touch $@

DEP_FILES        = $(OBJS:%.o=%.d)

RCDEP_FILES      = $(foreach rc, $(SRCS.resources:%.json=%.rcd), $(BUILD_DIR)/$(rc))

COMPILE.c        = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) $(ARCHFLAGS) -c
COMPILE.cxx      = $(CXX) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(ARCHFLAGS) -c
COMPILE.rc       = $(RC) -E
COMPILE.as       = $(CC) $(ASFLAGS) $(CPPFLAGS) $(ARCHFLAGS) -c
LINK.cxx         = $(CXX) $(ARCHFLAGS)
LINK.c           = $(CC) $(ARCHFLAGS)

OBJS             = $(foreach file, $(SRCS), $(BUILD_DIR)/$(basename $(file)).o)

LIBS            += m

LD               = $(if $(findstring .cpp,$(suffix $(SRCS))),$(LINK.cxx),$(LINK.c))

# Allow the source to be in sub-directories
BUILDDIRS        = $(sort $(dir $(OBJS)))

# Does the project have a trace config?
CPPFLAGS += $(if $(wildcard $(TOP)/conf/conf_ulog.h), -DHAS_ULOG_CONFIG_FILE,)

all : $(BUILDDIRS) $(BIN)$(BIN_EXT)

-include $(RCDEP_FILES)

# Create the build directory
$(BUILD_DIR): ; $(MUTE)$(MKDIR) $@

$(BIN)$(BIN_EXT) : $(BUILD_DIR)/$(BIN)$(BIN_EXT)
	@echo Copying $^ to $@
	@cp $^ $@

$(BUILD_DIR)/$(BIN)$(BIN_EXT) : $(OBJS)
	@echo Linking to $@
	$(PRE_LINK)
	$(MUTE)$(LD) -Wl,--start-group $^ $(foreach lib,$(LIBS),-l$(lib)) -Wl,--end-group ${LDFLAGS} -o $@
	$(POST_LINK)
	$(DIAG)

$(BUILD_DIR)/%.o : %.c
	@echo "Compiling  C " $<
	$(MUTE)$(COMPILE.c) $< -o $@

${BUILD_DIR}/%.o : %.cpp
	@echo Compiling C++ $<
	$(MUTE)$(COMPILE.cxx) $< -o $@

$(BUILD_DIR)/%.o : %.s
	@echo "Assembling   " $<
	$(MUTE)$(COMPILE.as) $< -o $@

$(BUILD_DIR)/%.rcd : %.json
	@echo Generating the resources from $<
	$(MUTE)[ -d $(@D) ] || mkdir -p $(@D)
	$(MUTE)$(COMPILE.rc) $@ $<

%.hpp : %.conf.py $(ASX_DIR)/make/modbus_rtu_rc.py
	@echo Generating $@ interface header code from $<
	$(MUTE)[ -d $(@D) ] || mkdir -p $(@D)
	$(MUTE)PYTHONPATH=$(ASX_DIR)/make python3 $< -o$@

#-----------------------------------------------------------------------------
# Create directory $$dir if it doesn't already exist.
#
define CreateDir
  if [ ! -d $$dir ]; then \
    (umask 002; $(MKDIR) $$dir); \
  fi
endef

#-----------------------------------------------------------------------------
# Build directory creation
#
$(BUILDDIRS) :
	$(MUTE)$(MKDIR) "$@"

# Include the .d if they exists
-include $(DEP_FILES)

#-----------------------------------------------------------------------------
# Clean rules
#
clean: $(BUILD_DIR)
	@echo "Removing build directory: $(BUILD_DIR)"
	@rm -rf $(BUILD_DIR) $(CLEAN_FILES)

help:
	@echo "Rules are: all, clean"
	@echo "Trace: Set the variable trace={error,warn,mile,info,debug} and dom={<dom0>,<dom1>...}"

endif # SWITCH_TO_CONTAINER

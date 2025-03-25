.PHONY: clean all shell

# Check if running inside Docker
SWITCH_TO_DOCKER:=$(shell test -f /.dockerenv && echo no || echo yes)

# For AVR Studio, build
ifdef ToolchainDir
SWITCH_TO_DOCKER:=no
endif

ifeq ($(SWITCH_TO_DOCKER),yes)
# Find the directory of this file (b.mak)
CURRENT_MAKEFILE := $(lastword $(MAKEFILE_LIST))
ASX_DIR := $(dir $(abspath $(CURRENT_MAKEFILE)/..))

# Define a space and non-breakable space
space := $(empty) $(empty)

# Not in Docker, respawn inside Docker
DOCKER_RUN_CMD := $(ASX_DIR)buildenv make
CMD_VARS=$(strip \
	$(foreach var,$(.VARIABLES), $(if $(filter command line, $(origin $(var))),$(var)=$($(var)))))

all $(filter-out all shell, $(MAKECMDGOALS)):
	@$(DOCKER_RUN_CMD) $@ $(CMD_VARS)

shell:
	@$(ASX_DIR)/buildenv
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
MKDIR ?=	mkdir -p
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

# Manage the tracing flags
# Argument to pass are trace=INFO dom="uart patch"
# Default values for trace level and domains
TRACE_LEVEL ?= WARN
dom ?= ""

# Map trace level to numeric values
ifeq ($(TRACE_LEVEL),ERROR)
    TRACE_LEVEL_NUM := 0
else ifeq ($(TRACE_LEVEL),WARN)
    TRACE_LEVEL_NUM := 1
else ifeq ($(TRACE_LEVEL),MILE)
    TRACE_LEVEL_NUM := 2
else ifeq ($(TRACE_LEVEL),TRACE)
    TRACE_LEVEL_NUM := 3
else ifeq ($(TRACE_LEVEL),INFO)
    TRACE_LEVEL_NUM := 4
else ifeq ($(TRACE_LEVEL),DEBUG)
    TRACE_LEVEL_NUM := 5
else
    $(error Invalid trace level: $(TRACE_LEVEL))
endif

# Convert domains to uppercase and add -D flags
DOMAINS_UPPER := $(shell echo $(dom) | tr a-z A-Z | tr ',' ' ')
DOMAIN_FLAGS := $(foreach domain, $(DOMAINS_UPPER), -DDOMAIN_$(domain)_ENABLED=1)

# Add trace level and domain flags to CPPFLAGS
CPPFLAGS += -DTRACE_LEVEL=$(TRACE_LEVEL_NUM) $(DOMAIN_FLAGS)

all : $(BUILDDIRS) $(BUILD_DIR)/$(BIN)$(BIN_EXT)

-include $(RCDEP_FILES)

# Create the build directory
$(BUILD_DIR): ; $(MUTE)-mkdir -p $@

$(BIN)$(BIN_EXT) : $(BUILD_DIR)/$(BIN)$(BIN_EXT)
	@echo Copying $^ to $@
	@cp $^ $@

$(BUILD_DIR)/$(BIN)$(BIN_EXT) : $(OBJS)
	@echo Linking to $@
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
    (umask 002; mkdir -p $$dir); \
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

endif # SWITCH_TO_DOCKER

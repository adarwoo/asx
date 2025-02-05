##
## Make rules for a Windows/AVR Studio build
##

# Change the compiler
CXX := $(ToolchainDir)/avr-g++
CC := $(ToolchainDir)/avr-gcc
SIZE := $(ToolchainDir)/avr-size
SREC_CAT := 'C:\Program Files\srecord\bin\srec_cat'
MKDIR := mkdir
MUTE  := $(if $(VERBOSE),,@)

BIN_EXT :=.elf

ARCHFLAGS :=-mmcu=$(ARCH) -B "$(DEVICE_STARTUP_ROOT)\gcc\dev\$(ARCH)"
CPPFLAGS += -isystem "$(DEVICE_STARTUP_ROOT)/include"
CFLAGS += -funsigned-char -funsigned-bitfields -ffunction-sections -fdata-sections -fshort-enums
CFLAGS += -O$(if $(NDEBUG),s,g)
CXXFLAGS += -fno-threadsafe-statics

LDFLAGS += -Wl,-Map="$(BIN).map" -Wl,--start-group -Wl,-lm  -Wl,--end-group -Wl,--gc-sections -Wl,--demangle -Wl,-flto

OBJS = $(foreach file, $(SRCS), $(BUILD_DIR)/$(basename $(file)).o)

define DIAG
endef

define POST_LINK
	$(MUTE)avr-objcopy -O ihex -R .eeprom -R .fuse -R .lock -R .signature -R .user_signatures  "$@" "${@:.elf=.hex}"
	$(MUTE)$(SREC_CAT) $(@:.elf=.hex) -intel -crop 0 $(FLASH_END) -fill 0xFF 0 $(FLASH_END) -CRC16_Big_Endian $(FLASH_END) -broken -o $(@:.elf=_crc.hex) -intel -line-length=44
	$(MUTE)$(SIZE) $@
	$(MUTE)cp $(BUILD_DIR)/$(BIN)$(BIN_EXT) $(BIN)$(BIN_EXT)
endef

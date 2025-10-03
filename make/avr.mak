##
## Make rules for a Linux/Docker/WSL build
##

# Change the compiler
CXX := avr-g++
CC := avr-gcc
SIZE := avr-size
AVRDUDE := /mnt/e/opt/avr-dude/avrdude
OBJCOPY := avr-objcopy

# AVR-Dude programmer settings
PROGRAMMER := atmelice_updi
PORT := usb

# Pack for the toolchain
PACK_VERSION = 2.0.368
PACK_PATH = /opt/ATtiny_DFP.${PACK_VERSION}
SPEC_PATH = ${PACK_PATH}/gcc/dev/$(ARCH)

# Target type for this build (cross-compilation)
BIN_EXT :=.elf

# Remove all logs
ARCHFLAGS :=-mmcu=$(ARCH) -B $(SPEC_PATH) -isystem $(PACK_PATH)/include
CFLAGS += -funsigned-char -funsigned-bitfields -ffunction-sections -fdata-sections -fshort-enums $(ARCHFLAGS)
CFLAGS += -O$(if $(NDEBUG),s,g)
ASFLAGS += $(CPPFLAGS) $(ARCHFLAGS)
CXXFLAGS += -fno-threadsafe-statics -Wno-subobject-linkage
LDFLAGS += $(ARCHFLAGS) -Wl,--demangle -Wl,-flto -Wl,-Map="$(BUILD_DIR)/$(BIN).map" -Wl,--start-group -Wl,-lm  -Wl,--end-group -Wl,--gc-sections -mmcu=$(ARCH)
LDFLAGS += -Wl,--script=$(BUILD_DIR)/avrx3_with_ulog_sections.ld

define DIAG
$(MUTE)$(SIZE) $@
endef

define PRE_LINK
	@echo "Creating custom linker script"
	$(MUTE)awk ' \
		/MEMORY/ { in_memory = 1 } \
		in_memory && /^\s*}/ && !mem_inserted { \
			print "  logmeta (r) : ORIGIN = 0x10000, LENGTH = 0x100000"; \
			mem_inserted = 1; \
		} \
		/SECTIONS/ { in_sections = 1 } \
		in_sections && /^\s*{/ && !inserted { \
			print; \
			print "  .logs (NOLOAD) :"; \
			print "  {"; \
			print "    KEEP(*(.logs))"; \
			print "  } > logmeta"; \
			print ""; \
			print "  .logstr (NOLOAD) :"; \
			print "  {"; \
			print "    KEEP(*(.logstr))"; \
			print "  } > logmeta"; \
			print ""; \
			inserted = 1; \
			next; \
		} \
		{ print } \
	' /opt/avr-gcc-14.1.0-x64-linux/avr/lib/ldscripts/avrxmega3.xn > $(BUILD_DIR)/avrx3_with_ulog_sections.ld
endef

define POST_LINK
	@echo "Creating memory maps"
	$(MUTE)avr-objcopy -O ihex -R .eeprom -R .fuse -R .lock -R .signature -R .user_signatures  "$@" "${@:.elf=.hex}"
	$(MUTE)avr-objcopy -j .eeprom  --set-section-flags=.eeprom=alloc,load --change-section-lma .eeprom=0  --no-change-warnings -O ihex "$@" "${@:.elf=.eep}" || exit 0
	$(MUTE)avr-objcopy -O srec -R .eeprom -R .fuse -R .lock -R .signature -R .user_signatures "$@" "${@:.elf=.lss}"
	@echo "Creating $(@:.elf=_crc.hex) which includes the flash CRC"
	$(MUTE)$(SREC_CAT) $(@:.elf=.hex) -intel -crop 0 $(FLASH_END) -fill 0xFF 0 $(FLASH_END) -CRC16_Big_Endian $(FLASH_END) -broken -o $(@:.elf=_crc.hex) -intel -line-length=44
endef

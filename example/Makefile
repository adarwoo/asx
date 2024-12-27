# Reference all from the top of the project. Assumes the all dependencies are in TOP
TOP:=..

# Select the tiny architecture
ARCH:=attiny3224

# Name of the binary to produce
BIN:=blink

# Extra path for the include. Make sure that conf_xxx files can be found
INCLUDE_DIRS:=.

# Let ASX know what the project requires. See asx.mak for the list.
ASX_USE:=timer eeprom

# Project own files. All core ASX files are added automatically
SRCS:=blink.cpp

# Inlude the actual build rules
ASX_DIR:=$(TOP)
include $(TOP)/make/rules.mak
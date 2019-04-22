# If we haven't got IDF_PATH set already, reinvoke with IDF_PATH in the environ
.NOTPARALLEL:

ifeq ($(IDF_PATH),)
THIS_MK_FILE:=$(notdir $(lastword $(MAKEFILE_LIST)))
THIS_DIR:=$(abspath $(dir $(lastword $(MAKEFILE_LIST))))
IDF_PATH=$(THIS_DIR)/sdk/esp32-esp-idf

TOOLCHAIN_VERSION:=20181106.0
PLATFORM:=linux-x86_64

ESP32_BIN:=$(THIS_DIR)/tools/toolchains/esp32-$(PLATFORM)-$(TOOLCHAIN_VERSION)/bin
ESP32_GCC:=$(ESP32_BIN)/xtensa-esp32-elf-gcc
ESP32_TOOLCHAIN_DL:=$(THIS_DIR)/cache/toolchain-esp32-$(PLATFORM)-$(TOOLCHAIN_VERSION).tar.xz

all: | $(ESP32_GCC)
%: | $(ESP32_GCC)
	@echo Setting IDF_PATH and re-invoking...
	@env IDF_PATH=$(IDF_PATH) PATH=$(PATH):$(ESP32_BIN) $(MAKE) -f $(THIS_MK_FILE) $@
	@if test "$@" = "clean"; then rm -rf $(THIS_DIR)/tools/toolchains/esp32-*; fi

$(ESP32_GCC): $(ESP32_TOOLCHAIN_DL)
	@echo Uncompressing toolchain
	@mkdir -p $(THIS_DIR)/tools/toolchains/
	@tar -xJf $< -C $(THIS_DIR)/tools/toolchains/
	# the archive contains ro files
	@chmod -R u+w $(THIS_DIR)/tools/toolchains/esp32-*
	@touch $@

$(ESP32_TOOLCHAIN_DL):
	@mkdir -p $(THIS_DIR)/cache
	wget --tries=10 --timeout=15 --waitretry=30 --read-timeout=20 --retry-connrefused https://github.com/jmattsson/esp-toolchains/releases/download/$(PLATFORM)-$(TOOLCHAIN_VERSION)/toolchain-esp32-$(PLATFORM)-$(TOOLCHAIN_VERSION).tar.xz -O $@ || { rm -f "$@"; exit 1; }

else

PROJECT_NAME:=NodeMCU

# This is, sadly, the cleanest way to resolve the different non-standard
# conventions for sized integers across the various components.
BASIC_TYPES=-Du32_t=uint32_t -Du16_t=uint16_t -Du8_t=uint8_t -Ds32_t=int32_t -Ds16_t=int16_t -Duint32=uint32_t -Duint16=uint16_t -Duint8=uint8_t -Dsint32=int32_t -Dsint16=int16_t -Dsint8=int8_t

include $(IDF_PATH)/make/project.mk

# Ensure these overrides are always used
CC:=$(CC) $(BASIC_TYPES) -D__ESP32__ $(MORE_CFLAGS)

endif

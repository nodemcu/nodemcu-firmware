# -*- coding: utf-8, tab-width: 2 -*-

# If we haven't got IDF_PATH set already, reinvoke with IDF_PATH in the environ
.NOTPARALLEL:

ifeq ($(IDF_PATH),)
THIS_MK_FILE:=$(notdir $(lastword $(MAKEFILE_LIST)))
THIS_DIR:=$(abspath $(dir $(lastword $(MAKEFILE_LIST))))
IDF_PATH=$(THIS_DIR)/sdk/esp32-esp-idf

TOOLCHAIN_RELEASES_BASEURL:=https://github.com/jmattsson/esp-toolchains/releases/
TOOLCHAIN_VERSION:=20181106.1
PLATFORM:=linux-$(shell uname --machine)

## Directory to place external modules:
export EXTRA_COMPONENT_DIRS:=$(THIS_DIR)/components/modules/external

ESP32_BIN:=$(THIS_DIR)/tools/toolchains/esp32-$(PLATFORM)-$(TOOLCHAIN_VERSION)/bin
ESP32_GCC:=$(ESP32_BIN)/xtensa-esp32-elf-gcc
ESP32_TOOLCHAIN_DL:=$(THIS_DIR)/cache/toolchain-esp32-$(PLATFORM)-$(TOOLCHAIN_VERSION).tar.xz

all: | $(ESP32_GCC)
%: | $(ESP32_GCC)
	@tools/update_buildinfo.sh
	@echo Setting IDF_PATH and re-invoking...
	@env IDF_PATH=$(IDF_PATH) PATH="$(PATH):$(ESP32_BIN)" $(MAKE) -f $(THIS_MK_FILE) $@
	@if test "$@" = "clean"; then rm -rf $(THIS_DIR)/tools/toolchains/esp32-*; fi

install_toolchain: $(ESP32_GCC)

$(ESP32_GCC): $(ESP32_TOOLCHAIN_DL)
	@echo Uncompressing toolchain
	@mkdir -p $(THIS_DIR)/tools/toolchains/
	@tar -xJf $< -C $(THIS_DIR)/tools/toolchains/
	# the archive contains ro files
	@chmod -R u+w $(THIS_DIR)/tools/toolchains/esp32-*
	@touch $@

download_toolchain: $(ESP32_TOOLCHAIN_DL)

$(ESP32_TOOLCHAIN_DL):
	@mkdir -p $(THIS_DIR)/cache
	wget --tries=10 --timeout=15 --waitretry=30 --read-timeout=20 --retry-connrefused ${TOOLCHAIN_RELEASES_BASEURL}download/$(PLATFORM)-$(TOOLCHAIN_VERSION)/toolchain-esp32-$(PLATFORM)-$(TOOLCHAIN_VERSION).tar.xz -O $@ || { rm -f -- "$@"; echo "W: Download failed. Please check ${TOOLCHAIN_RELEASES_BASEURL} for an appropriate version. If there is none for $(PLATFORM), you might need to compile it yourself."; exit 1; }

else

PROJECT_NAME:=NodeMCU

# This is, sadly, the cleanest way to resolve the different non-standard
# conventions for sized integers across the various components.
BASIC_TYPES=-Du32_t=uint32_t -Du16_t=uint16_t -Du8_t=uint8_t -Ds32_t=int32_t -Ds16_t=int16_t -Duint32=uint32_t -Duint16=uint16_t -Duint8=uint8_t -Dsint32=int32_t -Dsint16=int16_t -Dsint8=int8_t

$(IDF_PATH)/make/project.mk:
	@echo "ESP-IDF isn't properly installed, will try to re-install and then re-run make:"
	git submodule init
	git submodule update --recursive
	python -m pip install --user --requirement $(IDF_PATH)/requirements.txt || echo "W: pip failed, but this might be harmless. Let's just try whether it works anyway." >&2
  # If we'd just continue, make would fake-succeed albeit with a warning
  # "make[1]: *** No rule to make target `menuconfig'.  Stop."
#	$(MAKE) -f $(THIS_MK_FILE) $@ || exit $?

include $(IDF_PATH)/make/project.mk

# Ensure these overrides are always used
CC:=$(CC) $(BASIC_TYPES) -D__ESP32__ $(MORE_CFLAGS)

endif

extmod-update:
	@tools/extmod/extmod.sh update

extmod-clean:
	@tools/extmod/extmod.sh clean

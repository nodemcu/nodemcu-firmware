#  copyright (c) 2010 Espressif System
#
.NOTPARALLEL:

TOP_DIR:=$(abspath $(dir $(lastword $(MAKEFILE_LIST))))

# SDK base version, as released by Espressif depends on the RELEASE flag
#
# RELEASE = lastest pulls the latest V3.0.0 branch version as at the issue of this make
# otherwise it pulls the labelled version in the SDK version's release directory
#
ifeq ("$(RELEASE)","latest-3.0")
  SDK_VER        := 3.0.0
  SDK_FILE_SHA1  := NA
  SDK_ZIP_ROOT   := ESP8266_NONOS_SDK-release-v$(SDK_VER)
  SDK_FILE_VER   := release/v$(SDK_VER)
else ifeq ("$(RELEASE)","master")
  SDK_VER        := master
  SDK_FILE_SHA1  := NA
  SDK_ZIP_ROOT   := ESP8266_NONOS_SDK-$(SDK_VER)
  SDK_FILE_VER   := $(SDK_VER)
else
# SDK_VER        := 3.0
# SDK_FILE_VER   := v$(SDK_VER)
  SDK_FILE_VER   := e4434aa730e78c63040ace360493aef420ec267c
  SDK_VER        := 3.0-e4434aa
  SDK_FILE_SHA1  := ac6528a6a206d3d4c220e4035ced423eb314cfbf
  SDK_ZIP_ROOT   := ESP8266_NONOS_SDK-$(SDK_FILE_VER)
endif
SDK_REL_DIR      := sdk/esp_iot_sdk_v$(SDK_VER)
SDK_DIR          := $(TOP_DIR)/$(SDK_REL_DIR)
APP_DIR          := $(TOP_DIR)/app

ESPTOOL_VER := 2.6

# Ensure that the Espresif SDK is search before application paths and also prevent
# the SDK's c_types.h from being included from anywhere, by predefining its include-guard.

CCFLAGS :=  $(CCFLAGS) -I $(PDIR)sdk-overrides/include -I $(SDK_DIR)/include -D_C_TYPES_H_
LDFLAGS := -L$(SDK_DIR)/lib -L$(SDK_DIR)/ld $(LDFLAGS)

ifdef DEBUG
  CCFLAGS += -ggdb -O0
  LDFLAGS += -ggdb
else
  CCFLAGS += -O2
endif


# Handling of V=1/VERBOSE=1 flag
#
# if V=1, $(summary) does nothing
# if V is unset or not 1, $(summary) echoes a summary
VERBOSE ?=
V ?= $(VERBOSE)
ifeq ("$(V)","1")
  export summary := @true
else
  export summary := @echo
  # disable echoing of commands, directory names
  MAKEFLAGS += --silent -w
endif  # $(V)==1

ifndef BAUDRATE
  BAUDRATE=115200
endif

#############################################################
# Select compile
#
#  ** HEALTH WARNING ** This section is largely legacy directives left over from
#  an Espressif template.  As far as I (TerrryE) know, we've only used the Linux
#  Path. I have successfully build AMD and Intel (both x86, AMD64) and RPi ARM6
#  all under Ubuntu.  Our docker container runs on Windows in an Ubuntu VM.
#  Johny Mattson maintains a prebuild AMD64 xtensa cross-compile gcc v4.8.5
#  toolchain which is compatible with the non-OS SDK and can be used on any recent
#  Ubuntu version including the Docker and Travis build environments.
#
#  You have the option to build your own toolchain and specify a TOOLCHAIN_ROOT
#  environment variable (see https://github.com/pfalcon/esp-open-sdk).  If your
#  architecture is compatable then you can omit this variable and the make will
#  download and use this prebuilt toolchain.
#
#  If any developers wish to develop, test and support alternative environments
#  then please raise a GitHub issue on this work.
#

ifndef $(OS)
  # Assume Windows if MAKE_HOST contains "indows" and Linux otherwise
  ifneq (,$(findstring indows,$(MAKE_HOST)))
    OS := windows
  else
    OS := linux
  endif
endif

ifneq (,$(findstring indows,$(OS)))
  #------------ BEGIN UNTESTED ------------ We are not under Linux, e.g.under windows.
  ifeq ($(XTENSA_CORE),lx106)
    # It is xcc
    AR = xt-ar
    CC = xt-xcc
    CXX = xt-xcc
    NM = xt-nm
    CPP = xt-cpp
    OBJCOPY = xt-objcopy
    #MAKE = xt-make
    CCFLAGS += --rename-section .text=.irom0.text --rename-section .literal=.irom0.literal
  else
    # It is gcc, may be cygwin
    # Can we use -fdata-sections?
    CCFLAGS += -ffunction-sections -fno-jump-tables -fdata-sections -fpack-struct=4
    AR = xtensa-lx106-elf-ar
    CC = xtensa-lx106-elf-gcc
    CXX = xtensa-lx106-elf-g++
    NM = xtensa-lx106-elf-nm
    CPP = xtensa-lx106-elf-cpp
    OBJCOPY = xtensa-lx106-elf-objcopy
  endif
  FIRMWAREDIR = ..\\bin\\
  ifndef COMPORT
    ESPPORT = com1
  else
    ESPPORT = $(COMPORT)
  endif
  ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
# ->AMD64
  endif
  ifeq ($(PROCESSOR_ARCHITECTURE),x86)
# ->IA32
  endif
  #---------------- END UNTESTED ---------------- We are under windows.
else
  # We are under other system, may be Linux. Assume using gcc.

  UNAME_S := $(shell uname -s)
  UNAME_P := $(shell uname -p)
  ifeq ($(OS),linux)
    ifndef TOOLCHAIN_ROOT
      TOOLCHAIN_VERSION = 20181106.0
      GCCTOOLCHAIN      = linux-x86_64-$(TOOLCHAIN_VERSION)
      TOOLCHAIN_ROOT    = $(TOP_DIR)/tools/toolchains/esp8266-$(GCCTOOLCHAIN)
      GITHUB_TOOLCHAIN  = https://github.com/jmattsson/esp-toolchains
      GITHUB_ESPRESSIF_SDK = yes
      export PATH:=$(PATH):$(TOOLCHAIN_ROOT)/bin
    endif
  endif

  ifndef COMPORT
    ESPPORT = /dev/ttyUSB0
  else
    ESPPORT = $(COMPORT)
  endif

  CCFLAGS += -ffunction-sections -fno-jump-tables -fdata-sections
  AR      = xtensa-lx106-elf-ar
  CC      = $(WRAPCC) xtensa-lx106-elf-gcc
  CXX     = $(WRAPCC) xtensa-lx106-elf-g++
  NM      = xtensa-lx106-elf-nm
  CPP     = $(WRAPCC) xtensa-lx106-elf-gcc -E
  OBJCOPY = xtensa-lx106-elf-objcopy
  FIRMWAREDIR = ../bin/
  WGET = wget --tries=10 --timeout=15 --waitretry=30 --read-timeout=20 --retry-connrefused
endif

GITHUB_SDK       = https://github.com/espressif/ESP8266_NONOS_SDK
GITHUB_ESPTOOL   = https://github.com/espressif/esptool

ESPTOOL ?= $(TOP_DIR)/tools/toolchains/esptool.py

SUBDIRS ?= $(patsubst %/,%,$(dir $(filter-out tools/Makefile,$(wildcard */Makefile))))

ODIR    := .output

ifdef TARGET
CSRCS   ?= $(wildcard *.c)
CXXSRCS ?= $(wildcard *.cpp)
ASRCs   ?= $(wildcard *.s)
ASRCS   ?= $(wildcard *.S)

OBJODIR := $(ODIR)/$(TARGET)/$(FLAVOR)/obj

OBJS := $(CSRCS:%.c=$(OBJODIR)/%.o) \
        $(CXXSRCS:%.cpp=$(OBJODIR)/%.o) \
        $(ASRCs:%.s=$(OBJODIR)/%.o) \
        $(ASRCS:%.S=$(OBJODIR)/%.o)

DEPS := $(CSRCS:%.c=$(OBJODIR)/%.d) \
        $(CXXSCRS:%.cpp=$(OBJODIR)/%.d) \
        $(ASRCs:%.s=$(OBJODIR)/%.d) \
        $(ASRCS:%.S=$(OBJODIR)/%.d)

LIBODIR := $(ODIR)/$(TARGET)/$(FLAVOR)/lib
OLIBS := $(GEN_LIBS:%=$(LIBODIR)/%)

IMAGEODIR := $(ODIR)/$(TARGET)/$(FLAVOR)/image
OIMAGES := $(GEN_IMAGES:%=$(IMAGEODIR)/%)

BINODIR := $(ODIR)/$(TARGET)/$(FLAVOR)/bin
OBINS := $(GEN_BINS:%=$(BINODIR)/%)

ifndef PDIR
  ifneq ($(wildcard $(TOP_DIR)/local/fs/*),)
    SPECIAL_MKTARGETS += spiffs-image
  else
    SPECIAL_MKTARGETS += spiffs-image-remove
  endif
endif
endif   # TARGET
#
# Note:
# https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
# If you add global optimize options then they will override "-Os" defined above.
# Note that "-Os" should NOT be used to reduce code size because of the runtime
# impact of the extra non-aligned exception burdon.
#
CCFLAGS += 			\
	-g			\
	-Wpointer-arith		\
	-Wundef			\
	-Werror			\
	-Wl,-EL			\
	-fno-inline-functions	\
	-nostdlib       \
	-mlongcalls	\
	-mtext-section-literals \
#	-Wall

CFLAGS = $(CCFLAGS) $(DEFINES) $(EXTRA_CCFLAGS) $(STD_CFLAGS) $(INCLUDES)
DFLAGS = $(CCFLAGS) $(DDEFINES) $(EXTRA_CCFLAGS) $(STD_CFLAGS) $(INCLUDES)


#############################################################
# Functions
#

ifdef TARGET

define ShortcutRule
$(1): .subdirs $(2)/$(1)
endef

define MakeLibrary
DEP_LIBS_$(1) = $$(foreach lib,$$(filter %.a,$$(COMPONENTS_$(1))),$$(dir $$(lib))$$(LIBODIR)/$$(notdir $$(lib)))
DEP_OBJS_$(1) = $$(foreach obj,$$(filter %.o,$$(COMPONENTS_$(1))),$$(dir $$(obj))$$(OBJODIR)/$$(notdir $$(obj)))
$$(LIBODIR)/$(1).a: $$(OBJS) $$(DEP_OBJS_$(1)) $$(DEP_LIBS_$(1)) $$(DEPENDS_$(1))
	@mkdir -p $$(LIBODIR)
	$$(if $$(filter %.a,$$?),mkdir -p $$(EXTRACT_DIR)_$(1))
	$$(if $$(filter %.a,$$?),cd $$(EXTRACT_DIR)_$(1); $$(foreach lib,$$(filter %.a,$$?),$$(AR) xo $$(UP_EXTRACT_DIR)/$$(lib);))
	$(summary) AR $(patsubst $(TOP_DIR)/%,%,$(CURDIR))/$<
	$$(AR) ru $$@ $$(filter %.o,$$?) $$(if $$(filter %.a,$$?),$$(EXTRACT_DIR)_$(1)/*.o)
	$$(if $$(filter %.a,$$?),$$(RM) -r $$(EXTRACT_DIR)_$(1))
endef

define MakeImage
DEP_LIBS_$(1) = $$(foreach lib,$$(filter %.a,$$(COMPONENTS_$(1))),$$(dir $$(lib))$$(LIBODIR)/$$(notdir $$(lib)))
DEP_OBJS_$(1) = $$(foreach obj,$$(filter %.o,$$(COMPONENTS_$(1))),$$(dir $$(obj))$$(OBJODIR)/$$(notdir $$(obj)))
$$(IMAGEODIR)/$(1).out: $$(OBJS) $$(DEP_OBJS_$(1)) $$(DEP_LIBS_$(1)) $$(DEPENDS_$(1))
	@mkdir -p $$(IMAGEODIR)
	$(summary) LD $(patsubst $(TOP_DIR)/%,%,$(CURDIR))/$$@
	$$(CC) $$(LDFLAGS) $$(if $$(LINKFLAGS_$(1)),$$(LINKFLAGS_$(1)),$$(LINKFLAGS_DEFAULT) $$(OBJS) $$(DEP_OBJS_$(1)) $$(DEP_LIBS_$(1))) -o $$@
endef

$(BINODIR)/%.bin: $(IMAGEODIR)/%.out
	@mkdir -p $(BINODIR)
	$(summary) NM $(patsubst $(TOP_DIR)/%,%,$(CURDIR))/$@
	@$(NM) $< | grep -w U && { echo "Firmware has undefined (but unused) symbols!"; exit 1; } || true
	$(summary) ESPTOOL $(patsubst $(TOP_DIR)/%,%,$(CURDIR))/$< $(FIRMWAREDIR)
	$(ESPTOOL) elf2image --flash_mode dio --flash_freq 40m $< -o $(FIRMWAREDIR)

endif # TARGET
#############################################################
# Rules base
# Should be done in top-level makefile only
#

ifndef TARGET
all: toolchain sdk_pruned pre_build buildinfo .subdirs
else
all: .subdirs $(OBJS) $(OLIBS) $(OIMAGES) $(OBINS) $(SPECIAL_MKTARGETS)
endif

.PHONY: sdk_extracted
.PHONY: sdk_pruned
.PHONY: toolchain

sdk_extracted: $(TOP_DIR)/sdk/.extracted-$(SDK_VER)
sdk_pruned: sdk_extracted toolchain $(TOP_DIR)/sdk/.pruned-$(SDK_VER)

ifdef GITHUB_TOOLCHAIN
  TOOLCHAIN_ROOT := $(TOP_DIR)/tools/toolchains/esp8266-linux-x86_64-$(TOOLCHAIN_VERSION)

toolchain: $(TOOLCHAIN_ROOT)/bin $(ESPTOOL)

$(TOOLCHAIN_ROOT)/bin: $(TOP_DIR)/cache/toolchain-esp8266-$(GCCTOOLCHAIN).tar.xz
	mkdir -p $(TOP_DIR)/tools/toolchains/
	$(summary) EXTRACT $(patsubst $(TOP_DIR)/%,%,$<)
	tar -xJf $< -C $(TOP_DIR)/tools/toolchains/
	touch $@

$(TOP_DIR)/cache/toolchain-esp8266-$(GCCTOOLCHAIN).tar.xz:
	mkdir -p $(TOP_DIR)/cache
	$(summary) WGET $(patsubst $(TOP_DIR)/%,%,$@)
	$(WGET) $(GITHUB_TOOLCHAIN)/releases/download/$(GCCTOOLCHAIN)/toolchain-esp8266-$(GCCTOOLCHAIN).tar.xz -O $@ \
	|| { rm -f "$@"; exit 1; }
else
toolchain: $(ESPTOOL)
endif

$(ESPTOOL): $(TOP_DIR)/cache/esptool/v$(ESPTOOL_VER).tar.gz
	mkdir -p $(TOP_DIR)/tools/toolchains/
	tar -C $(TOP_DIR)/tools/toolchains/ -xzf $< --strip-components=1 esptool-$(ESPTOOL_VER)/esptool.py
	chmod +x $@
	touch $@

$(TOP_DIR)/cache/esptool/v$(ESPTOOL_VER).tar.gz:
	mkdir -p $(TOP_DIR)/cache/esptool/
	$(WGET) $(GITHUB_ESPTOOL)/archive/v$(ESPTOOL_VER).tar.gz -O $@ || { rm -f "$@"; exit 1; }

ifdef GITHUB_ESPRESSIF_SDK
$(TOP_DIR)/sdk/.extracted-$(SDK_VER): $(TOP_DIR)/cache/$(SDK_FILE_VER).zip
	mkdir -p "$(dir $@)"
	$(summary) UNZIP $(patsubst $(TOP_DIR)/%,%,$<)
	(cd "$(dir $@)" && \
	 rm -fr esp_iot_sdk_v$(SDK_VER) ESP8266_NONOS_SDK-* && \
	 unzip $(TOP_DIR)/cache/$(SDK_FILE_VER).zip \
	       '*/lib/*' \
	       '*/ld/*.v6.ld' \
	       '*/include/*' \
	       '*/bin/esp_init_data_default_v05.bin' \
	)
	mv $(dir $@)/$(SDK_ZIP_ROOT) $(dir $@)/esp_iot_sdk_v$(SDK_VER)
	touch $@

$(TOP_DIR)/sdk/.pruned-$(SDK_VER):
	rm -f $(SDK_DIR)/lib/liblwip.a $(SDK_DIR)/lib/libssl.a $(SDK_DIR)/lib/libmbedtls.a
	$(summary) PRUNE libmain.a libc.a
	echo $(PATH)
	$(AR) d $(SDK_DIR)/lib/libmain.a time.o
	$(AR) d $(SDK_DIR)/lib/libc.a lib_a-time.o
	touch $@

$(TOP_DIR)/cache/$(SDK_FILE_VER).zip:
	mkdir -p "$(dir $@)"
	$(summary) WGET $(patsubst $(TOP_DIR)/%,%,$@)
	$(WGET) $(GITHUB_SDK)/archive/$(SDK_FILE_VER).zip -O $@ || { rm -f "$@"; exit 1; }
	if test "$(SDK_FILE_SHA1)" != "NA"; then echo "$(SDK_FILE_SHA1)  $@" | sha1sum -c - || { rm -f "$@"; exit 1; }; fi
else
$(TOP_DIR)/sdk/.extracted-$(SDK_VER):
	echo "SDK provided"
	test -d $(dir $@)/esp_iot_sdk_v$(SDK_VER) || { echo "esp_iot_sdk_v$(SDK_VER) not found"; exit 1; };
$(TOP_DIR)/sdk/.pruned-$(SDK_VER):
	echo "SDK pruned"
endif

clean:
	$(foreach d, $(SUBDIRS), $(MAKE) -C $(d) clean;)
	$(RM) -r $(ODIR)/$(TARGET)/$(FLAVOR)
	$(RM) -r "$(TOP_DIR)/sdk"

clobber: $(SPECIAL_CLOBBER)
	$(foreach d, $(SUBDIRS), $(MAKE) -C $(d) clobber;)
	$(RM) -r $(ODIR)

flash:
	@echo "use one of the following targets to flash the firmware"
	@echo "  make flash512k     - for ESP with 512kB flash size"
	@echo "  make flash1m-dout  - for ESP with   1MB flash size and flash mode = dout (Sonoff, ESP8285)"
	@echo "  make flash4m       - for ESP with   4MB flash size"

flash512k:
	$(MAKE) -e FLASHOPTIONS="-fm qio -fs  4m -ff 40m" flashinternal

flash4m:
	$(MAKE) -e FLASHOPTIONS="-fm dio -fs 32m -ff 40m" flashinternal

flash1m-dout:
	$(MAKE) -e FLASHOPTIONS="-fm dout -fs 8m -ff 40m" flashinternal


flashinternal:
ifndef PDIR
	$(MAKE) -C $(APP_DIR) flashinternal
else
	$(ESPTOOL) --port $(ESPPORT) --baud $(BAUDRATE) write_flash $(FLASHOPTIONS) 0x00000 $(FIRMWAREDIR)0x00000.bin 0x10000 $(FIRMWAREDIR)0x10000.bin
endif

.subdirs:
	@set -e; $(foreach d, $(SUBDIRS), $(MAKE) -C $(d);)

ifneq ($(MAKECMDGOALS),clean)
  ifneq ($(MAKECMDGOALS),clobber)
    ifdef DEPS
      sinclude $(DEPS)
    endif
  endif
endif

.PHONY: spiffs-image-remove

spiffs-image-remove:
	$(MAKE) -C tools remove-image spiffsimg/spiffsimg

.PHONY: spiffs-image

spiffs-image: bin/0x10000.bin
	$(MAKE) -C tools
############ Note: this target needs moving into app/modules make ############
.PHONY: pre_build

ifneq ($(wildcard $(TOP_DIR)/server-ca.crt),)
pre_build: $(APP_DIR)/modules/server-ca.crt.h

$(APP_DIR)/modules/server-ca.crt.h: $(TOP_DIR)/server-ca.crt
	$(summary) MKCERT $(patsubst $(TOP_DIR)/%,%,$<)
	python $(TOP_DIR)/tools/make_server_cert.py $(TOP_DIR)/server-ca.crt > $(APP_DIR)/modules/server-ca.crt.h

DEFINES += -DHAVE_SSL_SERVER_CRT=\"server-ca.crt.h\"
else
pre_build:
	@-rm -f $(APP_DIR)/modules/server-ca.crt.h
endif

.PHONY: buildinfo

buildinfo:
	tools/update_buildinfo.sh

ifdef TARGET
$(OBJODIR)/%.o: %.c
	@mkdir -p $(dir $@);
	$(summary) CC $(patsubst $(TOP_DIR)/%,%,$(CURDIR))/$<
	$(CC) $(if $(findstring $<,$(DSRCS)),$(DFLAGS),$(CFLAGS)) $(COPTS_$(*F)) -o $@ -c $<

$(OBJODIR)/%.d: %.c
	@mkdir -p $(dir $@);
	$(summary) DEPEND: CC $(patsubst $(TOP_DIR)/%,%,$(CURDIR))/$<
	@set -e; rm -f $@; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\.o\)[ :]*,$(OBJODIR)/\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(OBJODIR)/%.o: %.cpp
	@mkdir -p $(OBJODIR);
	$(summary) CXX $(patsubst $(TOP_DIR)/%,%,$(CURDIR))/$<
	$(CXX) $(if $(findstring $<,$(DSRCS)),$(DFLAGS),$(CFLAGS)) $(COPTS_$(*F)) -o $@ -c $<

$(OBJODIR)/%.d: %.cpp
	@mkdir -p $(OBJODIR);
	$(summary) DEPEND: CXX $(patsubst $(TOP_DIR)/%,%,$(CURDIR))/$<
	@set -e; rm -f $@; \
	sed 's,\($*\.o\)[ :]*,$(OBJODIR)/\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(OBJODIR)/%.o: %.s
	@mkdir -p $(dir $@);
	$(summary) CC $(patsubst $(TOP_DIR)/%,%,$(CURDIR))/$<
	$(CC) $(CFLAGS) -o $@ -c $<

$(OBJODIR)/%.d: %.s
	@mkdir -p $(dir $@); \
	set -e; rm -f $@; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\.o\)[ :]*,$(OBJODIR)/\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(OBJODIR)/%.o: %.S
	@mkdir -p $(dir $@);
	$(summary) CC $(patsubst $(TOP_DIR)/%,%,$(CURDIR))/$<
	$(CC) $(CFLAGS) -D__ASSEMBLER__ -o $@ -c $<

$(OBJODIR)/%.d: %.S
	@mkdir -p $(dir $@); \
	set -e; rm -f $@; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\.o\)[ :]*,$(OBJODIR)/\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(foreach lib,$(GEN_LIBS),$(eval $(call ShortcutRule,$(lib),$(LIBODIR))))

$(foreach image,$(GEN_IMAGES),$(eval $(call ShortcutRule,$(image),$(IMAGEODIR))))

$(foreach bin,$(GEN_BINS),$(eval $(call ShortcutRule,$(bin),$(BINODIR))))

$(foreach lib,$(GEN_LIBS),$(eval $(call MakeLibrary,$(basename $(lib)))))

$(foreach image,$(GEN_IMAGES),$(eval $(call MakeImage,$(basename $(image)))))

endif # TARGET
#############################################################
# Recursion Magic - Don't touch this!!
#
# Each subtree potentially has an include directory
#   corresponding to the common APIs applicable to modules
#   rooted at that subtree. Accordingly, the INCLUDE PATH
#   of a module can only contain the include directories up
#   its parent path, and not its siblings
#
# Required for each makefile to inherit from the parent
#

PDIR := ../$(PDIR)
sinclude $(PDIR)Makefile

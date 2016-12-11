#  copyright (c) 2010 Espressif System
#
.NOTPARALLEL:

# SDK version NodeMCU is locked to
SDK_VER:=2.0.0

# no patch: SDK_BASE_VER equals SDK_VER and sdk dir depends on sdk_extracted
SDK_BASE_VER:=$(SDK_VER)
SDK_DIR_DEPENDS:=sdk_extracted
# with patch: SDK_BASE_VER differs from SDK_VER and sdk dir depends on sdk_patched
#SDK_BASE_VER:=1.5.4
#SDK_DIR_DEPENDS:=sdk_patched

SDK_FILE_VER:=$(SDK_BASE_VER)_16_08_10
SDK_FILE_SHA1:=b0127a99b45b3778be4a752387ab8dc0f6dd7810
#SDK_PATCH_VER:=$(SDK_VER)_patch_20160704
#SDK_PATCH_SHA1:=388d9e91df74e3b49fca126da482cf822cf1ebf1
# Ensure we search "our" SDK before the tool-chain's SDK (if any)
TOP_DIR:=$(abspath $(dir $(lastword $(MAKEFILE_LIST))))
SDK_DIR:=$(TOP_DIR)/sdk/esp_iot_sdk_v$(SDK_VER)
CCFLAGS:= -I$(TOP_DIR)/sdk-overrides/include -I$(SDK_DIR)/include
LDFLAGS:= -L$(SDK_DIR)/lib -L$(SDK_DIR)/ld $(LDFLAGS)

ifdef DEBUG
  CCFLAGS += -ggdb -O0
  LDFLAGS += -ggdb
else
  CCFLAGS += -Os
endif

#############################################################
# Select compile
#
ifeq ($(OS),Windows_NT)
# WIN32
# We are under windows.
	ifeq ($(XTENSA_CORE),lx106)
		# It is xcc
		AR = xt-ar
		CC = xt-xcc
		NM = xt-nm
		CPP = xt-cpp
		OBJCOPY = xt-objcopy
		#MAKE = xt-make
		CCFLAGS += --rename-section .text=.irom0.text --rename-section .literal=.irom0.literal
	else 
		# It is gcc, may be cygwin
		# Can we use -fdata-sections?
		CCFLAGS += -ffunction-sections -fno-jump-tables -fdata-sections
		AR = xtensa-lx106-elf-ar
		CC = xtensa-lx106-elf-gcc
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
else
# We are under other system, may be Linux. Assume using gcc.
	# Can we use -fdata-sections?
	ifndef COMPORT
		ESPPORT = /dev/ttyUSB0
	else
		ESPPORT = $(COMPORT)
	endif
	CCFLAGS += -ffunction-sections -fno-jump-tables -fdata-sections
	AR = xtensa-lx106-elf-ar
	CC = xtensa-lx106-elf-gcc
	NM = xtensa-lx106-elf-nm
	CPP = xtensa-lx106-elf-cpp
	OBJCOPY = xtensa-lx106-elf-objcopy
	FIRMWAREDIR = ../bin/
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
# LINUX
    endif
    ifeq ($(UNAME_S),Darwin)
# OSX
    endif
    UNAME_P := $(shell uname -p)
    ifeq ($(UNAME_P),x86_64)
# ->AMD64
    endif
    ifneq ($(filter %86,$(UNAME_P)),)
# ->IA32
    endif
    ifneq ($(filter arm%,$(UNAME_P)),)
# ->ARM
    endif
endif
#############################################################
ESPTOOL ?= ../tools/esptool.py


CSRCS ?= $(wildcard *.c)
ASRCs ?= $(wildcard *.s)
ASRCS ?= $(wildcard *.S)
SUBDIRS ?= $(patsubst %/,%,$(dir $(filter-out tools/Makefile,$(wildcard */Makefile))))

ODIR := .output
OBJODIR := $(ODIR)/$(TARGET)/$(FLAVOR)/obj

OBJS := $(CSRCS:%.c=$(OBJODIR)/%.o) \
        $(ASRCs:%.s=$(OBJODIR)/%.o) \
        $(ASRCS:%.S=$(OBJODIR)/%.o)

DEPS := $(CSRCS:%.c=$(OBJODIR)/%.d) \
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

#
# Note: 
# https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
# If you add global optimize options like "-O2" here 
# they will override "-Os" defined above.
# "-Os" should be used to reduce code size
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
	-mtext-section-literals
#	-Wall			

CFLAGS = $(CCFLAGS) $(DEFINES) $(EXTRA_CCFLAGS) $(STD_CFLAGS) $(INCLUDES)
DFLAGS = $(CCFLAGS) $(DDEFINES) $(EXTRA_CCFLAGS) $(STD_CFLAGS) $(INCLUDES)


#############################################################
# Functions
#

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
	$$(AR) ru $$@ $$(filter %.o,$$?) $$(if $$(filter %.a,$$?),$$(EXTRACT_DIR)_$(1)/*.o)
	$$(if $$(filter %.a,$$?),$$(RM) -r $$(EXTRACT_DIR)_$(1))
endef

define MakeImage
DEP_LIBS_$(1) = $$(foreach lib,$$(filter %.a,$$(COMPONENTS_$(1))),$$(dir $$(lib))$$(LIBODIR)/$$(notdir $$(lib)))
DEP_OBJS_$(1) = $$(foreach obj,$$(filter %.o,$$(COMPONENTS_$(1))),$$(dir $$(obj))$$(OBJODIR)/$$(notdir $$(obj)))
$$(IMAGEODIR)/$(1).out: $$(OBJS) $$(DEP_OBJS_$(1)) $$(DEP_LIBS_$(1)) $$(DEPENDS_$(1))
	@mkdir -p $$(IMAGEODIR)
	$$(CC) $$(LDFLAGS) $$(if $$(LINKFLAGS_$(1)),$$(LINKFLAGS_$(1)),$$(LINKFLAGS_DEFAULT) $$(OBJS) $$(DEP_OBJS_$(1)) $$(DEP_LIBS_$(1))) -o $$@ 
endef

$(BINODIR)/%.bin: $(IMAGEODIR)/%.out
	@mkdir -p $(BINODIR)
	$(ESPTOOL) elf2image $< -o $(FIRMWAREDIR)

#############################################################
# Rules base
# Should be done in top-level makefile only
#

all:	$(SDK_DIR_DEPENDS) pre_build .subdirs $(OBJS) $(OLIBS) $(OIMAGES) $(OBINS) $(SPECIAL_MKTARGETS)

.PHONY: sdk_extracted
.PHONY: sdk_patched

sdk_extracted: $(TOP_DIR)/sdk/.extracted-$(SDK_BASE_VER)
sdk_patched: sdk_extracted $(TOP_DIR)/sdk/.patched-$(SDK_VER)

$(TOP_DIR)/sdk/.extracted-$(SDK_BASE_VER): $(TOP_DIR)/cache/esp_iot_sdk_v$(SDK_FILE_VER).zip
	mkdir -p "$(dir $@)"
	(cd "$(dir $@)" && rm -fr esp_iot_sdk_v$(SDK_VER) ESP8266_NONOS_SDK && unzip $(TOP_DIR)/cache/esp_iot_sdk_v$(SDK_FILE_VER).zip ESP8266_NONOS_SDK/lib/* ESP8266_NONOS_SDK/ld/eagle.rom.addr.v6.ld ESP8266_NONOS_SDK/include/* ESP8266_NONOS_SDK/bin/esp_init_data_default.bin)
	mv $(dir $@)/ESP8266_NONOS_SDK $(dir $@)/esp_iot_sdk_v$(SDK_VER)
	rm -f $(SDK_DIR)/lib/liblwip.a
	touch $@

$(TOP_DIR)/sdk/.patched-$(SDK_VER): $(TOP_DIR)/cache/esp_iot_sdk_v$(SDK_PATCH_VER).zip
	mkdir -p "$(dir $@)/patch"
	(cd "$(dir $@)/patch" && unzip $(TOP_DIR)/cache/esp_iot_sdk_v$(SDK_PATCH_VER)*.zip *.a esp_init_data_default.bin && mv *.a $(SDK_DIR)/lib/ && mv esp_init_data_default.bin $(SDK_DIR)/bin/)
	rmdir $(dir $@)/patch
	rm -f $(SDK_DIR)/lib/liblwip.a
	touch $@

$(TOP_DIR)/cache/esp_iot_sdk_v$(SDK_FILE_VER).zip:
	mkdir -p "$(dir $@)"
	wget --tries=10 --timeout=15 --waitretry=30 --read-timeout=20 --retry-connrefused http://espressif.com/sites/default/files/sdks/esp8266_nonos_sdk_v$(SDK_FILE_VER).zip -O $@ || { rm -f "$@"; exit 1; }
	(echo "$(SDK_FILE_SHA1)  $@" | sha1sum -c -) || { rm -f "$@"; exit 1; }

$(TOP_DIR)/cache/esp_iot_sdk_v$(SDK_PATCH_VER).zip:
	mkdir -p "$(dir $@)"
	wget --tries=10 --timeout=15 --waitretry=30 --read-timeout=20 --retry-connrefused http://espressif.com/sites/default/files/sdks/esp8266_nonos_sdk_v$(SDK_PATCH_VER).zip -O $@ || { rm -f "$@"; exit 1; }
	(echo "$(SDK_PATCH_SHA1)  $@" | sha1sum -c -) || { rm -f "$@"; exit 1; }

clean:
	$(foreach d, $(SUBDIRS), $(MAKE) -C $(d) clean;)
	$(RM) -r $(ODIR)/$(TARGET)/$(FLAVOR)
	$(RM) -r "$(TOP_DIR)/sdk"

clobber: $(SPECIAL_CLOBBER)
	$(foreach d, $(SUBDIRS), $(MAKE) -C $(d) clobber;)
	$(RM) -r $(ODIR)

flash:
	@echo "use one of the following targets to flash the firmware"
	@echo "  make flash512k - for ESP with 512kB flash size"
	@echo "  make flash4m   - for ESP with   4MB flash size"

flash512k:
	$(MAKE) -e FLASHOPTIONS="-fm qio -fs  4m -ff 40m" flashinternal

flash4m:
	$(MAKE) -e FLASHOPTIONS="-fm dio -fs 32m -ff 40m" flashinternal

flashinternal:
ifndef PDIR
	$(MAKE) -C ./app flashinternal
else
	$(ESPTOOL) --port $(ESPPORT) write_flash $(FLASHOPTIONS) 0x00000 $(FIRMWAREDIR)0x00000.bin 0x10000 $(FIRMWAREDIR)0x10000.bin
endif

.subdirs:
	@set -e; $(foreach d, $(SUBDIRS), $(MAKE) -C $(d);)

#.subdirs:
#	$(foreach d, $(SUBDIRS), $(MAKE) -C $(d))

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

.PHONY: pre_build

ifneq ($(wildcard $(TOP_DIR)/server-ca.crt),)
pre_build: $(TOP_DIR)/app/modules/server-ca.crt.h

$(TOP_DIR)/app/modules/server-ca.crt.h: $(TOP_DIR)/server-ca.crt
	python $(TOP_DIR)/tools/make_server_cert.py $(TOP_DIR)/server-ca.crt > $(TOP_DIR)/app/modules/server-ca.crt.h

DEFINES += -DHAVE_SSL_SERVER_CRT=\"server-ca.crt.h\"
else
pre_build:
	@-rm -f $(TOP_DIR)/app/modules/server-ca.crt.h
endif


$(OBJODIR)/%.o: %.c
	@mkdir -p $(OBJODIR);
	$(CC) $(if $(findstring $<,$(DSRCS)),$(DFLAGS),$(CFLAGS)) $(COPTS_$(*F)) -o $@ -c $<

$(OBJODIR)/%.d: %.c
	@mkdir -p $(OBJODIR);
	@echo DEPEND: $(CC) -M $(CFLAGS) $<
	@set -e; rm -f $@; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\.o\)[ :]*,$(OBJODIR)/\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(OBJODIR)/%.o: %.s
	@mkdir -p $(OBJODIR);
	$(CC) $(CFLAGS) -o $@ -c $<

$(OBJODIR)/%.d: %.s
	@mkdir -p $(OBJODIR); \
	set -e; rm -f $@; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\.o\)[ :]*,$(OBJODIR)/\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(OBJODIR)/%.o: %.S
	@mkdir -p $(OBJODIR);
	$(CC) $(CFLAGS) -D__ASSEMBLER__ -o $@ -c $<

$(OBJODIR)/%.d: %.S
	@mkdir -p $(OBJODIR); \
	set -e; rm -f $@; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\.o\)[ :]*,$(OBJODIR)/\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(foreach lib,$(GEN_LIBS),$(eval $(call ShortcutRule,$(lib),$(LIBODIR))))

$(foreach image,$(GEN_IMAGES),$(eval $(call ShortcutRule,$(image),$(IMAGEODIR))))

$(foreach bin,$(GEN_BINS),$(eval $(call ShortcutRule,$(bin),$(BINODIR))))

$(foreach lib,$(GEN_LIBS),$(eval $(call MakeLibrary,$(basename $(lib)))))

$(foreach image,$(GEN_IMAGES),$(eval $(call MakeImage,$(basename $(image)))))

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

INCLUDES := $(INCLUDES) -I $(PDIR)include -I $(PDIR)include/$(TARGET)
PDIR := ../$(PDIR)
sinclude $(PDIR)Makefile

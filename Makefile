#  copyright (c) 2010 Espressif System
#
.NOTPARALLEL:

# SDK base version, as released by Espressif
SDK_BASE_VER:=2.2.1

# no patch: SDK_VER equals SDK_BASE_VER and sdk dir depends on sdk_extracted
SDK_VER:=$(SDK_BASE_VER)
SDK_DIR_DEPENDS:=sdk_extracted

# with patch: SDK_VER differs from SDK_BASE_VER and sdk dir depends on sdk_patched
#SDK_PATCH_VER:=f8f27ce
#SDK_VER:=$(SDK_BASE_VER)-$(SDK_PATCH_VER)
#SDK_DIR_DEPENDS:=sdk_patched

SDK_FILE_VER:=$(SDK_BASE_VER)
SDK_FILE_SHA1:=48f2242d5895823709f222bf0fffce9d525996c8
# SDK_PATCH_SHA1:=0bc21ec77b08488f04d3e1c9d161b711d07201a8
# Ensure we search "our" SDK before the tool-chain's SDK (if any)
TOP_DIR:=$(abspath $(dir $(lastword $(MAKEFILE_LIST))))
SDK_REL_DIR=sdk/esp_iot_sdk_v$(SDK_VER)
SDK_DIR:=$(TOP_DIR)/$(SDK_REL_DIR)
CCFLAGS:= -I$(TOP_DIR)/sdk-overrides/include -I$(TOP_DIR)/app/include/lwip/app -I$(SDK_DIR)/include
LDFLAGS:= -L$(SDK_DIR)/lib -L$(SDK_DIR)/ld $(LDFLAGS)

ifdef DEBUG
  CCFLAGS += -ggdb -O0
  LDFLAGS += -ggdb
else
  CCFLAGS += -O2
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
		CXX = xt-xcc
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
	CC = $(WRAPCC) xtensa-lx106-elf-gcc
	CXX = $(WRAPCC) xtensa-lx106-elf-g++
	NM = xtensa-lx106-elf-nm
	CPP = $(WRAPCC) xtensa-lx106-elf-gcc -E
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
CXXSRCS ?= $(wildcard *.cpp)
ASRCs ?= $(wildcard *.s)
ASRCS ?= $(wildcard *.S)
SUBDIRS ?= $(patsubst %/,%,$(dir $(filter-out tools/Makefile,$(wildcard */Makefile))))

ODIR := .output
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
	@$(NM) $< | grep -w U && { echo "Firmware has undefined (but unused) symbols!"; exit 1; } || true
	$(ESPTOOL) elf2image --flash_mode dio --flash_freq 40m $< -o $(FIRMWAREDIR)

#############################################################
# Rules base
# Should be done in top-level makefile only
#

all:	sdk_pruned pre_build .subdirs $(OBJS) $(OLIBS) $(OIMAGES) $(OBINS) $(SPECIAL_MKTARGETS)

.PHONY: sdk_extracted
.PHONY: sdk_patched
.PHONY: sdk_pruned

sdk_extracted: $(TOP_DIR)/sdk/.extracted-$(SDK_BASE_VER)
sdk_patched: sdk_extracted $(TOP_DIR)/sdk/.patched-$(SDK_VER)
sdk_pruned: $(SDK_DIR_DEPENDS) $(TOP_DIR)/sdk/.pruned-$(SDK_VER)

$(TOP_DIR)/sdk/.extracted-$(SDK_BASE_VER): $(TOP_DIR)/cache/v$(SDK_FILE_VER).zip
	mkdir -p "$(dir $@)"
	(cd "$(dir $@)" && rm -fr esp_iot_sdk_v$(SDK_VER) ESP8266_NONOS_SDK-$(SDK_BASE_VER) && unzip $(TOP_DIR)/cache/v$(SDK_FILE_VER).zip ESP8266_NONOS_SDK-$(SDK_BASE_VER)/lib/* ESP8266_NONOS_SDK-$(SDK_BASE_VER)/ld/eagle.rom.addr.v6.ld ESP8266_NONOS_SDK-$(SDK_BASE_VER)/include/* ESP8266_NONOS_SDK-$(SDK_BASE_VER)/bin/esp_init_data_default_v05.bin)
	mv $(dir $@)/ESP8266_NONOS_SDK-$(SDK_BASE_VER) $(dir $@)/esp_iot_sdk_v$(SDK_BASE_VER)
	touch $@

$(TOP_DIR)/sdk/.patched-$(SDK_VER): $(TOP_DIR)/cache/$(SDK_PATCH_VER).patch
	mv $(dir $@)/esp_iot_sdk_v$(SDK_BASE_VER) $(dir $@)/esp_iot_sdk_v$(SDK_VER)
	git apply --verbose -p1 --exclude='*VERSION' --exclude='*bin/at*' --directory=$(SDK_REL_DIR) $<
	touch $@

$(TOP_DIR)/sdk/.pruned-$(SDK_VER):
	rm -f $(SDK_DIR)/lib/liblwip.a $(SDK_DIR)/lib/libssl.a $(SDK_DIR)/lib/libmbedtls.a
	$(AR) d $(SDK_DIR)/lib/libmain.a time.o
	$(AR) d $(SDK_DIR)/lib/libc.a lib_a-time.o
	touch $@

$(TOP_DIR)/cache/v$(SDK_FILE_VER).zip:
	mkdir -p "$(dir $@)"
	wget --tries=10 --timeout=15 --waitretry=30 --read-timeout=20 --retry-connrefused https://github.com/espressif/ESP8266_NONOS_SDK/archive/v$(SDK_FILE_VER).zip -O $@ || { rm -f "$@"; exit 1; }
	(echo "$(SDK_FILE_SHA1)  $@" | sha1sum -c -) || { rm -f "$@"; exit 1; }

$(TOP_DIR)/cache/$(SDK_PATCH_VER).patch:
	mkdir -p "$(dir $@)"
	wget --tries=10 --timeout=15 --waitretry=30 --read-timeout=20 --retry-connrefused "https://github.com/espressif/ESP8266_NONOS_SDK/compare/v$(SDK_BASE_VER)...$(SDK_PATCH_VER).patch" -O $@ || { rm -f "$@"; exit 1; }
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
	@mkdir -p $(dir $@);
	$(CC) $(if $(findstring $<,$(DSRCS)),$(DFLAGS),$(CFLAGS)) $(COPTS_$(*F)) -o $@ -c $<

$(OBJODIR)/%.d: %.c
	@mkdir -p $(dir $@);
	@echo DEPEND: $(CC) -M $(CFLAGS) $<
	@set -e; rm -f $@; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\.o\)[ :]*,$(OBJODIR)/\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(OBJODIR)/%.o: %.cpp
	@mkdir -p $(OBJODIR);
	$(CXX) $(if $(findstring $<,$(DSRCS)),$(DFLAGS),$(CFLAGS)) $(COPTS_$(*F)) -o $@ -c $<

$(OBJODIR)/%.d: %.cpp
	@mkdir -p $(OBJODIR);
	@echo DEPEND: $(CXX) -M $(CFLAGS) $<
	@set -e; rm -f $@; \
	sed 's,\($*\.o\)[ :]*,$(OBJODIR)/\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(OBJODIR)/%.o: %.s
	@mkdir -p $(dir $@);
	$(CC) $(CFLAGS) -o $@ -c $<

$(OBJODIR)/%.d: %.s
	@mkdir -p $(dir $@); \
	set -e; rm -f $@; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\.o\)[ :]*,$(OBJODIR)/\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(OBJODIR)/%.o: %.S
	@mkdir -p $(dir $@);
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

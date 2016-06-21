#  copyright (c) 2010 Espressif System
#                2015-2016 NodeMCU team
#
.NOTPARALLEL:

# Get rid of most of the implicit rules by clearing the .SUFFIXES target
.SUFFIXES:
# Get rid of the auto-checkout from old version control systems rules
%: %,v
%: RCS/%,v
%: RCS/%
%: s.%
%: SCCS/s.%

# Set up flags and paths based on hardware target
HW?=ESP8266
ifeq ($(HW),ESP8266)
  TARGET=esp8266
	TARGET_SDK_LIBS=espconn cirom mirom nopoll
	TARGET_LDFLAGS=-u _UserExceptionVectorOverride -Wl,--wrap=printf
	LD_FILE=$(LDDIR)/nodemcu.ld
	AR = xtensa-lx106-elf-ar
	CC = xtensa-lx106-elf-gcc
	NM = xtensa-lx106-elf-nm
	CPP = xtensa-lx106-elf-cpp
	OBJCOPY = xtensa-lx106-elf-objcopy
else ifeq ($(HW),ESP32)
  TARGET=esp32
	TARGET_SDK_LIBS=rtc c m driver
	TARGET_LDFLAGS=
	LD_FILE=$(LDDIR)/nodemcu32.ld
	AR = xtensa-esp108-elf-ar
	CC = xtensa-esp108-elf-gcc
	NM = xtensa-esp108-elf-nm
	CPP = xtensa-esp108-elf-cpp
	OBJCOPY = xtensa-esp108-elf-objcopy
else
  $(error Unsupported hardware platform: $(HW))
endif

# Ensure we search "our" SDK before the tool-chain's SDK (if any)
TOP_DIR:=$(abspath $(dir $(lastword $(MAKEFILE_LIST))))
SDKDIR:=$(TOP_DIR)/sdk/$(TARGET)-rtos-sdk

# This is, sadly, the cleanest way to resolve the different non-standard
# conventions for sized integers across the various components.
BASIC_TYPES=-Du32_t=uint32_t -Du16_t=uint16_t -Du8_t=uint8_t -Ds32_t=int32_t -Ds16_t=int16_t -Duint32=uint32_t -Duint16=uint16_t -Duint8=uint8_t -Dsint32=int32_t -Dsint16=int16_t -Dsint8=int8_t

# Include dirs, ensure the overrides come first
INCLUDE_DIRS=\
  $(TOP_DIR)/sdk-overrides/include \
	$(TOP_DIR)/sdk-overrides/$(TARGET)-include \
	$(SDKDIR)/include \
	$(SDKDIR)/include/espressif \
	$(SDKDIR)/include/espressif/$(TARGET) \
	$(SDKDIR)/include/$(TARGET) \
	$(SDKDIR)/include/lwip \
	$(SDKDIR)/include/lwip/ipv4 \
	$(SDKDIR)/include/lwip/ipv6 \
	$(SDKDIR)/extra_include \
	$(SDKDIR)/third_party/include \
	$(SDKDIR)/third_party/include/lwip \
	$(SDKDIR)/third_party/include/lwip/ipv4 \
	$(SDKDIR)/third_party/include/lwip/ipv6 \
	$(SDKDIR)/driver_lib/include \

# ... and we have to mark them all as system include dirs rather than the usual
# -I for user include dir, or the esp-open-sdk toolchain headers wreak havoc
CCFLAGS:=$(addprefix -isystem,$(INCLUDE_DIRS)) $(BASIC_TYPES) -D__$(HW)__

LDFLAGS:= -L$(SDKDIR)/lib -L$(SDKDIR)/ld $(LDFLAGS)


#############################################################
ifndef COMPORT
	ESPPORT = /dev/ttyUSB0
else
	ESPPORT = $(COMPORT)
endif
CCFLAGS += -Os -ffunction-sections -fno-jump-tables -fdata-sections

FIRMWAREDIR = ../bin/$(TARGET)/

#############################################################
ESPTOOL ?= ../tools/esptool.py


CSRCS ?= $(wildcard *.c)
ASRCs ?= $(wildcard *.s)
ASRCS ?= $(wildcard *.S)
SUBDIRS ?= $(patsubst %/,%,$(dir $(wildcard */Makefile)))

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
	@mkdir -p $(BINODIR) $(FIRMWAREDIR)
ifeq ($(HW),ESP8266)
	$(ESPTOOL) elf2image $< -o $(FIRMWAREDIR)
else ifeq ($(HW),ESP32)
	@rm -f $(FIRMWAREDIR)/*
	@$(OBJCOPY) --only-section .text -O binary $< eagle.app.v7.text.bin
	@$(OBJCOPY) --only-section .data -O binary $< eagle.app.v7.data.bin
	@$(OBJCOPY) --only-section .rodata -O binary $< eagle.app.v7.rodata.bin
	@$(OBJCOPY) --only-section .irom0.text -O binary $< eagle.app.v7.irom0text.bin
	@$(OBJCOPY) --only-section .drom0.text -O binary $< eagle.app.v7.drom0text.bin
	@python $(SDKDIR)/tools/gen_appbin.py $< $(LD_FILE) 0 0 unused_app_cpu.bin $(FIRMWAREDIR)
	@rm -f eagle.app.v7.*.bin
endif

#############################################################
# Rules base
# Should be done in top-level makefile only
#

all:pre_build .subdirs $(OBJS) $(OLIBS) $(OIMAGES) $(OBINS) $(SPECIAL_MKTARGETS)

clean:
	$(foreach d, $(SUBDIRS), $(MAKE) -C $(d) clean;)
	$(RM) -r $(ODIR)/$(TARGET)/$(FLAVOR)

clobber: $(SPECIAL_CLOBBER)
	$(foreach d, $(SUBDIRS), $(MAKE) -C $(d) clobber;)
	$(RM) -r $(ODIR)

.subdirs:
	@set -e; $(foreach d, $(SUBDIRS), $(MAKE) -C $(d);)

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),clobber)
ifdef DEPS
sinclude $(DEPS)
endif
endif
endif

.PHONY: pre_build

ifneq ($(wildcard $(TOP_DIR)/server-ca.crt),)
pre_build:
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

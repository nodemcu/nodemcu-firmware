#############################################################
# Required variables for each makefile
# Discard this section from all parent makefiles
# Expected variables (with automatic defaults):
#   CSRCS (all "C" files in the dir)
#   SUBDIRS (all subdirs with a Makefile)
#   GEN_LIBS - list of libs to be generated ()
#   GEN_IMAGES - list of object file images to be generated ()
#   GEN_BINS - list of binaries to be generated ()
#   COMPONENTS_xxx - a list of libs/objs in the form
#     subdir/lib to be extracted and rolled up into
#     a generated lib/image xxx.a ()
#
TARGET = eagle
#FLAVOR = release
FLAVOR = debug

#EXTRA_CCFLAGS += -u

ifndef PDIR # {
GEN_IMAGES= eagle.app.v6.out
GEN_BINS= eagle.app.v6.bin
SPECIAL_MKTARGETS=$(APP_MKTARGETS)
SUBDIRS=    \
	user	\
	driver	\
	lwip	\
	json	\
	upgrade	\
	platform	\
	libc 	\
	lua 	\
	coap	\
	mqtt	\
	u8glib  \
	smart 	\
	wofs 	\
	modules	\
	spiffs	\
	cjson

endif # } PDIR

APPDIR = .
LDDIR = ../ld

CCFLAGS += -Os

TARGET_LDFLAGS =		\
	-nostdlib		\
	-Wl,-EL \
	--longcalls \
	--text-section-literals

ifeq ($(FLAVOR),debug)
    TARGET_LDFLAGS += -g -Os
endif

ifeq ($(FLAVOR),release)
    TARGET_LDFLAGS += -Os
endif

LD_FILE = $(LDDIR)/eagle.app.v6.ld

ifeq ($(APP), 1)
	LD_FILE = $(LDDIR)/eagle.app.v6.app1.ld
endif

ifeq ($(APP), 2)
	LD_FILE = $(LDDIR)/eagle.app.v6.app2.ld
endif

COMPONENTS_eagle.app.v6 = \
	user/libuser.a	\
	driver/libdriver.a	\
	lwip/liblwip.a	\
	json/libjson.a	\
	upgrade/libupgrade.a	\
	platform/libplatform.a	\
	libc/liblibc.a 	\
	lua/liblua.a 	\
	coap/coap.a 	\
	mqtt/mqtt.a 	\
	u8glib/u8glib.a \
	smart/smart.a 	\
	wofs/wofs.a 	\
	spiffs/spiffs.a 	\
	cjson/libcjson.a 	\
	modules/libmodules.a

LINKFLAGS_eagle.app.v6 = \
	-L../lib        \
	-Wl,--gc-sections \
	-Xlinker -Map=mapfile \
	-nostdlib	\
    -T$(LD_FILE)   \
	-Wl,--no-check-sections	\
    -u call_user_start	\
	-Wl,-static						\
	-Wl,--start-group					\
	-lc					\
	-lgcc					\
	-lhal					\
	-lphy	\
	-lpp	\
	-lnet80211	\
	-lwpa	\
	-lmain	\
	-ljson	\
	-lsmartconfig	\
	-lssl	\
	$(DEP_LIBS_eagle.app.v6)					\
	-Wl,--end-group			\
	-lm

DEPENDS_eagle.app.v6 = \
                $(LD_FILE) \
                $(LDDIR)/eagle.rom.addr.v6.ld

#############################################################
# Configuration i.e. compile options etc.
# Target specific stuff (defines etc.) goes in here!
# Generally values applying to a tree are captured in the
#   makefile at its root level - these are then overridden
#   for a subtree within the makefile rooted therein
#

#UNIVERSAL_TARGET_DEFINES =		\

# Other potential configuration flags include:
#	-DTXRX_TXBUF_DEBUG
#	-DTXRX_RXBUF_DEBUG
#	-DWLAN_CONFIG_CCX
CONFIGURATION_DEFINES =	-D__ets__ \
			-DICACHE_FLASH	\
			-DLWIP_OPEN_SRC	\
			-DPBUF_RSV_FOR_WLAN	\
			-DEBUF_LWIP

DEFINES +=				\
	$(UNIVERSAL_TARGET_DEFINES)	\
	$(CONFIGURATION_DEFINES)

DDEFINES +=				\
	$(UNIVERSAL_TARGET_DEFINES)	\
	$(CONFIGURATION_DEFINES)


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

INCLUDES := $(INCLUDES) -I $(PDIR)include
INCLUDES += -I ./
PDIR := ../$(PDIR)
sinclude $(PDIR)Makefile


.PHONY: FORCE
FORCE:

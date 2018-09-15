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
OPT_MKTARGETS        := coap crypto dht http mqtt pcm sjson sqlite3 tsl2561 websocket
OPT_MKLIBTARGETS     := u8g2 ucg
SEL_MKTARGETS        := $(shell $(CC) -E -dM include/user_modules.h | sed -n '/^\#define LUA_USE_MODULES_/{s/.\{24\}\(.*\)/\L\1/; p}')
OPT_SEL_MKLIBTARGETS := $(foreach tgt,$(OPT_MKLIBTARGETS),$(findstring $(tgt), $(SEL_MKTARGETS)))
OPT_SEL_MKTARGETS    := $(foreach tgt,$(OPT_MKTARGETS),$(findstring $(tgt), $(SEL_MKTARGETS))) \
                        $(foreach tgt,$(OPT_SEL_MKLIBTARGETS),$(tgt)lib)
OPT_SEL_COMPONENTS   := $(foreach tgt,$(OPT_SEL_MKTARGETS),$(tgt)/lib$(tgt).a)
SPECIAL_MKTARGETS    :=$(APP_MKTARGETS)

SUBDIRS= 					\
	user 					\
	driver 					\
	mbedtls 				\
	platform 				\
	libc 					\
	lua 					\
	lwip 					\
	task 					\
	smart 					\
	modules 				\
	spiffs 					\
	net					\
	fatfs					\
	esp-gdbstub				\
	pm					\
	$(OPT_SEL_MKTARGETS)

endif # } PDIR

APPDIR = .
LDDIR = ../ld

TARGET_LDFLAGS = 				\
	-nostdlib 				\
	-Wl,-EL 				\
	--longcalls 				\
	--text-section-literals

LD_FILE = $(LDDIR)/nodemcu.ld

COMPONENTS_eagle.app.v6 = 			\
	user/libuser.a 				\
	driver/libdriver.a 			\
	platform/libplatform.a 			\
	task/libtask.a 				\
	libc/liblibc.a 				\
	lua/liblua.a 				\
	lwip/liblwip.a 				\
	smart/smart.a 				\
	spiffs/spiffs.a 			\
	fatfs/libfatfs.a			\
	pm/libpm.a				\
	esp-gdbstub/libgdbstub.a		\
	net/libnodemcu_net.a			\
	mbedtls/libmbedtls.a      		\
	modules/libmodules.a			\
	$(OPT_SEL_COMPONENTS)


# Inspect the modules library and work out which modules need to be linked.
# For each enabled module, a symbol name of the form XYZ_module_selected is
# returned. At link time those names are declared undefined, so those (and
# only those) modules are pulled in.
SELECTED_MODULE_SYMS=$(filter %_module_selected %module_selected1,$(shell $(NM) modules/.output/$(TARGET)/$(FLAVOR)/lib/libmodules.a))

LINKFLAGS_eagle.app.v6 = 			\
	-Wl,--gc-sections 			\
	-Wl,-Map=mapfile 			\
	-nostdlib 				\
	-T$(LD_FILE) 				\
	-Wl,@../ld/defsym.rom			\
	-Wl,--no-check-sections 		\
	-Wl,--wrap=_xtos_set_exception_handler	\
	-Wl,-static 				\
	$(addprefix -u , $(SELECTED_MODULE_SYMS)) \
	-Wl,--start-group 			\
	-lmain 					\
	-lc 					\
	-lgcc 					\
	-lhal 					\
	-lphy 					\
	-lpp 					\
	-lnet80211 				\
	-lwpa 					\
	-lwpa2					\
	-lsmartconfig 				\
	-lcrypto				\
	-lwps					\
	$(DEP_LIBS_eagle.app.v6) 		\
	-Wl,--end-group 			\
	-lm

DEPENDS_eagle.app.v6 = 				\
                $(LD_FILE) 			\
		Makefile

#############################################################
# Configuration i.e. compile options etc.
# Target specific stuff (defines etc.) goes in here!
# Generally values applying to a tree are captured in the
#   makefile at its root level - these are then overridden
#   for a subtree within the makefile rooted therein
#

#UNIVERSAL_TARGET_DEFINES = 			\

# Other potential configuration flags include:
#	-DTXRX_TXBUF_DEBUG
#	-DTXRX_RXBUF_DEBUG
#	-DWLAN_CONFIG_CCX
CONFIGURATION_DEFINES =	-D__ets__ 		\
			-DICACHE_FLASH 		\
			-DLUA_OPTIMIZE_MEMORY=2	\
			-DMIN_OPT_LEVEL=2	\
			-DLWIP_OPEN_SRC 	\
			-DPBUF_RSV_FOR_WLAN 	\
			-DEBUF_LWIP		\
			-DUSE_OPTIMIZE_PRINTF	\
			-DMBEDTLS_USER_CONFIG_FILE=\"user_mbedtls.h\"  \

DEFINES += 					\
	$(UNIVERSAL_TARGET_DEFINES) 		\
	$(CONFIGURATION_DEFINES)

DDEFINES += 					\
	$(UNIVERSAL_TARGET_DEFINES) 		\
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

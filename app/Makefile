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

# Handle Lua Directory selector
ifeq ("$(LUA)","")
  LUA_DIR := lua
else ifeq ("$(LUA)","51")
  LUA_DIR := lua
else ifeq ("$(LUA)","53")
  LUA_DIR := lua53
else
  $(error Unsupported value "$(LUA)" for variable "LUA", \
    expected "51", "53" or unset/empty)
endif

ifndef PDIR # {
GEN_IMAGES= eagle.app.v6.out
GEN_BINS= eagle.app.v6.bin
OPT_MKTARGETS        := coap dht http mqtt pcm sjson tsl2561 websocket
OPT_MKLIBTARGETS     := u8g2 ucg
SEL_MKTARGETS        := $(shell $(CC) -E -dM include/user_modules.h | sed -n '/^\#define LUA_USE_MODULES_/{s/.\{24\}\(.*\)/\L\1/; p}')
OPT_SEL_MKLIBTARGETS := $(foreach tgt,$(OPT_MKLIBTARGETS),$(findstring $(tgt), $(SEL_MKTARGETS)))
OPT_SEL_MKTARGETS    := $(foreach tgt,$(OPT_MKTARGETS),$(findstring $(tgt), $(SEL_MKTARGETS))) \
                        $(foreach tgt,$(OPT_SEL_MKLIBTARGETS),$(tgt)lib)
OPT_SEL_COMPONENTS   := $(foreach tgt,$(OPT_SEL_MKTARGETS),$(tgt)/lib$(tgt).a)
SPECIAL_MKTARGETS    :=$(APP_MKTARGETS)

SUBDIRS= 					\
	user 					\
	crypto					\
	driver 					\
	mbedtls 				\
	platform 				\
	libc 					\
	$(LUA_DIR)				\
	lwip 					\
	smart 					\
	modules 				\
	spiffs 					\
	net					\
	fatfs					\
	esp-gdbstub				\
	pm					\
	uzlib					\
	$(OPT_SEL_MKTARGETS)

endif # } PDIR

APPDIR = .
LDDIR = ../ld

TARGET_LDFLAGS = 			\
	-nostdlib 				\
	-Wl,-EL 				\
	--longcalls 			\
	--text-section-literals

LD_FILE = $(LDDIR)/nodemcu.ld

COMPONENTS_eagle.app.v6 = 		\
	user/libuser.a 				\
	crypto/libcrypto.a			\
	driver/libdriver.a 			\
	platform/libplatform.a 		\
	libc/liblibc.a 				\
	$(LUA_DIR)/liblua.a 		\
	lwip/liblwip.a 				\
	smart/smart.a 				\
	spiffs/spiffs.a 			\
	fatfs/libfatfs.a			\
	pm/libpm.a					\
	esp-gdbstub/libgdbstub.a	\
	net/libnodemcu_net.a		\
	mbedtls/libmbedtls.a    	\
	modules/libmodules.a		\
	smart/smart.a 				\
	uzlib/libuzlib.a 			\
	$(OPT_SEL_COMPONENTS)

# Inspect the modules library and work out which modules need to be linked.
# For each enabled module, a symbol name of the form XYZ_module_selected is
# returned. At link time those names are declared undefined, so those (and
# only those) modules are pulled in.
SELECTED_MODULE_SYMS=$(filter %_module_selected %module_selected1,$(shell $(NM) modules/.output/$(TARGET)/$(FLAVOR)/lib/libmodules.a))

LINKFLAGS_eagle.app.v6 = 	\
	-Wl,--gc-sections 		\
	-Wl,-Map=mapfile 		\
	-nostdlib 				\
	-T$(LD_FILE) 			\
	-Wl,@../ld/defsym.rom	\
	-Wl,--no-check-sections	\
	-Wl,-static 			\
	$(addprefix -u , $(SELECTED_MODULE_SYMS)) \
	-Wl,--start-group 		\
	-lmain 					\
	$(DEP_LIBS_eagle.app.v6)\
	-Wl,--end-group 		\
	-Wl,--start-group 		\
	-lgcc 					\
	-lhal 					\
	-lphy 					\
	-lpp 					\
	-lnet80211 				\
	-lsmartconfig 			\
	-lwpa 					\
	-lwpa2					\
	-lcrypto				\
	-lwps					\
	-lc 					\
	-lm						\
	-Wl,--end-group
#	-Wl,--cref
#	-Wl,--wrap=_xtos_set_exception_handler

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

CONFIGURATION_DEFINES += -DLWIP_OPEN_SRC

DEFINES += 							\
	$(UNIVERSAL_TARGET_DEFINES) 	\
	$(CONFIGURATION_DEFINES)

DDEFINES += 						\
	$(UNIVERSAL_TARGET_DEFINES) 	\
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
INCLUDES := -I $(PDIR)libc -I $(PDIR)$(LUA_DIR) -I $(PDIR)platform \
            $(INCLUDES) -I $(PDIR) -I $(PDIR)include
PDIR := ../$(PDIR)

sinclude $(PDIR)Makefile

.PHONY: FORCE
FORCE:

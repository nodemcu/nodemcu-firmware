#
# This Makefile is called from the core Makefile hierarchy which is a hierarchical
# make which uses parent callbacks to implement inheritance.  However if luac_cross
# build stands outside this, it uses the host toolchain to implement a separate
# host build of the luac.cross image.
#
.NOTPARALLEL:

CCFLAGS:= -I. -I.. -I../../include -I../../uzlib
LDFLAGS:= -L$(SDK_DIR)/lib -L$(SDK_DIR)/ld -lm -ldl -Wl,-Map=mapfile

CCFLAGS += -Wall

TARGET = host

VERBOSE ?=
V ?= $(VERBOSE)
ifeq ("$(V)","1")
  export summary := @true
else
  export summary := @echo
  # disable echoing of commands, directory names
  # MAKEFLAGS += --silent -w
endif  # $(V)==1


# Validate LUA setting
ifeq ("$(LUA)","53")
  # ok
else ifeq ("$(LUA)","51")
  $(error Your variable LUA="$(LUA)" looks like you probably want \
    app/lua/luac_cross/Makefile instead)
else
  $(error Expected environment variable "LUA" to be "53", not "$(LUA)")
endif


DEBUG ?=
ifeq ("$(DEBUG)","1")
    FLAVOR         =   debug
    CCFLAGS        += -O0 -ggdb
    TARGET_LDFLAGS += -O0 -ggdb
    DEFINES        += -DLUA_DEBUG_BUILD -DDEVELOPMENT_TOOLS -DDEVELOPMENT_USE_GDB
else
    FLAVOR         =  release
    CCFLAGS        += -O2
    TARGET_LDFLAGS += -O2
endif  # DEBUG

LUACSRC := luac.c      liolib.c    loslib.c
LUASRC  := lapi.c      lauxlib.c   lbaselib.c  lcode.c     lcorolib.c  lctype.c \
           ldblib.c    ldebug.c    ldo.c       ldump.c     lfunc.c     lgc.c \
           linit.c     llex.c      lmathlib.c  lmem.c      loadlib.c   lnodemcu.c \
           lobject.c   lopcodes.c  lparser.c   lstate.c    lstring.c   lstrlib.c \
           ltable.c    ltablib.c   ltm.c       lundump.c   lutf8lib.c  lvm.c \
           lzio.c
UZSRC   := uzlib_deflate.c crc32.c

TEST ?=
ifeq ("$(TEST)","1")
   DEFINES  += -DLUA_ENABLE_TEST
   LUACSRC  += ltests.c
endif  # $(TEST)==1

#
# This relies on the files being unique on the vpath
#
SRC      := $(LUACSRC) $(LUASRC) $(UZSRC)
vpath %.c .:..:../../libc:../../uzlib

ODIR   := .output/$(TARGET)/$(FLAVOR)/obj

OBJS   := $(SRC:%.c=$(ODIR)/%.o)
DEPS   := $(SRC:%.c=$(ODIR)/%.d)

CFLAGS = $(CCFLAGS) $(DEFINES)  $(EXTRA_CCFLAGS) $(STD_CFLAGS) $(INCLUDES)
DFLAGS = $(CCFLAGS) $(DDEFINES) $(EXTRA_CCFLAGS) $(STD_CFLAGS) $(INCLUDES)

CC := $(WRAPCC) gcc

ECHO := echo

BUILD_TYPE := $(shell $(CC) $(EXTRA_CCFLAGS) -E -dM - <../../include/user_config.h | grep LUA_NUMBER_INTEGRAL | wc -l)
ifeq ($(BUILD_TYPE),0)
IMAGE := ../../../luac.cross
else
IMAGE := ../../../luac.cross.int
endif

.PHONY: test clean all

all: $(DEPS) $(IMAGE)

$(IMAGE) : $(OBJS)
	$(summary) HOSTLD $@
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

test :
	@echo CC: $(CC)
	@echo SRC: $(SRC)
	@echo OBJS: $(OBJS)
	@echo DEPS: $(DEPS)
	@echo IMAGE: $(IMAGE)

clean :
	$(RM) -r $(ODIR)

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif

$(ODIR)/%.o: %.c
	@mkdir -p $(ODIR);
	$(summary) HOSTCC $(CURDIR)/$<
	$(CC) $(if $(findstring $<,$(DSRCS)),$(DFLAGS),$(CFLAGS)) $(COPTS_$(*F)) -o $@ -c $<

$(ODIR)/%.d: %.c
	@mkdir -p $(ODIR);
	$(summary) DEPEND: HOSTCC $(CURDIR)/$<
	@set -e; rm -f $@; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\.o\)[ :]*,$(ODIR)/\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

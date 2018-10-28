# 
# This Make file is called from the core Makefile hierarchy with is a hierarchical
# make wwhich uses parent callbacks to implement inheritance.  However is luac_cross
# build stands outside this and uses the host toolchain to implement a separate
# host build of the luac.cross image. 
#
.NOTPARALLEL:

CCFLAGS:= -I.. -I../../include -I../../libc -I../../uzlib
LDFLAGS:= -L$(SDK_DIR)/lib -L$(SDK_DIR)/ld -lm -ldl -Wl,-Map=mapfile

CCFLAGS += -Wall

DEFINES += -DLUA_CROSS_COMPILER -DLUA_OPTIMIZE_MEMORY=2

TARGET = host

ifeq ($(FLAVOR),debug)
    CCFLAGS        += -O0 -g
    TARGET_LDFLAGS += -O0 -g
    DEFINES        += -DLUA_DEBUG_BUILD
else
    FLAVOR         =  release
    CCFLAGS        += -O2
    TARGET_LDFLAGS += -O2
endif

LUACSRC := luac.c      lflashimg.c liolib.c    loslib.c    print.c
LUASRC  := lapi.c      lauxlib.c   lbaselib.c  lcode.c     ldblib.c    ldebug.c \
           ldo.c       ldump.c     lfunc.c     lgc.c       linit.c     llex.c \
           lmathlib.c  lmem.c      loadlib.c   lobject.c   lopcodes.c  lparser.c \
           lrotable.c  lstate.c    lstring.c   lstrlib.c   ltable.c    ltablib.c \
           ltm.c       lundump.c   lvm.c       lzio.c
LIBCSRC := c_stdlib.c
UZSRC   := uzlib_deflate.c crc32.c

#
# This relies on the files being unique on the vpath
#
SRC      := $(LUACSRC) $(LUASRC) $(LIBCSRC) $(UZSRC)
vpath %.c .:..:../../libc:../../uzlib

ODIR   := .output/$(TARGET)/$(FLAVOR)/obj

OBJS   := $(SRC:%.c=$(ODIR)/%.o)
DEPS   := $(SRC:%.c=$(ODIR)/%.d)

CFLAGS = $(CCFLAGS) $(DEFINES)  $(EXTRA_CCFLAGS) $(STD_CFLAGS) $(INCLUDES)
DFLAGS = $(CCFLAGS) $(DDEFINES) $(EXTRA_CCFLAGS) $(STD_CFLAGS) $(INCLUDES)

CC := $(WRAPCC) gcc

ECHO := echo

IMAGE  := ../../../luac.cross

.PHONY: test clean all

all: $(DEPS) $(IMAGE)

$(IMAGE) : $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

test :
	@echo CC: $(CC)
	@echo SRC: $(SRC)
	@echo OBJS: $(OBJS)
	@echo DEPS: $(DEPS)

clean :
	$(RM) -r $(ODIR)

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif

$(ODIR)/%.o: %.c
	@mkdir -p $(ODIR);
	$(CC) $(if $(findstring $<,$(DSRCS)),$(DFLAGS),$(CFLAGS)) $(COPTS_$(*F)) -o $@ -c $<

$(ODIR)/%.d: %.c
	@mkdir -p $(ODIR);
	@echo DEPEND: $(CC) -M $(CFLAGS) $<
	@set -e; rm -f $@; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\.o\)[ :]*,$(ODIR)/\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

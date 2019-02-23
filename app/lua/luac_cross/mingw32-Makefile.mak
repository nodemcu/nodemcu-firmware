#
# This is a minimal Make file designed to be called from within a MinGW Cmd prompt.
# So if the distro ZIP file has been unpacked into C:\nodemcu-firmware then 
#
# C:\nodemcu-firmware\app\lua\luac_cross> mingw32-make -f mingw32-makefile.mak
#
# will create the WinX EXE luac.cross.exe within the root C:\nodemcu-firmware folder.
# This make has been stripped down to use the basic non-graphics MinGW32 install and 
# standard Windows commands available at the Cmd> prompt.  This make is quite separate
# from the normal toolchain build.

.NOTPARALLEL:

CCFLAGS:= -I.. -I../../include -I../../libc -I../../uzlib -Wall
LDFLAGS:= -lm -Wl,-Map=mapfile
DEFINES += -DLUA_CROSS_COMPILER -DLUA_OPTIMIZE_MEMORY=2

CFLAGS  = $(CCFLAGS) $(DEFINES)  $(EXTRA_CCFLAGS) $(STD_CFLAGS) $(INCLUDES)

TARGET = host
CC     := gcc

ifeq ($(FLAVOR),debug)
    CCFLAGS        += -O0 -g
    TARGET_LDFLAGS += -O0 -g
    DEFINES        += -DLUA_DEBUG_BUILD
else
    FLAVOR         =  release
    CCFLAGS        += -O2
    TARGET_LDFLAGS += -O2
endif
#
# C files needed to compile luac.cross
#
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

INCS   := -I.. -I../.. -I../../libc -I../../uzlib
ODIR   := .output\obj
OBJS   := $(SRC:%.c=$(ODIR)/%.o)
IMAGE  := ../../../luac.cross.exe

.PHONY: test clean all

all: $(DEPS) $(IMAGE)

$(IMAGE) : $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

test :
	@echo CC: $(CC)
	@echo SRC: $(SRC)
	@echo OBJS: $(OBJS)

clean :
	del /s /q $(ODIR)

$(ODIR)/%.o: %.c
	@mkdir $(ODIR) || echo .
	$(CC) $(INCS)  $(CFLAGS) -o $@ -c $<

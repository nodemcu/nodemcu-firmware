all: build

ifeq ($V,)
 Q:=@
endif

LUAC_CFLAGS:= -I../uzlib -I../lua -I$(BUILD_DIR_BASE)/include -I../base_nodemcu/include -O2 -g -Wall -Wextra
LUAC_LDFLAGS:= -ldl -lm

LUAC_DEFINES += -DLUA_CROSS_COMPILER -DLUA_USE_STDIO

vpath %.c ../lua ../uzlib ../base_nodemcu

LUAC_LUACSRC:= \
	luac.c      lflashimg.c loslib.c    print.c     liolib.c

LUAC_LUASRC:= $(addprefix ../lua/, \
	lapi.c      lauxlib.c   lbaselib.c  lcode.c     ldblib.c    ldebug.c \
    ldo.c       ldump.c     lfunc.c     lgc.c       llex.c \
	lmathlib.c  lmem.c      loadlib.c   lobject.c   lopcodes.c  lparser.c \
    lrotable.c  lstate.c    lstring.c   lstrlib.c   ltable.c    ltablib.c \
    ltm.c       lundump.c   lvm.c       lzio.c \
)

LUAC_UZSRC:= $(addprefix ../uzlib/, \
	uzlib_deflate.c crc32.c \
)

LUAC_NODEMCUSRC:= $(addprefix ../base_nodemcu/, \
	linit.c \
)

LUAC_BUILD_DIR:=$(BUILD_DIR_BASE)/luac_cross

LUAC_OBJS:=$(LUAC_LUACSRC:%.c=$(LUAC_BUILD_DIR)/%.o)
LUAC_OBJS+=$(LUAC_LUASRC:../lua/%.c=$(LUAC_BUILD_DIR)/%.o)
LUAC_OBJS+=$(LUAC_UZSRC:../uzlib/%.c=$(LUAC_BUILD_DIR)/%.o)
LUAC_OBJS+=$(LUAC_NODEMCUSRC:../base_nodemcu/%.c=$(LUAC_BUILD_DIR)/%.o)


LUAC_DEPS:=$(LUAC_OBJS:%.o=%.d)

LUAC_CROSS:=$(LUAC_BUILD_DIR)/luac.cross

$(LUAC_BUILD_DIR):
	@mkdir -p "$@"

$(LUAC_BUILD_DIR)/%.o: | $(LUAC_BUILD_DIR)
	@echo '[hostcc] $(notdir $@)'
	$Q$(HOSTCC) $(LUAC_DEFINES) $(LUAC_CFLAGS) "$<" -c -o "$@"

$(LUAC_BUILD_DIR)/%.d: SHELL=/bin/bash
$(LUAC_BUILD_DIR)/%.d: %.c | $(LUAC_BUILD_DIR)
	@echo '[  dep] $<'
	@rm -f "$@"
	$Qset -eo pipefail; $(HOSTCC) $(LUAC_DEFINES) $(LUAC_CFLAGS) -M "$<" | sed 's,\($*\.o\)[ :]*,$(LUAC_BUILD_DIR)/\1 $@ : ,g' > "$@.tmp"; mv "$@.tmp" "$@"

build: $(LUAC_DEPS) $(LUAC_CROSS)

$(LUAC_CROSS): $(LUAC_OBJS)
	@echo '[ link] $(notdir $@)'
	$Q$(HOSTCC) $(LUAC_CFLAGS) $^ $(LUAC_LDFLAGS) -o "$@"

ifneq ($(MAKECMDGOALS),clean)
-include $(LUAC_DEPS)
endif

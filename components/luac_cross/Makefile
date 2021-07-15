all: build

HOSTCC?=$(PYTHON) -m ziglang cc

# zig cc (0.8.0 at least) seems to get itself all confused with its cache
# when we're running a separate path for dependencies, so we skip them for
# now as for most people they're not needed anyway.
ifeq ($(findstring zig,$(HOSTCC)),zig)
  WITHOUT_DEPS:=1
endif

ifeq ($V,)
  Q:=@
endif

LUAC_CFLAGS:= -I$(COMPONENT_PATH)/../uzlib -I$(COMPONENT_PATH)/../lua -I$(BUILD_DIR_BASE)/config -I$(COMPONENT_PATH)/../base_nodemcu/include -O2 -g -Wall -Wextra
LUAC_LDFLAGS:= -ldl -lm

LUAC_DEFINES += -DLUA_CROSS_COMPILER -DLUA_USE_STDIO
ifneq ($(CONFIG_LUA_OPTIMIZE_DEBUG),)
LUAC_DEFINES += -DLUA_OPTIMIZE_DEBUG=$(CONFIG_LUA_OPTIMIZE_DEBUG)
endif

vpath %.c $(COMPONENT_PATH) $(COMPONENT_PATH)/../lua $(COMPONENT_PATH)/../uzlib $(COMPONENT_PATH)/../base_nodemcu

LUAC_LUACSRC:= \
	luac.c      lflashimg.c loslib.c    print.c     liolib.c

LUAC_LUASRC:= $(addprefix $(COMPONENT_PATH)/../lua/, \
	lapi.c      lauxlib.c   lbaselib.c  lcode.c     ldblib.c    ldebug.c \
    ldo.c       ldump.c     lfunc.c     lgc.c       llex.c \
	lmathlib.c  lmem.c      loadlib.c   lobject.c   lopcodes.c  lparser.c \
    lrotable.c  lstate.c    lstring.c   lstrlib.c   ltable.c    ltablib.c \
    ltm.c       lundump.c   lvm.c       lzio.c \
)

LUAC_UZSRC:= $(addprefix $(COMPONENT_PATH)/../uzlib/, \
	uzlib_deflate.c crc32.c \
)

LUAC_NODEMCUSRC:= $(addprefix $(COMPONENT_PATH)/../base_nodemcu/, \
	linit.c \
)

LUAC_BUILD_DIR:=$(BUILD_DIR_BASE)/luac_cross

LUAC_OBJS:=$(LUAC_LUACSRC:%.c=$(LUAC_BUILD_DIR)/%.o)
LUAC_OBJS+=$(LUAC_LUASRC:$(COMPONENT_PATH)/../lua/%.c=$(LUAC_BUILD_DIR)/%.o)
LUAC_OBJS+=$(LUAC_UZSRC:$(COMPONENT_PATH)/../uzlib/%.c=$(LUAC_BUILD_DIR)/%.o)
LUAC_OBJS+=$(LUAC_NODEMCUSRC:$(COMPONENT_PATH)/../base_nodemcu/%.c=$(LUAC_BUILD_DIR)/%.o)


ifneq ($(WITHOUT_DEPS),1)
LUAC_DEPS:=$(LUAC_OBJS:%.o=%.d)
endif

LUAC_CROSS:=$(LUAC_BUILD_DIR)/luac.cross

$(LUAC_BUILD_DIR):
	@mkdir -p "$@"

$(LUAC_BUILD_DIR)/%.o: %.c | $(LUAC_BUILD_DIR)
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

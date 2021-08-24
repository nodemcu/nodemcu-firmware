all: build

HOSTCC?=$(PYTHON) -m ziglang cc

ifeq ($V,)
  Q:=@
endif

LUAC_OBJ_DIR:=$(LUAC_BUILD_DIR)/$(notdir $(LUA_PATH))

LUAC_CFLAGS:= \
  -I$(LUAC_BUILD_DIR) \
  -I$(LUA_PATH) \
  -I$(LUA_PATH)/host \
  -I$(LUA_PATH)/../common \
  -I$(LUA_PATH)/../../uzlib \
  -O2 -g -Wall -Wextra -Wno-sign-compare

LUAC_LDFLAGS:= -ldl -lm

LUAC_DEFINES += \
	-DLUA_CROSS_COMPILER \
	-DLUA_USE_HOST \
	-DLUA_USE_STDIO \

vpath %.c $(LUA_PATH)/host $(LUA_PATH) $(LUA_PATH)/../common $(LUA_PATH)/../../uzlib

LUA_SRCS:=\
	luac.c      lflashimg.c loslib.c    print.c     liolib.c \
	lapi.c      lauxlib.c   lbaselib.c  lcode.c     ldblib.c    ldebug.c \
	ldo.c       ldump.c     lfunc.c     lgc.c       llex.c \
	lmathlib.c  lmem.c      loadlib.c   lobject.c   lopcodes.c  lparser.c \
	lnodemcu.c  lstate.c    lstring.c   lstrlib.c   ltable.c    ltablib.c \
	ltm.c       lundump.c   lvm.c       lzio.c \
	linit.c		lpanic.c \
	uzlib_deflate.c crc32.c \

LUAC_OBJS:=$(LUA_SRCS:%.c=$(LUAC_OBJ_DIR)/%.o)
LUAC_CROSS:=$(LUAC_BUILD_DIR)/luac.cross

$(LUAC_OBJ_DIR):
	@mkdir -p "$@"

$(LUAC_OBJ_DIR)/%.o: %.c | $(LUAC_OBJ_DIR)
	@echo '[hostcc] $(notdir $@)'
	$Q$(HOSTCC) $(LUAC_DEFINES) $(LUAC_CFLAGS) "$<" -c -o "$@"

$(LUAC_OBJ_DIR)/%.d: SHELL=/bin/bash
$(LUAC_OBJ_DIR)/%.d: %.c | $(LUAC_OBJ_DIR)
	@echo '[  dep] $<'
	@rm -f "$@"
	$Qset -eo pipefail; $(HOSTCC) $(LUAC_DEFINES) $(LUAC_CFLAGS) -M "$<" | sed 's,\($*\.o\)[ :]*,$(LUAC_OBJ_DIR)/\1 $@ : ,g' > "$@.tmp"; mv "$@.tmp" "$@"

build: $(LUAC_DEPS) $(LUAC_CROSS)

$(LUAC_CROSS): $(LUAC_OBJS)
	@echo '[ link] $(notdir $@)'
	$Q$(HOSTCC) $(LUAC_CFLAGS) $^ $(LUAC_LDFLAGS) -o "$@"

# zig cc (0.8.0 at least) seems to get itself all confused with its cache
# when we're running a separate path for dependencies, so we skip them for
# now as for most people they're not needed anyway.
ifneq ($(findstring zig,$(HOSTCC)),zig)
  LUAC_DEPS:=$(LUAC_OBJS:%.o=%.d)
  ifneq ($(MAKECMDGOALS),clean)
	-include $(LUAC_DEPS)
  endif
endif

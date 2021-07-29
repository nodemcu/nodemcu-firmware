idf_component_register(
  SRCS "lapi.c" "lauxlib.c" "lbaselib.c" "lcode.c" "ldblib.c" "ldebug.c"
  "ldo.c" "ldump.c" "legc.c" "lflash.c" "lfunc.c" "lgc.c" "llex.c" "lmathlib.c"
  "lmem.c" "loadlib.c" "lobject.c" "lopcodes.c" "lparser.c" "lrotable.c"
  "lstate.c" "lstring.c" "lstrlib.c" "ltable.c" "ltablib.c" "ltm.c"
  "lua.c" "lundump.c" "lvm.c" "lzio.c"
  INCLUDE_DIRS "."
  REQUIRES "platform" "uzlib" "driver_console"
  PRIV_REQUIRES "base_nodemcu" "embedded_lfs"
)

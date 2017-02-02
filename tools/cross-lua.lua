local args = { ... }
local b = require "tools.build"
local builder = b.new_builder( ".build/cross-lua" )
local utils = b.utils
local sf = string.format
 
if not (_VERSION == "Lua 5.1" and pcall(require,"lfs")) then
  print  [[

cross_lua.lua must be run within Lua 5.1 and it requires the Lua Filesystem to be installed. 
On most *nix distrubitions youwill find a packages lua-5.1 and lua-filesystem, or 
alternalively you can install lua-rocks and use the Rocks package manager to install lfs.
]]
  os.exit(1)
end
builder:init( args )
builder:set_build_mode( builder.BUILD_DIR_LINEARIZED )
local output = 'luac.cross'
local cdefs = '-DLUA_CROSS_COMPILER -Ddbg_printf=printf'

-- Lua source files and include path
local lua_files = [[
    lapi.c lauxlib.c lbaselib.c lcode.c ldblib.c ldebug.c ldo.c ldump.c 
    lfunc.c lgc.c llex.c lmathlib.c lmem.c loadlib.c lobject.c lopcodes.c  
    lparser.c lrotable.c lstate.c lstring.c lstrlib.c ltable.c ltablib.c 
    ltm.c  lundump.c lvm.c lzio.c 
    luac_cross/luac.c luac_cross/loslib.c luac_cross/print.c
    ../modules/linit.c
    ../libc/c_stdlib.c
  ]]
lua_files = lua_files:gsub( "\n" , "" )
local lua_full_files = utils.prepend_path( lua_files, "app/lua" )
local local_include = "-Iapp/include -Iinclude -Iapp/lua"

-- Compiler/linker options
builder:set_compile_cmd( sf( "gcc -O2 %s -Wall %s -c $(FIRST) -o $(TARGET)", local_include, cdefs ) )
builder:set_link_cmd( "gcc -o $(TARGET) $(DEPENDS) -lm" )

-- Build everything
builder:make_exe_target( output, lua_full_files )
builder:build()


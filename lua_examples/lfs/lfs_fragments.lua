-- First time image boot to discover the confuration
--
-- If you want to use absolute address LFS load or SPIFFS imaging, then boot the
-- image for the first time bare, that is without either LFS or SPIFFS preloaded
-- then enter the following commands interactively through the UART:
--
local _,mapa,fa=node.flashindex(); return ('0x%x, 0x%x, 0x%x'):format(
  mapa,fa,file.fscfg())
--
-- This will print out 3 hex constants: the absolute address used in the 
-- 'luac.cross -a' options and the flash adresses of the LFS and SPIFFS.
--
--[[  So you would need these commands to image your ESP module:
USB=/dev/ttyUSB0         # or whatever the device of your USB is
NODEMCU=~/nodemcu        # The root of your NodeMCU file hierarchy
SRC=$NODEMCU/local/lua   # your source directory for your LFS Lua files.
BIN=$NODEMCU/bin
ESPTOOL=$NODEMCU/tools/esptool.py

$ESPTOOL --port $USB erase_flash   # Do this is you are having load funnies
$ESPTOOL --port $USB --baud 460800  write_flash -fm dio 0x00000 \
  $BIN/0x00000.bin 0x10000 $BIN/0x10000.bin
#
# Now restart your module and use whatever your intective tool is to do the above 
# cmds, so if this outputs 0x4027b000, -0x7b000, 0x100000 then you can do
#
$NODEMCU/luac.cross -a 0x4027b000 -o $BIN/0x7b000-flash.img $SRC/*.lua
$ESPTOOL --port $USB --baud 460800  write_flash -fm dio 0x7b000 \
                                                   $BIN/0x7b000-flash.img
# and if you've setup a SPIFFS then
$ESPTOOL --port $USB --baud 460800  write_flash -fm dio 0x100000 \
                                                   $BIN/0x100000-0x10000.img
# and now you are good to go
]]

-----------------------------------------------------------------------------------
--
-- It is a good idea to add an _init.lua module to your LFS and do most of the 
-- LFS module related initialisaion in this.  This example uses standard Lua
-- features to simplify the LFS API.  
--
-- The first adds a 'LFS' table to _G and uses the __index metamethod to resolve
-- functions in the LFS, so you can execute the main function of module 'fred'
-- by doing LFS.fred(params)
--
-- The second adds the LFS to the require searchlist so that you can require a
-- Lua module 'jean' in the LFS by simply doing require "jean".  However not that
-- this is at the search entry following the FS searcher, so if you also have
-- jean.lc or jean.lua in SPIFFS, then this will get preferentially loaded,
-- albeit into RAM.  (Useful, for development).
--
do
  local index = node.flashindex 
  local lfs_t = { __index = function(_, name)
      local fn, ba = index(name)
      if not ba then return fn end -- or return nil implied
    end}
  getfenv().LFS = setmetatable(lfs_t,lfs_t)

  local function loader_flash(module)
    local fn, ba = index(module)
    return ba and "Module not in LFS" or fn 
  end
  package.loaders[3] = loader_flash

end

-----------------------------------------------------------------------------------
--
-- File: init.lua
--
-- With the previous example you still need an init.lua to bootstrap the _init 
-- module in LFS.  Here is an example.  It's a good idea either to use a timer 
-- delay or a GPIO pin during development, so that you as developer can break into 
-- the boot sequence if there is a problem with the _init bootstrap that is causing
-- a panic loop.  Here is one example of how you might do this.  You have a second to
-- inject tmr.stop(0) into UART0.  Extend if your reactions can't meet this.
--
-- You also want to do autoload the LFS, for example by adding the following:
--
if node.flashindex() == nil then 
  node.flashreload('flash.img') 
end

tmr.alarm(0, 1000, tmr.ALARM_SINGLE, 
  function()
    local fi=node.flashindex; return pcall(fi and fi'_init')
  end)

-----------------------------------------------------------------------------------
--
-- The debug.getstrings function can be used to get a listing of the RAM (or ROM
-- if LFS is loaded), as per the following example, so you can do this at the 
-- interactive prompt or call it as a debug function during a running application. 
--
do
  local a=debug.getstrings'RAM'
  for i =1, #a do a[i] = ('%q'):format(a[i]) end
  print ('local preload='..table.concat(a,','))
end

-----------------------------------------------------------------------------------
--
-- File: LFS_dummy_strings.lua
--
-- luac.cross -f will generate a ROM string table that includes all strings 
-- referenced in the loaded modules.  If you want to preload other string constants
-- hen the trick is to include a dummy module in the LFS.  You never need to call
-- this.  It's inclusion is enough to add the strings to the ROM table.  Once in
-- the ROM table, then you can use them in your application without incuring any 
-- RAM or Lua Garbage Collector (LGC) overhead.  Here is a useful starting point,
-- but you can add to this for your application.  
--
-- The trick is to build the LFS as normal then run the previous example from your
-- running application and append these lines to this file.
--
local preload = "?.lc;?.lua", "@init.lua", "_G", "_LOADED", "_LOADLIB", "__add", 
"__call", "__concat", "__div", "__eq", "__gc", "__index", "__le", "__len", "__lt", 
"__mod", "__mode", "__mul", "__newindex", "__pow", "__sub", "__tostring", "__unm", 
"collectgarbage", "cpath", "debug", "file", "file.obj", "file.vol", "flash", 
"getstrings", "index", "ipairs", "list", "loaded", "loader", "loaders", "loadlib", 
"module", "net.tcpserver", "net.tcpsocket", "net.udpsocket", "newproxy", "package", 
"pairs", "path", "preload", "reload", "require", "seeall", "wdclr"


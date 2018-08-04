--
-- File: LFS_dummy_strings.lua
--[[
  luac.cross -f generates a ROM string table which is part of the compiled LFS
  image. This table includes all strings referenced in the loaded modules. 

  If you want to preload other string constants, then one way to achieve this is
  to include a dummy module in the LFS that references the strings that you want
  to load. You never need to call this module; it's inclusion in the LFS image is
  enough to add the strings to the ROM table. Your application can use any strings
  in the ROM table without incuring any RAM or Lua Garbage Collector (LGC)
  overhead. 

  The local preload example is a useful starting point. However, if you call the
  following code in your application during testing, then this will provide a 
  listing of the current RAM string table.  

do
  local a=debug.getstrings'RAM'
  for i =1, #a do a[i] = ('%q'):format(a[i]) end
  print ('local preload='..table.concat(a,','))
end

  This will exclude any strings already in the ROM table, so the output is the list
  of putative strings that you should consider adding to LFS ROM table.

---------------------------------------------------------------------------------]]

local preload = "?.lc;?.lua", "/\n;\n?\n!\n-", "@init.lua", "_G", "_LOADED",
"_LOADLIB", "__add", "__call", "__concat", "__div", "__eq", "__gc", "__index",
"__le", "__len", "__lt", "__mod", "__mode", "__mul", "__newindex", "__pow",
"__sub", "__tostring", "__unm", "collectgarbage", "cpath", "debug", "file",
"file.obj", "file.vol", "flash", "getstrings", "index", "ipairs", "list", "loaded",
"loader", "loaders", "loadlib", "module", "net.tcpserver", "net.tcpsocket",
"net.udpsocket", "newproxy", "package", "pairs", "path", "preload", "reload",
"require", "seeall", "wdclr", "not enough memory", "sjson.decoder","sjson.encoder", 
"tmr.timer"

--------------------------------------------------------------------------------
-- LuaOTA provisioning system for ESPs using NodeMCU Lua
-- LICENCE: http://opensource.org/licenses/MIT
-- TerryE  15 Jul 2017
--
-- See luaOTA.md for description
--------------------------------------------------------------------------------

--[[ luaOTAserver.lua - an example provisioning server 

  This module implements an example server-side implementation of LuaOTA provisioning 
  system for ESPs used the SPI Flash FS (SPIFFS) on development and production modules. 

  This implementation is a simple TCP listener which can have one active provisioning
  client executing the luaOTA module at a time.  It will synchronise the client's FS
  with the content of the given directory on the command line.

]]

local socket     = require "socket"
local lfs        = require "lfs"
local md5        = require "md5"
local json       = require "cjson"
require "etc.strict"  --  see http://www.lua.org/extras/5.1/strict.lua

-- Local functions (implementation see below) ------------------------------------------

local get_inventory     -- function(root_directory, CPU_ID)
local send_command      -- function(esp, resp, buffer)
local receive_and_parse -- function(esp)
local provision         -- function(esp, config, files, inventory, fingerprint)
local read_file         -- function(fname)
local save_file         -- function(fname, data)
local compress_lua      -- function(lua_file)
local hmac              -- function(data)

-- Function-wide locals (can be upvalues)
local unpack = table.unpack or unpack
local concat = table.concat
local load   = loadstring or load
local format = string.format
-- use string % operators as a synomyn for string.format
getmetatable("").__mod = 
   function(a, b)
      return not b and a or 
       (type(b) == "table" and format(a, unpack(b)) or format(a, b))
      end

local ESPport    = 8266
local ESPtimeout = 15

local src_dir    = arg[1] or "."

-- Main process ------------------------ do encapsulation to prevent name-clash upvalues
local function main ()
  local server     = assert(socket.bind("*", ESPport))
  local ip, port   = server:getsockname()

  print("Lua OTA service listening on  %s:%u\n After connecting, the ESP timeout is %u s" 
     % {ip, port, ESPtimeout})

  -- Main loop forever waiting for ESP clients then processing each request ------------


  while true do
    local esp = server:accept()           -- wait for ESP connection 
    esp:settimeout(ESPtimeout)            -- set session timeout
    -- receive the opening request 
    local config = receive_and_parse(esp)
    if config and config.a == "HI" then 
      print ("Processing provision check from ESP-"..config.id)
      local inventory, fingerprint = get_inventory(src_dir, config.id)
      -- Process the ESP request
      if config.chk and config.chk == fingerprint then
        send_command(esp, {r = "OK!"})       -- no update so send_command with OK
        esp:receive("*l")                    -- dummy receive to allow client to close
      else
        local status, msg = pcall(provision, esp, config, inventory, fingerprint)
        if not status then print (msg) end
  
    end
    end
    pcall(esp.close, esp)
    print ("Provisioning complete")
  end
end

-- Local Function Implementations ------------------------------------------------------

local function get_hmac_md5(key)
  if key:len() > 64 then 
    key = md5.sum(key) 
  elseif key:len() < 64 then
    key = key .. ('\0'):rep(64-key:len())
  end
  local ki = md5.exor(('\54'):rep(64),key)
  local ko = md5.exor(('\92'):rep(64),key)
  return function (data) return md5.sumhexa(ko..md5.sum(ki..data)) end
end

-- Enumerate the sources directory and load the relevent inventory
------------------------------------------------------------------
get_inventory = function(dir, cpuid)
  if (not dir or lfs.attributes(dir).mode ~= "directory") then
    error("Cannot open directory, aborting %s" % arg[0], 0)
  end

  -- Load the CPU's (or the default) inventory 
  local invtype, inventory = "custom", read_file("%s/ESP-%s.json" % {dir, cpuid})  
  if not inventory then
    invtype, inventory = "default", read_file(dir .. "/default.json")
  end

  -- tolerate and remove whitespace formatting, then decode
  inventory = (inventory or ""):gsub("[ \t]*\n[ \t]*","")
  inventory = inventory:gsub("[ \t]*:[ \t]*",":")
  local ok; ok,inventory = pcall(json.decode, inventory)
  if ok and inventory.files then
    print( "Loading %s inventory for ESP-%s" % {invtype, cpuid})
  else
    error( "Invalid inventory for %s :%s" % {cpuid,inventory}, 0)
  end

  -- Calculate the current fingerprint of the inventory
  local fp,f = {},inventory.files
  for i= 1,#f do
    local name, fullname = f[i], "%s/%s" % {dir, f[i]}
    local fa = lfs.attributes(fullname)

    assert(fa, "File %s is required but not in sources directory" % name)
    fp[#fp+1] = name .. ":" .. fa.modification
    f[i] = {name = name, mtime = fa.modification, 
            size = fa.size, content = read_file(fullname) }
    assert (f[i].size == #(f[i].content or ''), "File %s unreadable" % name )
  end

  assert(#f == #fp, "Aborting provisioning die to missing fies",0)
  assert(type(inventory.secret) == "string", 
         "Aborting, config must contain a shared secret")
  hmac = get_hmac_md5(inventory.secret) 
  return inventory, md5.sumhexa(concat(fp,":"))
end


-- Encode a response buff, add a signature and any optional buffer
------------------------------------------------------------------
send_command = function(esp, resp, buffer)
  if type(buffer) == "string" then 
    resp.data = #buffer 
  else 
    buffer = ''
  end
  local rec = json.encode(resp)
  rec = rec .. hmac(rec):sub(-6) .."\n"
-- print("requesting ", rec:sub(1,-2), #(buffer or ''))
  esp:send(rec .. buffer)  
end


-- Decode a response buff, check the signature and any optional buffer
----------------------------------------------------------------------
receive_and_parse = function(esp)
  local line = esp:receive("*l")
  local packed_cmd, sig = line:sub(1,#line-6),line:sub(-6)
-- print("reply:", packed_cmd, sig)
  local status, cmd = pcall(json.decode, packed_cmd)
  if not hmac or hmac(packed_cmd):sub(-6) == sig then
    if cmd and cmd.data == "number" then
      local data = esp:receive(cmd.data)
      return cmd, data
    end
    return cmd
  end
end

      
provision = function(esp, config, inventory, fingerprint)

  if type(config.files) ~= "table" then config.files = {} end 
  local cf = config.files

  for _, f in ipairs(inventory.files) do
    local name, size, mtime, content = f.name, f.size, f.mtime, f.content
    if not cf[name] or cf[name] ~= mtime then
      -- Send the file
      local func, action, cmd, buf
      if  f.name:sub(-4) == ".lua" then
        assert(load(content, f.name))  -- check that the contents can compile
        if content:find("--SAFETRIM\n",1,true) then
          -- if the source is tagged with SAFETRIM then its safe to remove "--"
          -- comments, leading and trailing whitespace.  Not as good as LuaSrcDiet,
          -- but this simple source compression algo preserves line numbering in
          -- the generated lc files, which helps debugging. 
          content = content:gsub("\n[ \t]+","\n")
          content = content:gsub("[ \t]+\n","\n")
          content = content:gsub("%-%-[^\n]*","")
          size = #content
        end
        action = "cm"
      else
        action = "dl"
      end
      print ("Sending file ".. name)
    
      for i = 1, size, 1024 do
        if i+1023 < size then
          cmd = {a = "pu", data = 1024}
          buf = content:sub(i, i+1023)
        else
          cmd = {a = action, data = size - i + 1, name = name}
          buf = content:sub(i)
        end 
        send_command(esp, cmd, buf)
        local resp = receive_and_parse(esp)
        assert(resp and resp.s == "OK", "Command to ESP failed")
        if resp.lcsize then 
          print("Compiled file size %s bytes" % resp.lcsize)
        end
      end
    end

    cf[name] = mtime
  end

  config.chk = fingerprint
  config.id  = nil
  config.a   = "restart"
  send_command(esp, config)

end

-- Load contents of the given file (or null if absent/unreadable)
-----------------------------------------------------------------
read_file = function(fname)
  local file = io.open(fname, "rb")
  if not file then return end
  local data = file and file:read"*a"
  file:close()
  return data
end

-- Save contents to the given file
----------------------------------
save_file = function(fname, data)
  local file = io.open(fname, "wb")
  file:write(data) 
  file:close()
end

--------------------------------------------------------------------------------------

main()  -- now that all functions have been bound to locals, we can start the show :-)



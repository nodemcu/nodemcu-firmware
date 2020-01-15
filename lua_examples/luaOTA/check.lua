--SAFETRIM
--------------------------------------------------------------------------------
-- LuaOTA provisioning system for ESPs using NodeMCU Lua
-- LICENCE: http://opensource.org/licenses/MIT
-- TerryE  15 Jul 2017
--
-- See luaOTA.md for description and implementation notes

--------------------------------------------------------------------------------

-- upvals
local crypto, json, node, wifi =
      crypto, sjson, node, wifi
local error = error
local loadfile, gc   = loadfile, collectgarbage

local self = {post = node.task.post, prefix = "luaOTA/", conf = {}}
-- luacheck: globals DEBUG
self.log     = (DEBUG == true) and print or function() end
self.modname = ...
self.timer   = tmr.create()

_G["stopOTA"] = function()
  self.timer:stop()
end
--------------------------------------------------------------------------------------
-- Utility Functions

setmetatable( self, {__index=function(obj, func) --upval: loadfile
    -- The only __index calls in in LuaOTA are dynamically loaded functions.
    -- The convention is that functions starting with "_" are treated as
    -- call-once / ephemeral; the rest are registered in self
    func = obj.prefix .. func
    local f,msg = loadfile( func..".lc")
    if msg then f, msg = loadfile(func..".lua") end
    if msg then error (msg,2) end
    if func:sub(8,8) ~= "_" then obj[func] = f end
    return f
  end} )

function self.sign(arg)  --upval: crypto, json, self
  arg = json.encode(arg)
  return arg .. crypto.toHex(crypto.hmac("MD5", arg, self.secret):sub(-3)) .. '\n'
end

function self.startApp(arg) --upval: gc, self, wifi
  gc();gc()
  self.timer:unregister()
  self.socket = nil
  if not self.config.leave then wifi.setmode(wifi.NULLMODE,false) end
  local appMod    = self.config.app   or "luaOTA.default"
  local appMethod = self.config.entry or "entry"
  if not arg then arg =  "General timeout on provisioning" end
  self.post(function() --upval: appMod, appMethod, arg
              require(appMod)[appMethod](arg)
            end)
end

function self.socket_send(socket, rec, opt_buffer)
  return socket:send(self.sign(rec) .. (opt_buffer or ''))
end

self.post(function() -- upval: self
            -- This config check is to prevent a double execution if the
            -- user invokes with "require 'luaOTA/check':_init( etc>)" form
            if not rawget(self, "config") then self:_init() end
          end)

return self

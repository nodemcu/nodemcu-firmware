--
-- File: _init.lua
--[[

  This is a template for the LFS equivalent of the SPIFFS init.lua.

  It is a good idea to such an _init.lua module to your LFS and do most of the LFS
  module related initialisaion in this. This example uses standard Lua features to
  simplify the LFS API.

  For Lua 5.1, the first section adds a 'LFS' table to _G and uses the __index
  metamethod to resolve functions in the LFS, so you can execute the main
  function of module 'fred' by executing LFS.fred(params), etc.
  It also implements some standard readonly properties:

  LFS._time    The Unix Timestamp when the luac.cross was executed.  This can be
               used as a version identifier.

  LFS._config  This returns a table of useful configuration parameters, hence
                 print (("0x%6x"):format(LFS._config.lfs_base))
               gives you the parameter to use in the luac.cross -a option.

  LFS._list    This returns a table of the LFS modules, hence
                 print(table.concat(LFS._list,'\n'))
               gives you a single column listing of all modules in the LFS.

   For Lua 5.3 LFS table is populated by the LFS implementation in C so this part
   of the code is skipped.
---------------------------------------------------------------------------------]]

local index = node.flashindex
local G=_ENV or getfenv()
local lfs_t
if _VERSION == 'Lua 5.1' then
    lfs_t = {
    __index = function(_, name)
        local fn_ut, ba, ma, size, modules = index(name)
        if not ba then
          return fn_ut
        elseif name == '_time' then
          return fn_ut
        elseif name == '_config' then
          local fs_ma, fs_size = file.fscfg()
          return {lfs_base = ba, lfs_mapped = ma, lfs_size = size,
                  fs_mapped = fs_ma, fs_size = fs_size}
        elseif name == '_list' then
          return modules
        else
          return nil
        end
      end,

    __newindex = function(_, name, value) -- luacheck: no unused
        error("LFS is readonly. Invalid write to LFS." .. name, 2)
      end,
    }

    setmetatable(lfs_t,lfs_t)
    G.module       = nil    -- disable Lua 5.0 style modules to save RAM
    package.seeall = nil
else
    lfs_t = node.LFS
end
G.LFS = lfs_t

--[[-------------------------------------------------------------------------------
  The second section adds the LFS to the require searchlist, so that you can
  require a Lua module 'jean' in the LFS by simply doing require "jean". However
  note that this is at the search entry following the FS searcher, so if you also
  have jean.lc or jean.lua in SPIFFS, then this SPIFFS version will get loaded into
  RAM instead of using. (Useful, for development).

  See docs/en/lfs.md and the 'loaders' array in app/lua/loadlib.c for more details.

---------------------------------------------------------------------------------]]

package.loaders[3] = function(module) -- loader_flash
  return lfs_t[module]
end

--[[-------------------------------------------------------------------------------
  These replaces the builtins loadfile & dofile with ones which preferentially
  loads the corresponding module from LFS if present.  Flipping the search order
  is an exercise left to the reader.-
---------------------------------------------------------------------------------]]

local lf, df = loadfile, dofile
G.loadfile = function(n)
  local mod, ext = n:match("(.*)%.(l[uc]a?)");
  local fn, ba   = index(mod)
  if ba or (ext ~= 'lc' and ext ~= 'lua') then return lf(n) else return fn end
end

G.dofile = function(n)
  local mod, ext = n:match("(.*)%.(l[uc]a?)");
  local fn, ba   = index(mod)
  if ba or (ext ~= 'lc' and ext ~= 'lua') then return df(n) else return fn() end
end

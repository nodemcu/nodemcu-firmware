#!/usr/bin/lua

--------------------------------------------------------------------------------
-- Script to be used on PC to build resource.lua file
-- Parameters: [-o outputfile] file1 [file2] ...
-- Example: ./make_resource resource/*
--   creates resource.lua file with all files in resource directory
--------------------------------------------------------------------------------

local OUT = "resource.lua"

local function readfile(file)
  local f = io.open(file, "rb")
  if f then
    local lines = f:read("*all")
    f:close()
    return lines
  end
end

-- tests the functions above
print(string.format("make_resource script - %d parameter(s)", #arg))
print("parameters: [-o outputfile] file1 [file2] ...")

local larg = {}
local outpar = false
for i, a in pairs(arg) do
  if i>0 then
    if outpar then
      OUT = a
      outpar = false
    else
      if a == "-o" then
        outpar = true
      else
        table.insert(larg, a)
      end
    end
  end
end
print(string.format("output set to: %s", OUT))

local res = io.open(OUT, "w")
res:write("--  luacheck: max line length no\nlocal arg = ...\n")
res:close(file)

for _, a in pairs(larg) do
  local inp = string.match(a, ".*[/](.*)")
  local content = readfile(a)
  print(string.format("# processing %s", inp))

  if content then
    res = io.open(OUT, "a")
    res:write(('if arg == "%s" then return %q  end\n\n'):format(inp, content))
    res:close(file)
  end
end
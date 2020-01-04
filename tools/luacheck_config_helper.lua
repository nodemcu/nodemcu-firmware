#!/usr/bin/lua

---
-- Script to extract names and the functions themselves from NodeMCU modules and
-- to help creating luacheck configuration.
-- Usage: <in modules catalog> ../../tools/luacheck_config_helper.lua *.c (or filename for single module)

local M = {}

-- Recursive object dumper, for debugging.
-- (c) 2010 David Manura, MIT License.
-- From: https://github.com/davidm/lua-inspect/blob/master/lib/luainspect/dump.lua
local ignore_keys_ = {lineinfo = true}
local norecurse_keys_ = {parent = true, ast = true}
local function dumpstring_key_(k, isseen, newindent)
  local ks = type(k) == 'string' and k:match'^[%a_][%w_]*$' and k or
             '[' .. M.dumpstring(k, isseen, newindent) .. ']'
  return ks
end

local function sort_keys_(a, b)
  if type(a) == 'number' and type(b) == 'number' then
    return a < b
  elseif type(a) == 'number' then
    return false
  elseif type(b) == 'number' then
    return true
  elseif type(a) == 'string' and type(b) == 'string' then
    return a < b
  else
    return tostring(a) < tostring(b) -- arbitrary
  end
end

function M.dumpstring(o, isseen, indent, key)
  isseen = isseen or {}
  indent = indent or ''

  if type(o) == 'table' then
    if isseen[o] or norecurse_keys_[key] then
      return (type(o.tag) == 'string' and '`' .. o.tag .. ':' or '') .. tostring(o)
    else isseen[o] = true end -- avoid recursion

    local used = {}

    local tag = o.tag
    local s = '{'
    if type(o.tag) == 'string' then
      s = '`' .. tag .. s; used['tag'] = true
    end
    local newindent = indent .. '  '

    local ks = {}; for k in pairs(o) do ks[#ks+1] = k end
    table.sort(ks, sort_keys_)

    local forcenummultiline
    for k in pairs(o) do
       if type(k) == 'number' and type(o[k]) == 'table' then forcenummultiline = true end
    end

    -- inline elements
    for _,k in ipairs(ks) do
      if ignore_keys_[k] then used[k] = true
      elseif (type(k) ~= 'number' or not forcenummultiline) and
              type(k) ~= 'table' and (type(o[k]) ~= 'table' or norecurse_keys_[k])
      then
        s = s .. dumpstring_key_(k, isseen, newindent) .. ' = ' .. M.dumpstring(o[k], isseen, newindent, k) .. ', '
        used[k] = true
      end
    end

    -- elements on separate lines
    local done
    for _,k in ipairs(ks) do
      if not used[k] then
        if not done then s = s .. '\n'; done = true end
        s = s .. newindent .. dumpstring_key_(k, isseen) .. ' = ' .. M.dumpstring(o[k], isseen, newindent, k) .. ',\n'
      end
    end
    s = s:gsub(',(%s*)$', '%1')
    s = s .. (done and indent or '') .. '}'
    return s
  elseif type(o) == 'string' then
    return string.format('%q', o)
  else
    return tostring(o)
  end
end
-- End of dump.lua code

local function printTables(fileName)

  local findBegin, field
  if type(fileName) ~= "string" then error("Wrong argument") end
  local file = io.open(fileName, "r")
  if not file then error("Can't open file") end
  local result = {}
  result.fields = {}

  for line in file:lines() do
    findBegin, _, field = string.find(line, "LROT_BEGIN%((%g+)%)")
    if findBegin then
      io.write(field.." = ")
    end
    findBegin, _, field = string.find(line, "LROT_PUBLIC_BEGIN%((%g+)%)")
    if findBegin then
      print(field.." = ")
    end

    findBegin, _, field = string.find(line, "LROT_FUNCENTRY%(%s?(%g+),")
    if not findBegin then
      findBegin, _, field = string.find(line, "LROT_NUMENTRY%(%s?(%g+),")
    end
    if not findBegin then
      findBegin, _, field = string.find(line, "LROT_TABENTRY%(%s?(%g+),")
    end
    if not findBegin then
      findBegin, _, field = string.find(line, "LROT_FUNCENTRY_S%(%s?(%g+),")
    end
    if not findBegin then
      findBegin, _, field = string.find(line, "LROT_FUNCENTRY_F%(%s?(%g+),")
    end

    if findBegin then
     if not  string.find(field, "__") then
       result.fields[field] = {}
     end
    end

    findBegin = string.find(line, "LROT_END")
    if findBegin then
      print(string.gsub(M.dumpstring(result), "{}", "empty")..',')
      result = {}
      result.fields = {}
    end
  end
end

local function main()
  for i = 1, #arg do
    printTables(arg[i])
  end
end

main()
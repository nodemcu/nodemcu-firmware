local function TERMINAL_HANDLER(e, test, msg)
  if e == 'pass' then
    print(e.." "..test..': '..msg)
  elseif e == 'fail' then
    print(e.." "..test..': '..msg)
  elseif e == 'except' then
    print(e.." "..test..': '..msg)
  end
end

local function deepeq(a, b)
  -- Different types: false
  if type(a) ~= type(b) then return false end
  -- Functions
  if type(a) == 'function' then
    return string.dump(a) == string.dump(b)
  end
  -- Primitives and equal pointers
  if a == b then return true end
  -- Only equal tables could have passed previous tests
  if type(a) ~= 'table' then return false end
  -- Compare tables field by field
  for k,v in pairs(a) do
    if b[k] == nil or not deepeq(v, b[k]) then return false end
  end
  for k,v in pairs(b) do
    if a[k] == nil or not deepeq(v, a[k]) then return false end
  end
  return true
end

-- Compatibility for Lua 5.1 and Lua 5.2
local function args(...)
  return {n=select('#', ...), ...}
end

local function spy(f)
  local s = {}
  setmetatable(s, {__call = function(s, ...)
    s.called = s.called or {}
    local a = args(...)
    table.insert(s.called, {...})
    if f then
      local r
      r = args(pcall(f, (unpack or table.unpack)(a, 1, a.n)))
      if not r[1] then
        s.errors = s.errors or {}
        s.errors[#s.called] = r[2]
      else
        return (unpack or table.unpack)(r, 2, r.n)
      end
    end
  end})
  return s
end

local pendingtests = {}
local env = _G
local gambiarrahandler = TERMINAL_HANDLER

local function runpending()
  if pendingtests[1] ~= nil then pendingtests[1](runpending) end
end

return function(name, f, async)
  if type(name) == 'function' then
    gambiarrahandler = name
    env = f or _G
    return
  end

  local testfn = function(next)

    local prev = {
      ok = env.ok,
      spy = env.spy,
      eq = env.eq
    }

    local restore = function()
      env.ok = prev.ok
      env.spy = prev.spy
      env.eq = prev.eq
      gambiarrahandler('end', name)
      table.remove(pendingtests, 1)
      if next then next() end
    end

    local handler = gambiarrahandler

    env.eq = deepeq
    env.spy = spy
    env.ok = function (cond, msg)
      if not msg then
        -- debug.getinfo() does not exist in NodeMCU
        -- msg = debug.getinfo(2, 'S').short_src..":"..debug.getinfo(2, 'l').currentline
        msg = debug.traceback()
        msg = msg:match("\n[^\n]*\n\t*([^\n]*): in")
      end
      if cond then
        handler('pass', name, msg)
      else
        handler('fail', name, msg)
      end
    end
    env.nok = function(cond, msg) env.ok(not cond, msg) end

    handler('begin', name);
    local ok, err = pcall(f, restore)
    if not ok then
      handler('except', name, err)
    end

    if not async then
      handler('end', name);
      env.ok = prev.ok;
      env.spy = prev.spy;
      env.eq = prev.eq;
    end
  end

  if not async then
    testfn()
  else
    table.insert(pendingtests, testfn)
    if #pendingtests == 1 then
      runpending()
    end
  end
end

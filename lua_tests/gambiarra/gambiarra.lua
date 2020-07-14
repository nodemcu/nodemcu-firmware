local function TERMINAL_HANDLER(e, test, msg, errormsg)
  if errormsg then
    errormsg = ": "..errormsg
  else
    errormsg = ""
  end
  if e == 'pass' then
    print("  "..e.." "..test..': '..msg)
  elseif e == 'fail' then
    print(" ==>  "..e.." "..test..': '..msg..errormsg)
  elseif e == 'except' then
    print(" ==>  "..e.." "..test..': '..msg..errormsg)
  else
    print(e.." "..test)
  end
end

local function deepeq(a, b)
  -- Different types: false
  if type(a) ~= type(b) then return {false, "type 1 is "..type(a)..", type 2 is "..type(b)} end
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

local function assertok(handler, name, cond, msg)
  if not msg then
    -- debug.getinfo() does not exist in NodeMCU
    -- msg = debug.getinfo(2, 'S').short_src..":"..debug.getinfo(2, 'l').currentline
    msg = debug.traceback()
    msg = msg:match(".*\n\t*([^\n]*): in.-\n\t*.*in function 'pcall'")
    msg = msg:match(".-([^\\/]*)$") -- cut off path of filename
  end
  local errormsg

  if type(cond) == "table" then
    errormsg = cond[2]
    cond = cond[1]
  end
  if cond then
    handler('pass', name, msg, errormsg)
  else
    handler('fail', name, msg, errormsg)
    error('_*_TestAbort_*_')
  end
end

local function fail(handler, name, func, expected, msg)
  local status, err = pcall(func)
  if status then
    local messagePart = ""
    if expected then
        messagePart = " containing \"" .. expected .. "\""
    end
    handler('fail', name, msg, "Expected to fail with Error" .. messagePart)
    error('_*_TestAbort_*_')
  end
  if (expected and not string.find(err, expected)) then
    err = err:match(".-([^\\/]*)$") -- cut off path of filename
    handler('fail', name, msg, "expected errormessage \"" .. err .. "\" to contain \"" .. expected .. "\"")
    error('_*_TestAbort_*_')
  end
  handler('pass', name, msg)
end

local pendingtests = {}
local env = _G
local outputhandler = TERMINAL_HANDLER

local function runpending()
  if pendingtests[1] ~= nil then 
    node.task.post(node.task.LOW_PRIORITY, function()
      pendingtests[1](runpending)
    end)
  end
end

local function copyenv(dest, src)
  dest.eq = src.eq
  dest.spy = src.spy
  dest.ok = src.ok
  dest.nok = src.nok
  dest.fail = src.fail
end

return function(name, f, async)
  if type(name) == 'function' then
    outputhandler = name
    env = f or _G
    return
  end

  local testfn = function(next)

    local prev = {}
    copyenv(prev, env)

    local restore = function()
      copyenv(env, prev)
      outputhandler('end', name)
      table.remove(pendingtests, 1)
      if next then next() end
    end

    local handler = outputhandler
    
    local function wrap(f, ...)
      f(handler, name, ...)
    end

    env.eq = deepeq
    env.spy = spy
    env.ok = function (cond, msg) wrap(assertok, cond, msg) end
    env.nok = function(cond, msg) env.ok(not cond, msg) end
    env.fail = function (func, expected, msg) wrap(fail, func, expected, msg) end

    handler('begin', name);
    local ok, err = pcall(f, async and restore)
    if not ok then
      err = err:match(".-([^\\/]*)$") -- cut off path of filename
      if not err:match('_*_TestAbort_*_') then
        handler('except', name, err)
      end
      if async then
        restore()
      end
    end

    if not async then
      restore()
    end
  end

  table.insert(pendingtests, testfn)
  if #pendingtests == 1 then
    runpending()
  end
end

local function TERMINAL_HANDLER(e, test, msg, errormsg)
  if errormsg then
    errormsg = ": "..errormsg
  else
    errormsg = ""
  end
  if e == 'start' then
    print("######## "..e.."ed "..test.." tests")
  elseif e == 'pass' then
    print("  "..e.." "..test..': '..msg)
  elseif e == 'fail' then
    print(" ==>  "..e.." "..test..': '..msg..errormsg)
  elseif e == 'except' then
    print(" ==>  "..e.." "..test..': '..msg..errormsg)
  elseif e == 'finish' then
    print("######## "..e.."ed "..test.." tests")
  else
    print(e.." "..test)
  end
end

-- implement pseudo task handling for on host testing
local drain_post_queue = function() end

if not node then  -- assume we run on host, not on MCU
  local post_queue = {{},{},{}}

  drain_post_queue = function()
    while #post_queue[1] + #post_queue[2] + #post_queue[3] > 0 do
      for i = 3, 1, -1 do
        if #post_queue[i] > 0 then
          local f = table.remove(post_queue[i], 1)
          if f then
            f()
          end
          break
        end
      end
    end
  end

  -- luacheck: push ignore 121 122 (setting read-only global variable)
  node = {}
  node.task = {LOW_PRIORITY = 1, MEDIUM_PRIORITY = 2, HIGH_PRIORITY = 3}
  node.task.post = function (p, f)
    table.insert(post_queue[p], f)
  end

  node.setonerror = function(fn) node.Host_Error_Func = fn end  -- luacheck: ignore 142
  -- luacheck: pop
end



--[[
if equal returns true
if different returns {msg = "<reason>"}
  this will be handled spechially by ok and nok
--]]
local function deepeq(a, b)
  local function notEqual(m)
    return { msg=m }
  end

  -- Different types: false
  if type(a) ~= type(b) then return notEqual("type 1 is "..type(a)..", type 2 is "..type(b)) end
  -- Functions
  if type(a) == 'function' then
    if string.dump(a) == string.dump(b) then
      return true
    else
      return notEqual("functions differ")
    end
  end
  -- Primitives and equal pointers
  if a == b then return true end
  -- Only equal tables could have passed previous tests
  if type(a) ~= 'table' then return notEqual("different "..type(a).."s expected "..a.." vs. "..b) end
  -- Compare tables field by field
  for k,v in pairs(a) do
    if b[k] == nil then return notEqual("key "..k.."only contained in left part") end
    local result = deepeq(v, b[k])
    if type(result) == 'table' then return result end
  end
  for k,v in pairs(b) do
    if a[k] == nil then return notEqual("key "..k.."only contained in right part") end
    local result = deepeq(a[k], v)
    if type(result) == 'table' then return result end
  end
  return true
end

-- Compatibility for Lua 5.1 and Lua 5.2
local function args(...)
  return {n=select('#', ...), ...}
end

local function spy(f)
  local mt = {}
  setmetatable(mt, {__call = function(s, ...)
    s.called = s.called or {}
    local a = args(...)
    table.insert(s.called, {...})
    if f then
      local r
      r = args(pcall(f, unpack(a, 1, a.n)))
      if not r[1] then
        s.errors = s.errors or {}
        s.errors[#s.called] = r[2]
      else
        return unpack(r, 2, r.n)
      end
    end
  end})
  return mt
end

local function getstackframe()
    -- debug.getinfo() does not exist in NodeMCU Lua 5.1
    if debug.getinfo then
      return debug.getinfo(5, 'S').short_src:match("([^\\/]*)$")..":"..debug.getinfo(5, 'l').currentline
    end

    local msg
    msg = debug.traceback()
    msg = msg:match("\t[^\t]*\t[^\t]*\t[^\t]*\t[^\t]*\t([^\t]*): in")  -- Get 5th stack frame
    msg = msg:match(".-([^\\/]*)$") -- cut off path of filename
    return msg
end

local function assertok(handler, name, invert, cond, msg)
  local errormsg
  -- check if cond is return object of 'eq' call
  if type(cond) == 'table' and cond.msg then
    errormsg = cond.msg
    cond = false
  end
  if not msg then
    msg = getstackframe()
  end

  if invert then
    cond = not cond
  end
  if cond then
    handler('pass', name, msg)
  else
    handler('fail', name, msg, errormsg)
    error('_*_TestAbort_*_')
  end
end

local function fail(handler, name, func, expected, msg)
  local status, err = pcall(func)
  if not msg then
    msg = getstackframe()
  end
  if status then
    local messageParts = {"Expected to fail with Error"}
    if expected then
        messageParts[2] = " containing \"" .. expected .. "\""
    end
    handler('fail', name, msg, table.concat(messageParts, ""))
    error('_*_TestAbort_*_')
  end
  if (expected and not string.find(err, expected)) then
    err = err:match(".-([^\\/]*)$") -- cut off path of filename
    handler('fail', name, msg, "expected errormessage \"" .. err .. "\" to contain \"" .. expected .. "\"")
    error('_*_TestAbort_*_')
  end
  handler('pass', name, msg)
end

local function NTest(testrunname, failoldinterface)

  if failoldinterface then error("The interface has changed. Please see documentstion.") end

  local pendingtests = {}
  local env = _G
  local outputhandler = TERMINAL_HANDLER
  local started

  local function runpending()
    if pendingtests[1] ~= nil then
      node.task.post(node.task.LOW_PRIORITY, function()
        pendingtests[1](runpending)
      end)
    else
      outputhandler('finish', testrunname)
    end
  end

  local function copyenv(dest, src)
    dest.eq = src.eq
    dest.spy = src.spy
    dest.ok = src.ok
    dest.nok = src.nok
    dest.fail = src.fail
  end

  local function testimpl(name, f, async)
    local testfn = function(next)

      local prev = {}
      copyenv(prev, env)

      local handler = outputhandler

      local restore = function(err)
        if err then
          err = err:match(".-([^\\/]*)$") -- cut off path of filename
          if not err:match('_*_TestAbort_*_') then
            handler('except', name, err)
          end
        end
        if node then node.setonerror() end
        copyenv(env, prev)
        outputhandler('end', name)
        table.remove(pendingtests, 1)
        collectgarbage()
        if next then next() end
      end

      local function wrap(method, ...)
        method(handler, name, ...)
      end

      local function cbError(err)
        err = err:match(".-([^\\/]*)$") -- cut off path of filename
        if not err:match('_*_TestAbort_*_') then
          handler('except', name, err)
        end
        restore()
      end

      env.eq = deepeq
      env.spy = spy
      env.ok = function (cond, msg) wrap(assertok, false, cond, msg) end
      env.nok = function(cond, msg) wrap(assertok, true,  cond, msg) end
      env.fail = function (func, expected, msg) wrap(fail, func, expected, msg) end

      handler('begin', name)
      node.setonerror(cbError)
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

    if not started then
      outputhandler('start', testrunname)
      started = true
    end


    table.insert(pendingtests, testfn)
    if #pendingtests == 1 then
      runpending()
      drain_post_queue()
    end
  end

  local function test(name, f)
    testimpl(name, f)
  end

  local function testasync(name, f)
    testimpl(name, f, true)
  end

  local function report(f, envP)
    outputhandler = f or outputhandler
    env = envP or env
  end

  local currentCoName

  local function testco(name, func)
  --  local t = tmr.create();
    local co
    testasync(name, function(Next)
      currentCoName = name

      local function getCB(cbName)
        return  function(...) -- upval: co, cbName
                  local result, err = coroutine.resume(co, cbName, ...)
                  if (not result) then
                    if (name == currentCoName) then
                      currentCoName = nil
                      Next(err)
                    else
                      outputhandler('fail', name, "Found stray Callback '"..cbName.."' from test '"..name.."'")
                    end
                  elseif coroutine.status(co) == "dead" then
                    currentCoName = nil
                    Next()
                  end
                end
      end

      local function waitCb()
        return coroutine.yield()
      end

      co = coroutine.create(function(wr, wa)
        func(wr, wa)
      end)

      local result, err = coroutine.resume(co, getCB, waitCb)
      if (not result) then
        currentCoName = nil
        Next(err)
      elseif coroutine.status(co) == "dead" then
        currentCoName = nil
        Next()
      end
    end)
  end


  return {test = test, testasync = testasync, testco = testco, report = report}
end

return NTest


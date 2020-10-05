local N = require('gambiarra')("selftest")

local expected = {}
local currentTest = {}

local failed, passed = 0,0

local orig_node = node

local cbWrap = function(cb) return cb end

-- implement pseudo task handling for on host testing
local drain_post_queue = function() end

if not node then
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

  node = {}
  node.task = {LOW_PRIORITY = 1, MEDIUM_PRIORITY = 2, HIGH_PRIORITY = 3}
  node.task.post = function (p, f)
    table.insert(post_queue[p], f)
  end

  local errorfunc
  node.setonerror = function(fn) errorfunc = fn end
  cbWrap = function(cb)
      return function(...)
          local ok, p1,p2,p3,p4 = pcall(cb, ...)
          if not ok then
            if errorfunc then
              errorfunc(p1)
            else
              print(p1, "::::::::::::: reboot :::::::::::::")
            end
          else
            return p1,p2,p3,p4
          end
        end
    end
end

-- Helper function to print arrays
local function stringify(t)
  local s = ''
  for i=1,#(t or {}) do
    s = s .. '"' .. t[i] .. '"' .. ', '
  end
  return s:gsub('..$', '')
end

-- Helper function to compare two tables
local function comparetables(t1, t2)
  if #t1 ~= (t2 and #t2 or 0) then return false end
  for i=1,#t1 do
    if t1[i] ~= t2[i] then return false end
  end
  return true
end

-- Set meta test handler
N.report(function(e, test, msg, errormsg)
  if e == 'begin' then
    currentTest.name = test
  elseif e == 'end' then
    local pass = true
    if not comparetables(expected[1].pass, currentTest.pass) then
      print("--- FAIL "..expected[1].name..' (passed): expected ['..
        stringify(expected[1].pass)..'] vs ['..
        stringify(currentTest.pass)..']')
      pass = false
    end
    if not comparetables(expected[1].fail, currentTest.fail) then
      print("--- FAIL "..expected[1].name..' (failed): expected ['..
        stringify(expected[1].fail)..'] vs ['..
        stringify(currentTest.fail)..']')
      pass = false
    end
    if not comparetables(expected[1].except, currentTest.except) then
      print("--- FAIL "..expected[1].name..' (failed): expected ['..
        stringify(expected[1].except)..'] vs ['..
        stringify(currentTest.except)..']')
      pass = false
    end
    if pass then
      print("+++ Pass "..expected[1].name)
      passed = passed + 1
    else
      failed = failed + 1
    end
    currentTest = {}
    table.remove(expected, 1)
  elseif e == 'pass' then
    currentTest.pass = currentTest.pass or {}
    table.insert(currentTest.pass, msg)
    if errormsg then table.insert(currentTest.pass, errormsg) end
  elseif e == 'fail' then
    currentTest.fail = currentTest.fail or {}
    table.insert(currentTest.fail, msg)
    if errormsg then table.insert(currentTest.fail, errormsg) end
  elseif e == 'except' then
    currentTest.except = currentTest.except or {}
    table.insert(currentTest.except, msg)
    if errormsg then table.insert(currentTest.except, errormsg) end
  elseif e == 'start' or e == 'finish' then
    -- ignore
  else
    print("Extra output: ", e, test, msg, errormsg)
  end
end)

local async_queue = {}
local function async(f) table.insert(async_queue, cbWrap(f)) end
local function async_next()
  local f = table.remove(async_queue, 1)
  if f then
    f() 
  end
end

local function drain_async_queue()
  while #async_queue > 0 do
    async_next()
  end
end

local function metatest(name, f, expectedPassed, expectedFailed, expectedExcept, async)
  local ff = f
  if async then
    ff = function(...)
      f(...)
      drain_async_queue()
    end
    if (async == "co") then
      N.testco(name,ff)
    else
      N.testasync(name, ff)
    end
  else
    N.test(name, ff)
  end
  table.insert(expected, {
    name = name,
    pass = expectedPassed,
    fail = expectedFailed,
    except = expectedExcept or {}
  })
end

--
-- Basic tests
--
metatest('simple assert passes', function()
  ok(2 == 2, '2==2')
end, {'2==2'}, {})

metatest('simple negative assert fails', function()
  nok(2 == 2, '2==2')
  nok(false, 'unreachable code')
end, {}, {'2==2'})

metatest('simple assert fails', function()
  ok(1 == 2, '1~=2')
  ok(true, 'unreachable code')
end, {}, {'1~=2'})

metatest('simple negative assert passe', function()
  nok(1 == 2, '1~=2')
end, {'1~=2'}, {})

metatest('ok without a message', function()
  ok(1 == 1)
  ok(1 == 2)
end, {'gambiarra_test.lua:185'}, {'gambiarra_test.lua:186'})

metatest('nok without a message', function()
  nok(1 == "")
  nok(1 == 1)
end, {'gambiarra_test.lua:190'}, {'gambiarra_test.lua:191'})

--
-- Equality tests
--
metatest('eq nil', function()
  ok(eq(nil, nil), 'nil == nil')
  nok(eq("", nil), '"" != nil')
  nok(eq(nil, ""), 'nil != ""')
end, {'nil == nil', '"" != nil', 'nil != ""'}, {})

metatest('eq primitives', function()
  ok(eq('foo', 'foo'), 'str == str')
  nok(eq('foo', 'bar'), 'str != str')
  ok(eq(123, 123), 'num == num')
  nok(eq(123, 12), 'num != num')
end, {'str == str', 'str != str', 'num == num', 'num != num'}, {})

metatest('eq arrays', function()
  ok(eq({}, {}), 'empty')
  ok(eq({1, 2}, {1, 2}), 'equal')
  nok(eq({1, 2}, {2, 1}), 'swapped')
  nok(eq({1, 2, 3}, {1, 2}), 'longer')
  nok(eq({1, 2}, {1, 2, 3}), 'shorter')
end, {'empty', 'equal', 'swapped', 'longer', 'shorter'}, {})

metatest('eq objects', function()
  ok(eq({}, {}), 'empty')
  ok(eq({a=1,b=2}, {a=1,b=2}), 'equal')
  ok(eq({b=2,a=1}, {a=1,b=2}), 'swapped')
  nok(eq({a=1,b=2}, {a=1,b=3}), 'not equal')
end, {'empty', 'equal', 'swapped', 'not equal'}, {})

metatest('eq nested objects', function()
  ok(eq({
    ['1'] = { name = 'mhc', age = 28 },
    ['2'] = { name = 'arb', age = 26 }
  }, {
    ['1'] = { name = 'mhc', age = 28 },
    ['2'] = { name = 'arb', age = 26 }
  }), 'equal')
  ok(eq({
    ['1'] = { name = 'mhc', age = 28 },
    ['2'] = { name = 'arb', age = 26 }
  }, {
    ['1'] = { name = 'mhc', age = 28 },
    ['2'] = { name = 'arb', age = 27 }
  }), 'not equal')
end, {'equal'}, {'not equal', 'different numbers expected 26 vs. 27'})

metatest('eq functions', function()
  ok(eq(function(x) return x end, function(x) return x end), 'equal')
  nok(eq(function(z) return x end, function(z) return y end), 'wrong variable')
  nok(eq(function(x) return x end, function(x) return x+2 end), 'wrong code')
end, {'equal', 'wrong variable', 'wrong code'}, {})

metatest('eq different types', function()
  local eqos = eq({a=1,b=2}, "text")
  ok(eq(eqos.msg, "type 1 is table, type 2 is string"), 'object/string')
  local eqfn = eq(function(x) return x end, 12)
  ok(eq(eqfn.msg, "type 1 is function, type 2 is number"), 'function/int')
  nok(eq(12, "Hallo"), 'int/string')
end, {"object/string", "function/int", 'int/string'}, {})

--
-- Spies
--
metatest('spy called', function()
  local f = spy()
  ok(not f.called or #f.called == 0, 'not called')
  f()
  ok(f.called, 'called')
  ok(#f.called == 1, 'called once')
  f()
  ok(#f.called == 2, 'called twice')
end, {'not called', 'called', 'called once', 'called twice'}, {})

metatest('spy with arguments', function()
  local x = 0
  local function setX(n) x = n end
  local f = spy(setX)
  f(1)
  ok(x == 1, 'x == 1')
  ok(eq(f.called, {{1}}), 'called with 1')
  f(42)
  ok(x == 42, 'x == 42')
  ok(eq(f.called, {{1}, {42}}), 'called with 42')
end, {'x == 1', 'called with 1', 'x == 42', 'called with 42'}, {})

metatest('spy with nils', function()
  local function nils(a, dummy, b) return a, nil, b, nil end
  local f = spy(nils)
  local r1, r2, r3, r4 = f(1, nil, 2)
  ok(eq(f.called, {{1, nil, 2}}), 'nil in args')
  ok(r1 == 1 and r2 == nil and r3 == 2 and r4 == nil, 'nil in returns')
end, {'nil in args', 'nil in returns'}, {})

metatest('spy with exception', function()
  local function throwSomething(s)
    if s ~= 'nopanic' then error('panic: '..s) end
  end
  local f = spy(throwSomething)
  f('nopanic')
  ok(f.errors == nil, 'no errors yet')
  f('foo')
  ok(eq(f.called, {{'nopanic'}, {'foo'}}), 'args ok')
  ok(f.errors[1] == nil and f.errors[2] ~= nil, 'thrown ok')
end, {'no errors yet', 'args ok', 'thrown ok'}, {})

metatest('another spy with exception', function()
  local f = spy(function() local a = unknownVariable + 1 end)
  f()
  ok(f.errors[1], 'exception thrown')
end, {'exception thrown'}, {})

metatest('spy should return a value', function()
  local f = spy(function() return 5 end)
  ok(f() == 5, 'spy returns a value')
  local g = spy()
  ok(g() == nil, 'default spy returns undefined')
end, {'spy returns a value', 'default spy returns undefined'}, {})

--
-- fail tests
--
metatest('fail with correct errormessage', function()
  fail(function() error("my error") end, "my error", "Failed with correct error")
  ok(true, 'reachable code')
end, {'Failed with correct error', 'reachable code'}, {})

metatest('fail with incorrect errormessage', function()
  fail(function() error("my error") end, "different error", "Failed with incorrect error")
  ok(true, 'unreachable code')
end, {}, {'Failed with incorrect error',
      'expected errormessage "gambiarra_test.lua:323: my error" to contain "different error"'})

metatest('fail with incorrect errormessage default message', function()
  fail(function() error("my error") end, "different error")
  ok(true, 'unreachable code')
end, {}, {'gambiarra_test.lua:329',
      'expected errormessage "gambiarra_test.lua:329: my error" to contain "different error"'})

metatest('fail with not failing code', function()
  fail(function() end, "my error", "did not fail")
  ok(true, 'unreachable code')
end, {}, {"did not fail", 'Expected to fail with Error containing "my error"'})

metatest('fail with failing code', function()
  fail(function() error("my error") end, nil, "Failed as expected")
  ok(true, 'reachable code')
end, {'Failed as expected', 'reachable code'}, {})

metatest('fail with not failing code', function()
  fail(function() end, nil , "did not fail")
  ok(true, 'unreachable code')
end, {}, {"did not fail", 'Expected to fail with Error'})

metatest('fail with not failing code default message', function()
  fail(function() end)
  ok(true, 'unreachable code')
end, {}, {"gambiarra_test.lua:350", 'Expected to fail with Error'})

--
-- except tests
--
metatest('error should panic', function()
  error("lua error")
  ok(true, 'unreachable code')
end, {}, {}, {'gambiarra_test.lua:358: lua error'})

--
-- called function except
--

local function subfunc()
  error("lua error")
end

metatest('subroutine error should panic', function()
  subfunc()
  ok(true, 'unreachable code')
end, {}, {}, {'gambiarra_test.lua:367: lua error'})

--drain_post_queue()

--
-- Async tests
--
metatest('async test', function(next)
  async(function() 
    ok(true, 'bar')
    async(function() 
      ok(true, 'baz')
      next()
    end)
  end)
  ok(true, 'foo')
end, {'foo', 'bar', 'baz'}, {}, {}, true)

metatest('async test without actually async', function(next)
  ok(true, 'bar')
  next()
end, {'bar'}, {}, {}, true)

metatest('async fail in main', function(next)
  ok(false, "async fail")
  ok(true, 'unreachable code')
end, {}, {'async fail'}, {}, true)

--drain_post_queue()

metatest('another async test after async queue drained', function(next)
  async(function() 
    ok(true, 'bar')
    next()
  end)
  ok(true, 'foo')
end, {'foo', 'bar'}, {}, {}, true)

--
-- except tests async
--
metatest('async except in main', function(next)
  error("async except")
  ok(true, 'unreachable code')
end, {}, {}, {'gambiarra_test.lua:415: async except'}, true)

metatest('async fail in callback', function(next)
  async(function() 
    ok(false, "async fail")
    next()
  end)
  ok(true, 'foo')
end, {'foo'}, {'async fail'}, {}, true)

metatest('async except in callback', function(next)
  async(function() 
    error("async Lua error")
    next()
  end)
  ok(true, 'foo')
end, {'foo'}, {}, {'gambiarra_test.lua:429: async Lua error'}, true)

--
-- sync after async test
--
local marker = 0
metatest('set marker async', function(next)
  async(function() 
    marker = "marked"
    ok(true, 'async bar')
    next()
  end)
  ok(true, 'foo')
end, {'foo', 'async bar'}, {}, {}, true)

metatest('check marker async', function()
  ok(eq(marker, "marked"), "marker check")
end, {"marker check"}, {})

--
-- coroutine async tests
--
metatest('coroutine pass', function(getCB, waitCb)
  ok(true, "passing")
end, {"passing"}, {}, {}, "co")

metatest('coroutine fail in main', function(getCB, waitCb)
  ok(false, "coroutine fail")
  ok(true, 'unreachable code')
end, {}, {'coroutine fail'}, {}, "co")

metatest('coroutine fail in main', function(getCB, waitCb)
  nok(true, "coroutine fail")
  nok(false, 'unreachable code')
end, {}, {'coroutine fail'}, {}, "co")

metatest('coroutine fail error', function(getCB, waitCb)
  fail(function() error("my error") end, "my error", "Failed with correct error")
  fail(function() error("my error") end, "other error", "Failed with other error")
  ok(true, 'unreachable code')
end, {'Failed with correct error'}, {'Failed with other error', 'expected errormessage "gambiarra_test.lua:471: my error" to contain "other error"'}, {}, "co")

metatest('coroutine except in main', function(getCB, waitCb)
  error("coroutine except")
  ok(true, 'unreachable code')
end, {}, {}, {'gambiarra_test.lua:476: coroutine except'}, "co")

--local function coasync(f) table.insert(post_queue, 1, f) end
local function coasync(f, p1, p2) node.task.post(node.task.MEDIUM_PRIORITY, function() f(p1,p2) end) end

metatest('coroutine with callback', function(getCB, waitCb)
  coasync(getCB("testCb"))
  name = waitCb()
  ok(eq(name, "testCb"), "cb")
end, {"cb"}, {}, {}, "co")

metatest('coroutine with callback with values', function(getCB, waitCb)
  coasync(getCB("testCb"), "p1", 2)
  name, p1, p2 = waitCb()
  ok(eq(name, "testCb"), "cb")
  ok(eq(p1, "p1"), "p1")
  ok(eq(p2, 2), "p2")
end, {"cb", "p1", "p2"}, {}, {}, "co")

metatest('coroutine fail after callback', function(getCB, waitCb)
  coasync(getCB("testCb"))
  name = waitCb()
  ok(eq(name, "testCb"), "cb")
  ok(false, "fail")
  ok(true, 'unreachable code')
end, {"cb"}, {"fail"}, {}, "co")

metatest('coroutine except after callback', function(getCB, waitCb)
  coasync(getCB("testCb"))
  name = waitCb()
  ok(eq(name, "testCb"), "cb")
  error("error")
  ok(true, 'unreachable code')
end, {"cb"}, {}, {"gambiarra_test.lua:509: error"}, "co")

--- detect stray callbacks after the test has finished
local strayCb
local function rewrap()
  coasync(strayCb)
end

metatest('leave stray callback', function(getCB, waitCb)
  strayCb = getCB("testa")
  coasync(rewrap)
end, {}, {}, {}, "co")

metatest('test after stray cb', function(getCB, waitCb)
end, {}, {"Found stray Callback 'testa' from test 'leave stray callback'"}, {}, "co")



--
-- Finalize: check test results
--
metatest("finishing up pending tests", function()
  for i = 1,#expected -1 do
    print("--- FAIL "..expected[i].name..' (pending)')
    failed = failed + 1
  end
  print("failed: "..failed, "passed: "..passed)
  node = orig_node
end, {}, {})

drain_post_queue()

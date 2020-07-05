local test = require('gambiarra')

local actual = {}
local expected = {}

-- Set meta test handler
test(function(e, test, msg, errormsg)
  if e == 'begin' then
    currentTest = {
      name = test,
      pass = {},
      fail = {},
      except = {}
    }
  elseif e == 'end' then
    table.insert(actual, currentTest)
  elseif e == 'pass' then
    table.insert(currentTest.pass, msg)
    if errormsg then table.insert(currentTest.pass, errormsg) end
  elseif e == 'fail' then
    table.insert(currentTest.fail, msg)
    if errormsg then table.insert(currentTest.fail, errormsg) end
  elseif e == 'except' then
    table.insert(currentTest.except, msg)
    if errormsg then table.insert(currentTest.except, errormsg) end
  end
end)

-- Helper function to print arrays
local function stringify(t)
  local s = ''
  for i=1,#t do
    s = s .. '"' .. t[i] .. '"' .. ' '
  end
  return s:gsub('%s*$', '')
end

-- Helper function to compare two tables
local function comparetables(t1, t2)
  if #t1 ~= #t2 then return false end
  for i=1,#t1 do
    if t1[i] ~= t2[i] then return false end
  end
  return true
end

local function metatest(name, f, expectedPassed, expectedFailed, expectedExcept, async)
  test(name, f, async)
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
end, {'gambiarra_test.lua:79'}, {'gambiarra_test.lua:80'})

metatest('nok without a message', function()
  nok(1 == 2)
  nok(1 == 1)
end, {'gambiarra_test.lua:84'}, {'gambiarra_test.lua:85'})

--
-- Equality tests
--
metatest('eq nil', function()
  ok(eq(nil, nil), 'nil == nil')
end, {'nil == nil'}, {})

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
end, {'equal'}, {'not equal'})

metatest('eq functions', function()
  ok(eq(function(x) return x end, function(x) return x end), 'equal')
  nok(eq(function(z) return x end, function(z) return y end), 'wrong variable')
  nok(eq(function(x) return x end, function(x) return x+2 end), 'wrong code')
end, {'equal', 'wrong variable', 'wrong code'}, {})

metatest('eq different types', function()
  local eqos = eq({a=1,b=2}, "text")
  ok(eq(eqos[2], "type 1 is table, type 2 is string"), 'object/string')
  local eqfn = eq(function(x) return x end, 12)
  ok(eq(eqfn[2], "type 1 is function, type 2 is number"), 'function/int')
end, {"object/string", "function/int"}, {})

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
  function setX(n) x = n end
  local f = spy(setX)
  f(1)
  ok(x == 1, 'x == 1')
  ok(eq(f.called, {{1}}), 'called with 1')
  f(42)
  ok(x == 42, 'x == 42')
  ok(eq(f.called, {{1}, {42}}), 'called with 42')
end, {'x == 1', 'called with 1', 'x == 42', 'called with 42'}, {})

metatest('spy with nils', function()
  function nils(a, dummy, b) return a, nil, b, nil end
  local f = spy(nils)
  r1, r2, r3, r4 = f(1, nil, 2)
  ok(eq(f.called, {{1, nil, 2}}), 'nil in args')
  ok(r1 == 1 and r2 == nil and r3 == 2 and r4 == nil, 'nil in returns')
end, {'nil in args', 'nil in returns'}, {})

metatest('spy with exception', function()
  function throwSomething(s)
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
      'expected errormessage "gambiarra_test.lua:214: my error" to contain "different error"'})

metatest('fail with not failing code', function()
  fail(function() end, "my error", "Failed without error")
  ok(true, 'unreachable code')
end, {}, {"Failed without error", 'Expected to fail with Error containing "my error"'})

--
-- except tests
--
metatest('error should panic', function()
  error("lua error")
  ok(true, 'unreachable code')
end, {}, {}, {'gambiarra_test.lua:228: lua error'})

--
-- Async tests
--
local queue = {}
local function async(f) table.insert(queue, f) end
local function async_next()
  local f = table.remove(queue, 1)
  if f then f() end
end

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

async_next()
async_next()

metatest('another async test after async queue drained', function(next)
  async(function() 
    ok(true, 'bar')
    next()
  end)
  ok(true, 'foo')
end, {'foo', 'bar'}, {}, {}, true)

async_next()

--
-- except tests async
--
metatest('error should panic async', function(next)
  async(function() 
   -- error("async Lua error")
    next()
  end)
  ok(true, 'foo')
end, {'foo'}, {}, {'gambiarra_test.lua:259: async Lua error'}, true)

async_next()

--
-- Finalize: check test results
--
local failed, passed = 0,0
for i = 1,#expected do
  if actual[i] == nil then
    print("--- FAIL "..expected[i].name..' (pending)')
    failed = failed + 1
  elseif not comparetables(expected[i].pass, actual[i].pass) then
    print("--- FAIL "..expected[i].name..' (passed): expected ['..
      stringify(expected[i].pass)..'] vs ['..
      stringify(actual[i].pass)..']')
    failed = failed + 1
  elseif not comparetables(expected[i].fail, actual[i].fail) then
    print("--- FAIL "..expected[i].name..' (failed): expected ['..
      stringify(expected[i].fail)..'] vs ['..
      stringify(actual[i].fail)..']')
    failed = failed + 1
  elseif not comparetables(expected[i].except, actual[i].except) then
    print("--- FAIL "..expected[i].name..' (failed): expected ['..
      stringify(expected[i].except)..'] vs ['..
      stringify(actual[i].except)..']')
    failed = failed + 1
  else
    print("+++ Pass "..expected[i].name)
    passed = passed + 1
  end
end

print("failed: "..failed, "passed: "..passed)
# NTest
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2020-11-01 | [Gregor Hartmann](https://github.com/HHHartmann) | [Gregor Hartmann](https://github.com/HHHartmann) | [NTest.lua](NTest.lua) |

NTest is a test system for NodeMCU which is originally based on gambiarra. It is designed to run on chip but the selftest also runs on the host using luac.cross.

!!! warning
	This module is too big to load by standard `require` function or compile on ESP8266 using `node.compile()`. The only option to load and use it is to use [LFS](../lfs.md).

## Example

``` Lua
-- Simple synchronous test
local tests = require("NTest")("first testrun")

tests.test('Check dogma', function()
  ok(2+2 == 5, 'two plus two equals five')
end)

-- A more advanced asynchronous test
tests.testasync('Do it later', function(done)
  someAsyncFn(function(result)
    ok(result == expected)
    done()     -- this starts the next async test
  end)
end)

-- An asynchronous test using a coroutine
tests.testco('Do it in place', function(getCB, waitCB)
  someAsyncFn(getCB("callback"))
  local CBName = waitCB()
  ok(CBName, "callback")
end)
```

## API

`require('NTest')`  returns an new function which must be called with a string.

``` Lua
local new = require('NTest')
```

`new(testrunname:string)`       returns an object with the following functions:

``` Lua
local tests = new("first testrun")
```

`test(name:string, f:function)` allows you to define a new test:

``` Lua
tests.test('My sync test', function()
end)
```

`testasync(name:string, f:function(done:function))` allows you to define a new asynchronous test:
To tell NTest that the test is finished you need to call the function `done` which gets passed in.
In async scenarios the test function will usually terminate quickly but the test is still waiting for
some callback to be called before it is finished.

``` Lua
tests.testasync('My async test', function(done)
  done()
end)
```

`testco(name:string, f:function(getCB:function, waitCB:function))` allows you to define a new asynchronous
test which runs in a coroutine:
This allows handling callbacks in the test in a linear way. simply use getCB to create a new callback stub.
waitCB blocks the test until the callback is called and returns the parameters.

``` Lua
tests.testco('My coroutine test', function(getCB, waitCB)
end)

tests.testco('My coroutine test with callback', function(getCB, waitCB)
  local t = tmr.create();
  t:alarm(200, tmr.ALARM_AUTO, getCB("timer"))
  local name, tCB = waitCB()
  ok(eq(name, "timer"), "CB name matches")
  
  name, tCB = waitCB()
  ok(eq(name, "timer"), "CB name matches again")

  tCB:stop()
  
  ok(true, "coroutine end")
end)

```

All test functions also define some helper functions that are added when the test is executed - `ok`, `nok`, `fail`, `eq` and `spy`.

`ok`, `nok`, `fail` are assert functions which will break the test if the condition is not met.

`ok(cond:bool, [msg:string])`. It takes any boolean condition and an optional assertion message. If no message is defined - current filename and line will be used. If the condition evaluetes to thuthy nothing happens.
If it is falsy the message is printed and the test is aborted. The next test will then be executed.

``` Lua
ok(1 == 2)                   -- prints 'foo.lua:42'
ok(1 == 2, 'one equals one') -- prints 'one equals one'
ok(1 == 1)                   -- prints nothing
```

`nok(cond:bool, [msg:string])` is a shorthand for `ok(not cond, 'msg')`.

``` Lua
nok(1 == 1)                   -- prints 'foo.lua:42'
nok(1 == 1, 'one equals one') -- prints 'one equals one'
```

`fail(func:function, [expected:string], [msg:string])` tests a function for failure. If expected is given the errormessage poduced by the function must also contain the given string else `fail` will fail the test. If no message is defined the current filename and line will be used.

``` Lua
local function f() error("my error") end
fail(f, "expected error", "Failed with incorrect error")
     -- fails with 'Failed with incorrect error' and 
     --            'expected errormessage "foo.lua:2: my error" to contain "expected error"'
``` 

`eq(a, b)` is a helper to deeply compare lua variables. It supports numbers, strings, booleans, nils, functions and tables. It's mostly useful within ok() and nok():
If the variables are equal it returns `true` else it returns `{msg='<reason>'}` This is recognized by `ok` and `nok` and results in also logging the reason for difference.

``` Lua
ok(eq(1, 1))
ok(eq({a='b',c='d'}, {c='d',a='b'})
ok(eq('foo', 'bar'))     -- will fail
```

`spy([f])` creates function wrappers that remember each call (arguments, errors) but behaves much like the real function. Real function is optional, in this case spy will return nil, but will still record its calls.
Spies are most helpful when passing them as callbacks and testing that they were called with correct values.

``` Lua
local f = spy(function(s) return #s end)
ok(f('hello') == 5)
ok(f('foo') == 3)
ok(#f.called == 2)
ok(eq(f.called[1], {'hello'})
ok(eq(f.called[2], {'foo'})
f(nil)
ok(f.errors[3] ~= nil)
```

## Reports

Another useful feature is that you can customize test reports as you need. The default `reports` just more or less prints out a basic report. You can easily override this behavior as well as add any other information you need (number of passed/failed assertions, time the test took etc):

Events are:
`start`   when testing starts
`finish`  when all tests have finished
`begin`   Will be called before each test
`end`     Will be called after each test
`pass`    Test has passed
`fail`    Test has failed with not fulfilled assert (ok, nok, fail)
`except`  Test has failed with unexpected error


``` Lua
local passed = 0
local failed = 0

tests.report(function(event, testfunc, msg)
  if event == 'begin' then
    print('Started test', testfunc)
    passed = 0
    failed = 0
  elseif event == 'end' then
    print('Finished test', testfunc, passed, failed)
  elseif event == 'pass' then
    passed = passed + 1
  elseif event == 'fail' then
    print('FAIL', testfunc, msg)
    failed = failed + 1
  elseif event == 'except' then
    print('ERROR', testfunc, msg)
  end
end)
```

Additionally, you can pass a different environment to keep `_G` unpolluted:
You need to set it, so the helper functions mentioned above can be added before calling the test function.

``` Lua
local myenv = {}

tests.report(function() ... end, myenv)

tests.test('Some test', function()
  myenv.ok(myenv.eq(...))
  local f = myenv.spy()
end)
```

You can set any of the parameters to `nil` to leave the value unchanged. 


## Appendix

This Library is for NodeMCU versions Lua 5.1 and Lua 5.3. 

It is based on https://bitbucket.org/zserge/gambiarra and includes bugfixes, substantial extensions of functionality and adaptions to NodeMCU requirements.

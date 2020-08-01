# Gambiarra

Gambiarra is a Lua version of Kludjs, and follows the idea of ultimately
minimal unit testing.

## Install

Get the sources:

`hg clone https://bitbucket.org/zserge/gambiarra`

Or get only `gambiarra.lua` and start writing tests:

`wget https://bitbucket.org/zserge/gambiarra/raw/tip/gambiarra.lua`

## Example

	-- Simple synchronous test
  tests = require("gambiarra.lua")

	tests.test('Check dogma', function()
		ok(2+2 == 5, 'two plus two equals five')
	end)

	-- A more advanced asyncrhonous test
	tests.testasync('Do it later', function(done)
		someAsyncFn(function(result)
			ok(result == expected)
			done()     -- this starts the next async test
		end)
	end)

## API

`require('gambiarra')` returns an object with the following functions:

	local tests = require('gambiarra')

`test(name:string, f:function, [async:bool])` allows you to define a new test:

	tests.test('My sync test', function()
	end)

	tests.testasync('My async test', function(done)
		done()
	end)

All test functions also define some helper functions that are added when the test is
executed - `ok`, `nok`, `fail`, `eq` and `spy`.

`ok`, `nok`, `fail` are assert functions which will break the test if the condition is not met.

`ok(cond:bool, [msg:string])`. It takes any
boolean condition and an optional assertion message. If no message is defined -
current filename and line will be used. If the condition evaluetes to thuthy nothing happens.
If it is falsy the message is printed and the test is aborted and the next test is executed.

	ok(1 == 2)                   -- prints 'foo.lua:42'
	ok(1 == 2, 'one equals one') -- prints 'one equals one'
	ok(1 == 1)                   -- prints nothing

`nok(cond:bool, [msg:string])` is a shorthand for `ok(not cond, 'msg')`.

	`nok(1 == 1)`                   -- prints 'foo.lua:42'
	`nok(1 == 1, 'one equals one')` -- prints 'one equals one'

`fail(func:function, [expected:string], [msg:string])` tests a function for failure. If expected is given the errormessage poduced by the function must contain the given string else `fail` will fail the test. If no message is defined -
current filename and line will be used.  

	`local function f() error("my error") end`
	`nok(1 == 1)`                   -- prints 'foo.lua:42'
	`fail(f, "different error", "Failed with incorrect error")`  -- prints 'Failed with incorrect error' and 
                                                              'expected errormessage "gambiarra_test.lua:294: my error" to contain "different error"'
 

`eq(a, b)` is a helper to deeply compare lua variables. It supports numbers,
strings, booleans, nils, functions and tables. It's mostly useful within ok() and nok():

	ok(eq(1, 1))
	ok(eq('foo', 'bar'))
	ok(eq({a='b',c='d'}, {c='d',a='b'})


`spy([f])` creates function wrappers that remember each call
(arguments, errors) but behaves much like the real function. Real function is
optional, in this case spy will return nil, but will still record its calls.
Spies are most helpful when passing them as callbacks and testing that they
were called with correct values.

	local f = spy(function(s) return #s end)
	ok(f('hello') == 5)
	ok(f('foo') == 3)
	ok(#f.called == 2)
	ok(eq(f.called[1], {'hello'})
	ok(eq(f.called[2], {'foo'})
	f(nil)
	ok(f.errors[3] ~= nil)

## Reports

Another useful feature is that you can customize test reports as you need.
The default reports are printed in color using ANSI escape sequences and use
UTF-8 symbols to indicate passed/failed state. If your environment doesn't
support it - you can easily override this behavior as well as add any other
information you need (number of passed/failed assertions, time the test took
etc):

Events are:

`begin`   Will be called before each test
`end`     Will be called after each test
`pass`    Test has passed
`fail`    Test has failed with not fulfilled assert (ok, nok, fail)
`except`  Test has failed with unexpected error



	local passed = 0
	local failed = 0

	tests.report(function(event, testfunc, msg)
		if event == 'begin' then
			print('Started test', testfunc)
			passed = 0
			failed = 0
		elseif event == 'end' then
			print('Finished test', testfunc, passed, failed, os.clock() - clock)
		elseif event == 'pass' then
			passed = passed + 1
		elseif event == 'fail' then
			print('FAIL', testfunc, msg)
			failed = failed + 1
		elseif event == 'except' then
			print('ERROR', testfunc, msg)
		end
	end)

Additionally, you can pass a different environment to keep `_G` unpolluted:

  local myenv = {}
  
	tests.report(function() ... end, myenv)

	tests.test('Some test', function()
		myenv.ok(myenv.eq(...))
		local f = myenv.spy()
	end)

## Appendix

This Library is for NodeMCU versions Lua 5.1 and Lua 5.3. 

It is based on https://bitbucket.org/zserge/gambiarra and includes bugfixes, extensions of functionality and adaptions to NodeMCU requirements.

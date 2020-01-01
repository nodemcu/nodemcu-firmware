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
	test('Check dogma', function()
		ok(2+2 == 5, 'two plus two equals five')
	end)

	-- A more advanced asyncrhonous test
	test('Do it later', function(done)
		someAsyncFn(function(result)
			ok(result == expected)
			done()     -- this starts the next async test
		end)
	end, true)     -- 'true' means 'async test' here

## API

`require('gambiarra')` returns a test function which can also be used to
customize test reports:

	local test = require('gambiarra')

`test(name:string, f:function, [async:bool])` allows you to define a new test:

	test('My sync test', function()
	end)

	test('My async test', function(done)
		done()
	end, true)

`test()` defines also three helper functions that are added when test is
executed - `ok`, `eq` and `spy`.

`ok(cond:bool, [msg:string])` is a simple assertion helper. It takes any
boolean condition and an optional assertion message.  If no message is define -
current filename and line will be used.

	ok(1 == 1)                   -- prints 'foo.lua:42'
	ok(1 == 1, 'one equals one') -- prints 'one equals one'

`eq(a, b)` is a helper to deeply compare lua variables. It supports numbers,
strings, booleans, nils, functions and tables. It's mostly useful within ok():

	ok(eq(1, 1))
	ok(eq('foo', 'bar'))
	ok(eq({a='b',c='d'}, {c='d',a='b'})

Finally, `spy([f])` creates function wrappers that remember each their call
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
But default tests are printed in color using ANSI escape sequences and use
UTF-8 symbols to indicate passed/failed state. If your environment doesn't
support it - you can easily override this behavior as well as add any other
information you need (number of passed/failed assertions, time the test took
etc):

	local passed = 0
	local failed = 0
	local clock = 0

	test(function(event, testfunc, msg)
		if event == 'begin' then
			print('Started test', testfunc)
			passed = 0
			failed = 0
			clock = os.clock()
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

	test(function() ... end, myenv)

	test('Some test', function()
		myenv.ok(myenv.eq(...))
		local f = myenv.spy()
	end)

## Appendix

Library supports Lua 5.1 and Lua 5.2. It is distributed under the MIT license.
Enjoy!

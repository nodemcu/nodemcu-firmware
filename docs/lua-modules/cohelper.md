# cohelper Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2019-07-24 | [TerryE](https://github.com/TerryE) | [TerryE](https://github.com/TerryE)  | [cohelper.lua](../../lua_modules/cohelper/cohelper.lua) |

This module provides a simple wrapper around long running functions to allow
these to execute within the SDK and its advised limit of 15 mSec per individual
task execution.  It does this by exploiting the standard Lua coroutine
functionality as described in the [Lua RM §2.11](https://www.lua.org/manual/5.1/manual.html#2.11) and [PiL Chapter 9](https://www.lua.org/pil/9.html).

The NodeMCU Lua VM fully supports the standard coroutine functionality. Any
interactive or callback tasks are executed in the default thread, and the coroutine
itself runs in a second separate Lua stack. The coroutine can call any library
functions, but any subsequent callbacks will, of course, execute in the default
stack.

Interaction between the coroutine and the parent is through yield and resume
statements, and since the order of SDK tasks is indeterminate, the application
must take care to handle any ordering issues.  This particular example uses
the `node.task.post()` API with the `taskYield()`function to resume itself,
so the running code can call `taskYield()` at regular points in the processing
to spilt the work into separate SDK tasks.

A similar approach could be based on timer or on a socket or pipe CB.  If you
want to develop such a variant then start by reviewing the source and understanding
what it does.

### Require
```lua
local cohelper = require("cohelper")
-- or linked directly with the  `exec()` method
require("cohelper").exec(func, <params>)
```

### Release

Not required.  All resources are released on completion of the `exec()` method.

## `cohelper.exec()`
Execute a function which is wrapped by a coroutine handler.

#### Syntax
`require("cohelper").exec(func, <params>)`

#### Parameters
- `func`: Lua function to be executed as a coroutine.
- `<params>`: list of 0 or more parameters used to initialise func.  the number and types must be matched to the funct declaration

#### Returns
Return result of first yield.

#### Notes
1.  The coroutine function `func()` has 1+_n_ arguments The first is the supplied task yield function. Calling this yield function within `func()` will temporarily break execution and cause an SDK reschedule which migh allow other executinng tasks to be executed before is resumed. The remaining arguments are passed to the `func()` on first call.
2.  The current implementation passes a single integer parameter across `resume()` /   `yield()` interface.  This acts to count the number of yields that occur.  Depending on your appplication requirements, you might wish to amend this.

### Full Example

Here is a function which recursively walks the globals environment, the ROM table
and the Registry. Without coroutining, this walk terminate with a PANIC following
a watchdog timout. I don't want to sprinkle the code with `tmr.wdclr(`) that could
in turn cause the network stack to fail. Here is how to do it using coroutining:

```Lua
require "cohelper".exec(
  function(taskYield, list)
    local s, n, nCBs = {}, 0, 0

    local function list_entry (name, v) -- upval: taskYield, nCBs
      print(name, v)
      n = n + 1
      if n % 20 == 0 then nCBs = taskYield(nCBs) end
      if type(v):sub(-5) ~= 'table' or s[v] or name == 'Reg.stdout' then return end
      s[v]=true
      for k,tv in pairs(v) do
        list_entry(name..'.'..k, tv)
      end
      s[v] = nil
    end

    for k,v in pairs(list) do
      list_entry(k, v)
    end
    print ('Total lines, print batches = ', n, nCBs)
  end,
  {_G = _G, Reg = debug.getregistry(), ROM = ROM}
)
```


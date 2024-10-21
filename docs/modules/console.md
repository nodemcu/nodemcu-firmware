# Console Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2024-10-22 | [jmattsson](https://github.com/jmattsson) | [jmattsson](https://github.com/jmattsson) | [console.c](../../components/modules/console.c)|

The `console` modules allows direct access to the system console. The system
console has typically been a UART, but by now several options are available
across the ESP32 range of SoCs, including UART, USB-Serial-JTAG and USB CDC-ACM.

By default the system console is linked up to provide an interactive Lua
shell (REPL — Read-Execute-Print Loop). It also provides a hook for listening
in on the data received on the console programatically, and the interactivity
may also be disabled (and re-enabled) programatically if so desired.

## console.on()

Used to register or deregister a callback function to handle console events.

#### Syntax
`console.on(method, [number/end_char], [function])`

#### Parameters
- `method`. One of
    - "data" for bytes received on the console
    - "error" if an error condition is encountered on the console
- `number/end_char`. Only for event `data`.
	- if pass in a number n, the callback will called when n chars are received.
	- if n=0, will receive every char in buffer.
	- if pass in a one char string "c", the callback will called when "c" is encounterd, or max n=255 received.
- `function` callback function. 
  - event "data" has a callback like this: `function(data) end`
  - event "error" has a callback like this: `function(err) end`

To unregister the callback, specify `nil` as the function.

#### Returns
`nil`

#### Example
```lua
-- when 4 chars is received.
console.on("data", 4,
  function(data)
	print("received from console:", data)
	if data=="quit" then
	  console.on("data", 0, nil)
	end
end)
-- when '\r' is received.
console.on("data", "\r",
  function(data)
	print("received from console:", data)
	if data=="quit\r" then
	  console.on("data", 0, nil)
	end
end)

-- error handler
console.on("error",
  function(err)
	print("error on console:", err)
  end)
```

## console.mode()

Controls the interactivity of the console.

#### Syntax
`console.mode(mode)`

#### Parameters
- `mode` One of
    - `console.INTERACTIVE` automatically pass console data to the Lua VM
      for execution. This is the default mode.
    - `console.NONINTERACTIVE` disables the automatic passing of console data
      to the Lua VM. The data only goes to the registered "data" callback,
      if any.

#### Returns
`nil`

#### Example
```lua
-- Implement a REL instead of the usual REPL
console.on("data", "\r", function(line) node.input(line.."\r\n") end)
console.mode(console.NONINTERACTIVE)
```

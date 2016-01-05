# uart Module
The uart module allows configuration of and communication over the uart serial port.

## uart.setup()

(Re-)configures the communication parameters of the UART.

####Syntax
`uart.setup(id, baud, databits, parity, stopbits, echo)`

####Parameters
  - `id`: Always zero, only one uart supported.
  - `baud`: One of 300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 74880, 115200, 230400, 460800, 921600, 1843200, 2686400.
  - `databits`: One of 5, 6, 7, 8.
  - `parity`: uart.PARITY_NONE, uart.PARITY_ODD, or uart.PARITY_EVEN.
  - `stopbits`: uart.STOPBITS_1, uart.STOPBITS_1_5, or uart.STOPBITS_2.
  - `echo`: if zero, disable echo, otherwise enable echo.

####Returns
number:configured baud rate

####Example
```lua
-- configure for 9600, 8N1, with echo
uart.setup(0, 9600, 8, uart.PARITY_NONE, uart.STOPBITS_1, 1)
```
___
## uart.write()

Write string or byte to the uart.

####Syntax
uart.write(id, data1 [, data2, ...])

####Parameters
  - `id`: Always zero, only one uart supported.
  - `data1`...: String or byte to send via uart.

####Returns
`nil`

####Example
```lua
uart.write(0, "Hello, world\n")
```
___
## uart.on()

Sets the callback function to handle uart events.

Currently only the "data" event is supported.

####Syntax
`uart.on(method, [number/end_char], [function], [run_input])`

####Parameters
  - `method`: "data", data has been received on the uart
  - `number/end_char`:
    - if pass in a number n<255, the callback will called when n chars are received.
    - if n=0, will receive every char in buffer.
    - if pass in a one char string "c", the callback will called when "c" is encounterd, or max n=255 received.
  - `function`: callback function, event "data" has a callback like this: function(data) end
  - `run_input`: 0 or 1; If 0, input from uart will not go into Lua interpreter, can accept binary data. If 1, input from uart will go into Lua interpreter, and run.

To unregister the callback, provide only the "data" parameter.

####Returns
`nil`

####Example
```lua
-- when 4 chars is received.
uart.on("data", 4,
  function(data)
	print("receive from uart:", data)
	if data=="quit" then
	  uart.on("data") -- unregister callback function
	end
end, 0)
-- when '\r' is received.
uart.on("data", "\r",
  function(data)
	print("receive from uart:", data)
	if data=="quit\r" then
	  uart.on("data") -- unregister callback function
	end
end, 0)
```
___


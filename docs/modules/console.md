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

There is a helper script (`scripts/upload-file.py`) which can be used to
easily upload files to NodeMCU via this module. The script may also be used
as inspiration for integrating such functionality into IDEs.

If using a SoC with USB CDC-ACM as the console, consider increasing the receive
buffer from the default. This type of console is quite prone to overflows, and
increasing the receive buffer helps mitigate (but not completely resolve) that.
Look for `Component config -> ESP system settings ->  Size of USB CDC RX buffer`
in the menuconfig (`ESP_CONSOLE_USB_CDC_RX_BUF_SIZE` in sdkconfig). Increasing
this value from the default 64 to 512 makes it match what is typically used
for a UART console. Some utilities and IDEs may have their own minimum
requirements for the receive buffer.

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
console.on("data", 4, function(data)
  print("received from console:", data)
  if data=="quit" then
    console.on("data", 0, nil)
  end
end)
-- when '\r' is received.
console.on("data", "\r", function(data)
  print("received from console:", data)
  if data=="quit\r" then
    console.on("data", 0, nil)
  end
end)

-- error handler
console.on("error", function(err)
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
Implement a REL instead of the usual REPL
```lua
console.on("data", "\r", function(line) node.input(line.."\r\n") end)
console.mode(console.NONINTERACTIVE)
```

Receive potentially binary data on the console, and process it without letting
it reach the Lua interpreter.
```lua
-- Instantiate a stream handling function that processes a classic framing
-- protocol: <STX><escaped-data><ETX> where <escaped-data> has the
-- STX, ETX and DLE symbols escaped as <DLE><STX>, <DLE><ETX>, <DLE><DLE>.
-- The chunk_cb gets called incrementally with partial stream data which is
-- effectively unescaped. When the end of frame is encountered, the done_cb
-- gets invoked.
-- To avoid overruns on slower consoles (e.g. CDC-ACM) each block gets
-- acknowledged by printing another prompt. This allows the sender to easily
-- throttle the upload to a maintainable pace.
function transmission_receiver(chunk_cb, done_cb, blocksize)
  local inframe = false
  local escaped = false
  local done = false
  local len = 0
  local STX = 2
  local ETX = 3
  local DLE = 16
  local function dispatch(data, i, j)
    if (j - i) < 0 then return end
    chunk_cb(data:sub(i, j))
  end
  return function(data)
    if done then return end
    len = len + #data
    while len >= blocksize do
      len = len - blocksize
      console.write("> ")
    end
    local from
    local to
    for i = 1, #data
    do
      local b = data:byte(i)
      if inframe
      then
        if not from then from = i end -- first valid byte
        if escaped
        then
          escaped = false
        else
          if b == DLE
          then
            escaped = true
            dispatch(data, from, i-1)
            from = nil
          elseif b == ETX
          then
            done = true
            to = i-1
            break
          end
        end
      else -- look for an (unescaped) STX to sync frame start
        if b == DLE then escaped = true
        elseif b == STX and not escaped then inframe = true
        else escaped = false
        end
      end
      -- else ignore byte outside of framing
    end
    if from then dispatch(data, from, to or #data) end
    if done then done_cb() end
  end
end

function print_hex(chunk)
  for i = 1, #chunk
  do
    print(string.format("%02x ", chunk:sub(i,i)))
  end
end

function resume_interactive()
  console.on("data", 0, nil)
  console.mode(console.INTERACTIVE)
end

-- The 0 may be adjusted upwards for improved efficiency, but be mindful to
-- always send enough data to reach the ETX marker if so.
console.on("data", 0, transmission_receiver(print_hex, resume_interactive, 64))
console.mode(console.NONINTERACTIVE)
```

Example C program for encoding data suitable for the above.
```C
#include <stdio.h>

#define STX 0x02
#define ETX 0x03
#define DLE 0x10

int main(int argc, char *argv[])
{
  putchar(STX);
  int ch;
  while ((ch = getchar()) != EOF)
  {
    switch(ch)
    {
      case STX: case ETX: case DLE: putchar(DLE); break;
      default: break;
    }
    putchar(ch);
  }
  putchar(ETX);
  return 0;
}
```

## console.write()

Provides ability to write raw, unformatted data to the console.

#### Syntax
`console.write(str_or_num [, str2_or_num ...])`

#### Parameters
- `str_or_num` Either
    - A string to write to the console. May contain binary data.
    - A number representing the character code to write the console.
  Multiple parameters may be given, and they will be written in sequence.

#### Returns
`nil`

#### Example
Write "Hello world!\n" to the console, in a roundabout manner
```lua
console.write("Hello", 0x20, "world", 0x21, "\n")
```

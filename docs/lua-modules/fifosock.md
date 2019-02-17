# fifosock Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2019-02-10 | [TerryE](https://github.com/TerryE) | [nwf](https://github.com/nwf) | [fifosock.lua](../../lua_modules/fifo/fifosock.lua) |

This module provides a convenient, efficient wrapper around the `net.socket`
`send` method.  It ensures in-order transmission while striving to minimize
memory footprint and packet count by coalescing queued strings.  It also serves
as a detailed, worked example of the `fifo` module.

## Use
```lua
ssend = (require "fifosock")(sock)

ssend("hello, ") ssend("world\n")
```

Once the `sock`et has been wrapped, one should use only the resulting `ssend`
function in lieu of `sock:send`, and one should not change the
`sock:on("sent")` callback.

### Advanced Use

In addition to passing strings representing part of the stream to be sent, it
is possible to pass the resulting `ssend` function *functions*.  These
functions will be given no parameters, but should return two values:

- A string to be sent on the socket, or `nil` if no output is desired
- A replacement function, or `nil` if the function is to be dequeued.  Functions
  may, of course, offer themselves as their own replacement to stay at the front
  of the queue.

This facility is useful for providing a replacement for the `sock:on("sent")`
callback channel.  In the fragment below, "All sent" will be `print`ed only
when the entirety of "hello, world\n" has been successfully sent on the
`sock`et.

```lua
ssend("hello, ") ssend("world\n")
ssend(function() print("All sent") end) -- implicitly returns nil, nil
```

This facility is also useful for *generators* of the stream, roughly akin to
`sendfile`-like primitives in larger systems.  Here, for example, we can stream
SPIFFS data across the network without ever holding a large amount in RAM.

```lua
local function sendfile(fn)
 local offset = 0
 local function send()
  local f = file.open(fn, "r")
  if f and f:seek("set", offset) then
    r = f:read()
    f:close()
    if r then
      offset = offset + #r
      return r, send
    end
  end
  -- implicitly returns nil, nil and falls out of the stream
 end
 return send, function() return offset end
end

local fn = "test"
ssend(("Sending file '%s'...\n"):format(fn))
dosf, getsent = sendfile(fn)
ssend(dosf)
ssend(("Sent %d bytes from '%s'\n"):format(getsent(), fn))
```

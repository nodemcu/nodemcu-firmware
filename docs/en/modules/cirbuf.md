# cirbuf Module
The cirbuf module provides a simple, in memory, [circular buffer](https://en.wikipedia.org/wiki/Circular_buffer). This simplifies the design of typical workloads like the sampling of sensors at regular intervals. The memory usage is efficient, apart from ~20 bytes for administration of the circular buffer no additional overhead is required to store the data.

## cirbuf.new()

Returns a handle to a lua data buffer. This buffer is automatically de-allocated by the internal garbage collector or by writing `nil` value to the handle.

#### Syntax
`handle = cirbuf.new(num_elements, elem_size)`

#### Parameters
 - `num_elements` number (integer) of data objects to allocate.
 - `elem_size` number (integer) size of a data object in bytes.

#### Returns
 - `handle` handle to the instantiated circular buffer.

#### Example

Create a circular buffer for 1440 data objects of 3 bytes and 100 objects of 16 bytes.

```lua
h1 = cirbuf.new(1440,3)
h2 = cirbuf.new(100,16)
```

## cirbuf.push()

Push a single data element to the buffer.

#### Syntax
`cirbuf.push(handle [, data ] [, data] ..)`

#### Parameters
 - `handle` handle of the circular buffer.
 - `data` zero or more numbers of strings. Which will be appended to the end of the data buffer. Numbers are concatenated in little endian format. If less data is available the remainder will be padded with `0`. If more data is available than the `elem_size` the resulting data will be truncated to `elem_size` bytes.
 
#### Returns
none

#### Example
Periodic sampling of a temperature and humidity sensor.

```lua
b=cirbuf.new(2880,3)
am2320.init(1,2)

function sample()
  local h,t
  if pcall(function()
      h, t =am2320.read()
    end) then
    t=t*1024
    cirbuf.push(b, h+t)
  else
    cirbuf.push(b) -- push 0 data
  end
end

interval=10000
tmr.register(0, interval, tmr.ALARM_AUTO, sample)
tmr.start(0)
```

## cirbuf.readstart()

Reads from the beginning of buffer.

#### Syntax
`string = cirbuf.readstart(handle [, maxnum])`

#### Parameters
 - `handle` handle of the circular buffer.
 - `maxnum` number (integer), maximum number of elements to return. So maximum number of bytes returned is `maxnum` * `elem_size`.

#### Returns
 - `string` string, returns the largest contiguous block of data starting at the beginning of the buffer. If `maxnum` is defined no more than `maxnum` elements will be returned. If the buffer is empty `nil` will be returned.

#### Example
```lua
h=cirbuf.new(100,6)
cirbuf.push(h, 0x31323334, "abcdefghij")
buf = cirbuf.readstart(h, 1)
print(buf, #buf)
buf = cirbuf.readstart(h)
print(buf, #buf)
```

## cirbuf.readnext()

Reads from the beginning of buffer.

#### Syntax
`string = cirbuf.readstart(handle [, maxnum])`

#### Parameters
 - `handle` handle of the circular buffer.
 - `maxnum` number (integer), maximum number of elements to return. So maximum number of bytes returned is `maxnum` * `elem_size`.

#### Returns
 - `string` string, returns the largest contiguous block of data continuing from the last read. If `maxnum` is defined no more than `maxnum` elements will be returned. If there's no more data `nil` will be returned.

#### Example
```lua
function on_sent(conn, h)
{
  local buf = cirbuf.readnext(h, 256)
  if buf then
    conn:send(buf)
  else
    conn:close()
  end
}
```

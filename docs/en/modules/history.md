# history Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-06-02 | [Terry Ellison](https://github.com/TerryE) | [Terry Ellison](https://github.com/TerryE) | [history.c](../../../app/modules/history.c)|

The history modules provides various functions for working with a circular historization buffer.

A typical use might be for debugging purposes.  Records can be dumped to a circular buffer and
the the last 20, say, can be read at a specific point in execution.

## history.buffer()

Create a new hstory buffer.  Note that:
-  The buffer is allocated in RAM so a sensble balance has to be made in having a large enough 
buffer to be useful, but leaving enough free RAM to be able to run the application.
-  The buffer persists as log as the returned value is in scope.  The buffer is garbage collected
using standard Lua rules, once dereferenced.

#### Syntax
`history.buffer(size)`

#### Parameters
  - `size` the size of the buffer to be allocated
#### Returns
A history buffer object.

#### Example
```lua
log = history.buffer(1024)
```

## history.buffer:add()

Add a new record to the head of the circular buffer.  If there isn't enough free in the buffer,
then sufficient records are deleted from the tail of the buffer to enable the new record to be added.

-  If the data is a list then this is treate as a `string.format()` argument list.
-  The records can be any length up to the size of the buffer.
-  No assumptions are made about the string content.  Any valid string can be logged and read,
including binary data; though parsing binary data must be done by the application.

#### Syntax
`history.buffer:add(record)` or <br>
`history.buffer:add(formatstring, ...)`

#### Parameters
- `record` record to be added to the buffer
- `formatstring` the format string using `string.format()` conventions.  The remaining arguments 
are processed in the context of the `formatstring`. 

#### Returns
nil

#### Example
```lua
log = history.buffer(1024)
log:add("reading at %u:%u,%u",tmr.now(),x.temp,x.rh)
```

## history.buffer:find()

Returns the number of readable records from a given reference, and set the `readnext()` location 
at this record.

#### Syntax
`history.buffer:find([n])` 

#### Parameters
- `n` record offset.  This defaults to zero if omitted 
   -  `n >=0` offset relative to the oldest log record
   -  `n < 0` offset relative to the youngest log record

#### Returns
Count of records found 

#### Example
```lua
print("No of log records", log:find())
```

## history.buffer:readnext()

Read the next records from the current `readnext()` location.  This location is then bumped
so that the next `readnext()` reads the next record.  Note that adds and reads can be intermingled, 
but if any additions wrap past the current read postion then any subsequent `readnext()` will
return `nil` until a new `find()` is issued.

#### Syntax
`history.buffer:readnext()` 

#### Parameters
None

#### Returns
Content next record, or `nil` if no valid record exists.

#### Example
```lua
for i = 1, log:find(-20) do print(log:readnext()) end
```


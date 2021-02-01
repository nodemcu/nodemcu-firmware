# pipe Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2019-07-18 | [Terry Ellison](https://github.com/TerryE) | [Terry Ellison](https://github.com/TerryE) | [pipe.c](../../app/modules/pipe.c)|

The pipe module provides a RAM-efficient means of passing character stream of records from one Lua
task to another.

## pipe.create()

Create a pipe.

#### Syntax
`pobj = pipe.create([CB_function],[task_priority])`

#### Parameters
- `CB_function` optional reader callback which is called through the `node.task.post()` when the pipe is written to.  If the CB returns a boolean, then the reposting action is forced: it is reposted if true and not if false. If the return is nil or omitted then the deault is to repost if a pipe write has occured since the last call.
-  `task_priority` See `ǹode.task.post()`

#### Returns
A pipe resource.

## pobj:read()

Read a record from a pipe object.

Note that the recommended method of reading from a pipe is to use a reader function as described below.

#### Syntax
`pobj:read([size/end_char])`

#### Parameters
- `size/end_char`
	- If numeric then a string of `size` length will be returned from the pipe.
	- If a string then this is a single character delimiter, followed by an optional "+" flag.  The delimiter is used as an end-of-record to split the character stream into separate records.  If the flag "+" is specified then the delimiter is also returned at the end of the record, otherwise it is discarded.
  - If omitted, then this defaults to `"\n+"`

Note that if the last record in the pipe is missing a delimiter or is too short, then it is still returned, emptying the pipe.

#### Returns
A string or `nil` if the pipe is empty

#### Example
```lua
line = pobj:read('\n')
line = pobj:read(50)
```

## pobj:reader()

Returns a Lua **iterator** function for a pipe object.  This is as described in the
[Lua Language: For Statement](http://www.lua.org/manual/5.1/manual.html#2.4.5). \(Note that the
`state` and `object` variables mentioned in 2.5.4 are optional and default to `nil`, so this
conforms to the`for` iterator syntax and works in a for because it maintains the state and `pobj`
internally as upvalues.

An emptied pipe takes up minimal RAM resources (an empty Lua array), and just like any other array
this is reclaimed if all variables referencing it go out of scope or are over-written.  Note
that any reader iterators that you have created also refer to the pipe as an upval, so you will
need to discard these to descope the pipe array.

#### Syntax
`myFunc = pobj:reader([size/end_char])`

#### Parameters
- `size/end_char` as for `pobj:read()`

#### Returns
-  `myFunc` iterator function

#### Examples

-  used in `for` loop:
```lua
for rec in p:reader() do print(rec) end
-- or
fp = p:reader()
-- ...
for rec in fp do print(rec) end
```

-  used in callback task:
```Lua
do
  local pipe_reader = p:reader(1400)
  local function flush(sk)  -- Upvals flush, pipe_reader
    local next = pipe_reader()
    if next then
      sk:send(next, flush)
    else
      sk:on('sent') -- dereference to allow GC
      flush = nil
    end
  end
  flush()
end
```

## pobj:unread()

Write a string to the head of a pipe object.  This can be used to back-out a previous read.

#### Syntax
`pobj:unread(s)`

#### Parameters
`s` Any input string.  Note that with all Lua strings, these may contain all character values including "\0".

#### Returns
Nothing

#### Example

```Lua
a=p:read()
p:unread(a) -- restores pipe to state before the read
```

## pobj:write()

Write a string to a pipe object.

#### Syntax
`pobj:write(s)`

#### Parameters
`s` Any input string.  Note that with all Lua strings, these may contain all character values including "\0".

#### Returns
Nothing


## pobj:nrec()

Return the number of internal records in the pipe.  Each record ranges from 1
to 256 bytes in length, with full chunks being the most common case.  As
extracting from a pipe only to `unread` if too few bytes are available, it may
be useful to have a quickly estimated upper bound on the length of the string
that would be returned.

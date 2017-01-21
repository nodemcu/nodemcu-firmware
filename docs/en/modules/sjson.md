# CJSON Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-02-01 | [Philip Gladstone](https://github.com/pjsg) | [Philip Gladstone](https://github.com/pjsg) | [sjson](../../../app/modules/sjson.c) |

The JSON support module. Allows encoding and decoding to/from JSON.

Please note that nested tables can require a lot of memory to encode. To catch out-of-memory errors, use `pcall()`.

This code using the streaming json library [jsonsl](https://github.com/mnunberg/jsonsl) to do the parsing of the string.

This module can be used in two ways. The simpler way is to use it as a direct drop-in for cjson (you can just do `_G.cjson = sjson`). 
The more advanced approach is to use the streaming interface. This allows encoding and decoding of significantly larger objects.

The handling of json null is as follows:

- By default, the decoder represents null as sjson.NULL (which is a userdata object). This is the behavior of cjson.
- The encoder always converts any userdata object into null.
- Optionally, a single string can be specified in both the encoder and decoder. This string will be used in encoding/decoding to represent json null values. This string should not be used
anywhere else in your data structures. A suitable value might be `"\0"`.

When encoding a lua object, if a function is found, then it is invoked (with no arguments) and the (single) returned value is encoded in the place of the function.

## sjson.encoder()

This creates an encoder object that can convert a LUA object into a JSON encoded string.

####Syntax
`sjson.encoder(table [, opts])`

####Parameters
- `table` data to encode
- `opts` an optional table of options. The possible entries are:
   - `depth` the maximum encoding depth needed to encode the table. The default is 20 which should be enough for nearly all situations.
   - `null` the string value to treat as null.

####Returns
A `sjson.encoder` object.

## sjson.encoder:read

This gets a chunk of JSON encoded data.

####Syntax
`encoder:read([size])`

####Parameters
- `size` an optional value for the number of bytes to return. The default is 1024.

####Returns
A string of up to `size` bytes, or `nil` if the encoding is complete and all data has been returned.

#### Example
The following example prints out (in 64 byte chunks) a JSON encoded string containing the first 4k of every file in the file system. The total string
can be bigger than the total amount of memory on the NodeMCU.
```
function files() 
   result = {}
   for k,v in pairs(file.list()) do
     result[k] = function() return file.open(k):read(4096) end
   end
   return result
end

local encoder = sjson.encoder(files())

while true do
   data = encoder:read(64)
   if not data then
      break
   end
   print(data)
end
```

## sjson.encode()

Encode a Lua table to a JSON string. This is a convenience method provided for backwards compatibility with `cjson`.

####Syntax
`sjson.encode(table [, opts])`

####Parameters
- `table` data to encode
- `opts` an optional table of options. The possible entries are:
    - `depth` the maximum encoding depth needed to encode the table. The default is 20 which should be enough for nearly all situations.
    - `null` the string value to treat as null.

####Returns
JSON string

####Example
```lua
ok, json = pcall(sjson.encode, {key="value"})
if ok then
  print(json)
else
  print("failed to encode!")
end
```

## sjson.decoder()

This makes a decoder object that can parse a JSON encoded string into a lua object. A metatable can be specified for all the newly created lua tables. This allows
you to handle each value as it is inserted into each table.

####Syntax
`sjson.decoder([opts])`

#### Parameters
- `opts` an optional table of options. The possible entries are:
    - `depth` the maximum encoding depth needed to encode the table. The default is 20 which should be enough for nearly all situations.
    - `null` the string value to treat as null.
    - `metatable` a table to use as the metatable for all the new tables in the returned object.

#### Returns
A `sjson.decoder` object


## sjson.decoder:write

This provides more data to be parsed into the lua object.

####Syntax
`decoder:write(string)`

####Parameters

- `string` the next piece of JSON encoded data

####Returns
The constructed lua object or `nil` if the decode is not yet complete.

####Errors
If a parse error occurrs during this decode, then an error is thrown and the parse is aborted. The object cannot be used again.


## sjson.decoder:result

This gets the decoded lua object, or raises an error if the decode is not yet complete

####Syntax
`decoder:result()`

####Errors
If the decode is not complete, then an error is thrown.

####Example
```
local decoder = sjson.decoder()

decoder:write("[10, 1")
decoder:write("1")
decoder:write(", \"foo\"]")

for k,v in pairs(decoder:result()) do
   print (k, v)
end
```

The next example demonstrates the use of the metatable argument. In this case it just prints out the operations, but it could suppress the assignment
altogether if desired.

```
local decoder = sjson.decoder({metatable=
        {__newindex=function(t,k,v) print("Setting '" .. k .. "' = '" .. tostring(v) .."'") 
                                    rawset(t,k,v) end}})

decoder:write('[1, 2, {"foo":"bar"}]')

```


## sjson.decode()

Decode a JSON string to a Lua table. This is a convenience method provided for backwards compatibility with `cjson`.

####Syntax
`sjson.decode(str[, opts])`

####Parameters
- `str` JSON string to decode
- `opts` an optional table of options. The possible entries are:
    - `depth` the maximum encoding depth needed to encode the table. The default is 20 which should be enough for nearly all situations.
    - `null` the string value to treat as null.
    - `metatable` a table to use as the metatable for all the new tables in the returned object.

####Returns
Lua table representation of the JSON data

####Errors
If the string is not valid JSON, then an error is thrown.

####Example
```lua
t = sjson.decode('{"key":"value"}')
for k,v in pairs(t) do print(k,v) end
```

##Constants

There is one constant -- `sjson.NULL` -- which is used in lua structures to represent the presence of a JSON null.


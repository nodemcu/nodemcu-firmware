# CJSON Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-02-01 | [Philip Gladstone](https://github.com/pjsg) | [Philip Gladstone](https://github.com/pjsg) | [sjson](../../../app/modules/sjson.c) |

The JSON support module. Allows encoding and decoding to/from JSON.

Please note that nested tables can require a lot of memory to encode. To catch out-of-memory errors, use `pcall()`.

This code using the streaming json library [jsonsl](https://github.com/mnunberg/jsonsl) to do the parsing of the string.

This module can be used in two ways. The simpler way is to use it as a direct drop-in for cjson. The more advanced approach is to use the streaming interface. This allows encoding and decoding of significantly larger objects.

In the event of errors, all memory consumed is released.

## sjson.encoder()

This creates an encoder object that can convert a LUA object into a JSON encoded string.

####Syntax
`sjson.encoder(table [, depth])`

####Parameters
- `table` data to encode
- `depth` the maximum encoding depth needed to encode the table. The default is 32 which should be enough for nearly all situations.

####Returns
A `sjson.encoder` object.





## sjson.encode()

Encode a Lua table to a JSON string. 

####Syntax
`sjson.encode(table)`

####Parameters
`table` data to encode

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

## sjson.decode()

Decode a JSON string to a Lua table. 

####Syntax
`sjson.decode(str)`

####Parameters
`str` JSON string to decode

####Returns
Lua table representation of the JSON data

####Example
```lua
t = sjson.decode('{"key":"value"}')
for k,v in pairs(t) do print(k,v) end
```

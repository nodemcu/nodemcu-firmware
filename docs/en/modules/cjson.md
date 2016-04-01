# CJSON Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-03-16 | [Mark Pulford](http://kyne.com.au/~mark/software/lua-cjson.php), [Zeroday](https://github.com/funshine) | [Zeroday](https://github.com/funshine) | [cjson](../../../app/modules/cjson.c) |

The JSON support module. Allows encoding and decoding to/from JSON.

Please note that nested tables can require a lot of memory to encode. To catch out-of-memory errors, use `pcall()`.

## cjson.encode()

Encode a Lua table to a JSON string. For details see the [documentation of the original Lua library](http://kyne.com.au/~mark/software/lua-cjson-manual.html#encode).

####Syntax
`cjson.encode(table)`

####Parameters
`table` data to encode

While it also is possible to encode plain strings and numbers rather than a table, it is not particularly useful to do so.

####Returns
JSON string

####Example
```lua
ok, json = pcall(cjson.encode, {key="value"})
if ok then
  print(json)
else
  print("failed to encode!")
end
```

## cjson.decode()

Decode a JSON string to a Lua table. For details see the [documentation of the original Lua library](http://kyne.com.au/~mark/software/lua-cjson-manual.html#_decode).

####Syntax
`cjson.decode(str)`

####Parameters
`str` JSON string to decode

####Returns
Lua table representation of the JSON data

####Example
```lua
t = cjson.decode('{"key":"value"}')
for k,v in pairs(t) do print(k,v) end
```

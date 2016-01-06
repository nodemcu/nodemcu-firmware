# cjson Module

The JSON support module. Allows encoding and decoding to/from JSON.

Please note that nested tables can require a lot of memory to encode. To catch out-of-memory errors, use `pcall()`.

## cjson.encode()

Encode a Lua table to a JSON string.

####Syntax
`cjson.encode(table)`

####Parameters
  - `table`: data to encode

While it also is possible to encode plain strings and numbers rather than a table, it is not particularly useful to do so.

####Returns
string:in json format

####Example
```lua
ok, json = pcall(cjson.encode, {key="value"})
if ok then
  print(json)
else
  print("failed to encode!")
end
```
___
## cjson.decode()

Decode a JSON string to a Lua table.

####Syntax
`cjson.decode(str)`

####Parameters
  - `str`: The JSON string to decode

####Returns
table:Lua representation of the JSON data

####Example
```lua
t = cjson.decode('{"key":"value"}')
for k,v in pairs(t) do print(k,v) end
```
___

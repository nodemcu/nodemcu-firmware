# Redis Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-02-06 | [Vladimir Dronnikov](https://github.com/dvv) | [Vladimir Dronnikov](https://github.com/dvv) | [redis.lua](../../lua_modules/redis/redis.lua) |

This Lua module provides a simple implementation of a [Redis](https://redis.io/) client.

### Require
```lua
redis = dofile("redis.lua")
```

### Release
```lua
redis = nil
```

## redis.connect()
Function used to connect to Redis server.

#### Syntax
`redis.connect(host, [port])`

#### Parameters
- `host`: Redis host name or address
- `port`: Redis database port. Default value is 6379.

#### Returns
Object with rest of the functions.

## subscribe()
Subscribe to a Redis channel.

#### Syntax
`redis:subscribe(channel, handler)`

#### Parameters
- `channel`: Channel name
- `handler`: Handler function that will be called on new message in subscribed channel

#### Returns
`nil`

## redis:publish()
Publish a message to a Redis channel.

#### Syntax
`redis:publish(channel, message)`

#### Parameters
- `channel`: Channel name
- `message`: Message to publish

#### Returns
`nil`

## redis:unsubscribe()
Unsubscribes from a channel.

#### Syntax
`redis:unsubscribe(channel)`

#### Parameters
- `channel`: Channel name to unsubscribe from

#### Returns
`nil`

#### redis:close()
Function to close connection to Redis server.

#### Syntax
`redis:close()`

#### Parameters
None

#### Returns
`nil`

#### Example
```lua
local redis = dofile("redis.lua").connect(host, port)
redis:publish("chan1", foo")
redis:subscribe("chan1", function(channel, msg) print(channel, msg) end)
```

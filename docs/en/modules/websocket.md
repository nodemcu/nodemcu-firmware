# Websocket Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-08-02 | [Luís Fonseca](https://github.com/luismfonseca) | [Luís Fonseca](https://github.com/luismfonseca) | [websocket.c](../../../app/modules/websocket.c)|

A websocket *client* module that implements [RFC6455](https://tools.ietf.org/html/rfc6455) (version 13) and provides a simple interface to send and receive messages.

The implementation supports fragmented messages, automatically respondes to ping requests and periodically pings if the server isn't communicating.

**SSL/TLS support**

Take note of constraints documented in the [net module](net.md). 


## websocket.createClient()

Creates a new websocket client. This client should be stored in a variable and will provide all the functions to handle a connection.

When the connection becomes closed, the same client can still be reused - the callback functions are kept - and you can connect again to any server.

Before disposing the client, make sure to call `ws:close()`.

#### Syntax
`websocket.createClient()`

#### Parameters
none

#### Returns
`websocketclient`

#### Example
```lua
local ws = websocket.createClient()
-- ...
ws:close()
ws = nil
```


## websocket.client:close()

Closes a websocket connection. The client issues a close frame and attemtps to gracefully close the websocket.
If server doesn't reply, the connection is terminated after a small timeout.

This function can be called even if the websocket isn't connected.

This function must *always* be called before disposing the reference to the websocket client.

#### Syntax
`websocket:close()`

#### Parameters
none

#### Returns
`nil`

#### Example
```lua
ws = websocket.createClient()
ws:close()
ws:close() -- nothing will happen

ws = nil -- fully dispose the client as Lua will now gc it
```


## websocket.client:config(params)

Configures websocket client instance.

#### Syntax
`websocket:config(params)`

#### Parameters
- `params` table with configuration parameters. Following keys are recognized:
  - `headers` table of extra request headers affecting every request

#### Returns
`nil`

#### Example
```lua
ws = websocket.createClient()
ws:config({headers={['User-Agent']='NodeMCU'}})
```


## websocket.client:connect()

Attempts to estabilish a websocket connection to the given URL.

#### Syntax
`websocket:connect(url)`

#### Parameters
- `url` the URL for the websocket.

#### Returns
`nil`

#### Example
```lua
ws = websocket.createClient()
ws:connect('ws://echo.websocket.org')
```

If it fails, an error will be delivered via `websocket:on("close", handler)`.


## websocket.client:on()

Registers the callback function to handle websockets events (there can be only one handler function registered per event type).

#### Syntax
`websocket:on(eventName, function(ws, ...))`

#### Parameters
- `eventName` the type of websocket event to register the callback function. Those events are: `connection`, `receive` and `close`.
- `function(ws, ...)` callback function.
The function first parameter is always the websocketclient.
Other arguments are required depending on the event type. See example for more details.
If `nil`, any previously configured callback is unregistered.

#### Returns
`nil`

#### Example
```lua
local ws = websocket.createClient()
ws:on("connection", function(ws)
  print('got ws connection')
end)
ws:on("receive", function(_, msg, opcode)
  print('got message:', msg, opcode) -- opcode is 1 for text message, 2 for binary
end)
ws:on("close", function(_, status)
  print('connection closed', status)
  ws = nil -- required to Lua gc the websocket client
end)

ws:connect('ws://echo.websocket.org')
```

Note that the close callback is also triggered if any error occurs.

The status code for the close, if not 0 then it represents an error, as described in the following table.


| Status Code  | Explanation  |
| :----------- | :----------- |
| 0            | User requested close or the connection was terminated gracefully |
| -1           | Failed to extract protocol from URL |
| -2           | Hostname is too large (>256 chars) |
| -3           | Invalid port number (must be >0 and <= 65535) |
| -4           | Failed to extract hostname |
| -5           | DNS failed to lookup hostname |
| -6           | Server requested termination |
| -7           | Server sent invalid handshake HTTP response (i.e. server sent a bad key) |
| -8 to -14    | Failed to allocate memory to receive message |
| -15          | Server not following FIN bit protocol correctly |
| -16          | Failed to allocate memory to send message |
| -17          | Server is not switching protocols |
| -18          | Connect timeout |
| -19          | Server is not responding to health checks nor communicating |
| -99 to -999  | Well, something bad has happenned |


## websocket.client:send()

Sends a message through the websocket connection.

#### Syntax
`websocket:send(message, opcode)`

#### Parameters
- `message` the data to send.
- `opcode` optionally set the opcode (default: 1, text message)

#### Returns
`nil` or an error if socket is not connected

#### Example
```lua
ws = websocket.createClient()
ws:on("connection", function()
  ws:send('hello!')
end)
ws:connect('ws://echo.websocket.org')
```

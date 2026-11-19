# Websocket Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2026-11-19 | Thomas Jakober | Thomas Jakober | [websocket.c](../../components/modules/websocket.c)|

A websocket *client* module (RFC 6455) for NodeMCU-ESP32. It wraps the
ESP-IDF component [`esp_websocket_client`](https://github.com/espressif/esp-protocols/tree/master/components/esp_websocket_client)
and offers the same Lua API as the ESP8266 `websocket` module, so existing
scripts run with little or no change.

Features: `ws://` and `wss://`, fragmented and large messages are reassembled
before they reach Lua, ping/pong is handled automatically, extra request
headers and subprotocols.

Differences to the ESP8266 module:

- `ws:close()` returns after the close handshake (max. ~3 s). The `close`
  callback follows asynchronously. Call `ws:connect()` again only from (or
  after) the `close` callback.
- The `close` callback receives an optional third argument: the RFC 6455
  close code sent by the server (e.g. `1000`, `1001`), if there was one.
- `ws:config()` accepts additional keys (see below).
- Incoming messages larger than `CONFIG_NODEMCU_CMODULE_WEBSOCKET_MAX_MSG`
  (default 8192 bytes, set in menuconfig) are dropped.

**TLS:** for `wss://` either pass the server's CA certificate via
`ws:config({ca_cert = pem})`, or enable
*Component config → ESP-TLS → Allow potentially insecure options →
Skip server certificate verification by default* in menuconfig.

## websocket.createClient()

Creates a new websocket client. The callbacks are kept when the connection
closes, so the same client can connect again.

#### Syntax
`websocket.createClient()`

#### Returns
`websocketclient`

## websocket.client:config(params)

Configures the client. Takes effect on the next `connect()`.

#### Syntax
`ws:config(params)`

#### Parameters
- `params` table, recognized keys:
  - `headers` table of extra request headers, e.g. `{['User-Agent']='NodeMCU'}`
  - `subprotocol` string for `Sec-WebSocket-Protocol`
  - `ping_interval` seconds between client pings (default 10)
  - `timeout` network timeout in ms for connect/send (default 10000)
  - `ca_cert` PEM string to verify the server certificate (`wss://`)

#### Returns
`nil`

## websocket.client:connect(url)

Starts connecting to `url` (`ws://host[:port][/path]` or `wss://...`).
Success is reported via the `connection` callback, failure via `close`.
While connected the client object is kept alive by the module, even if
there is no Lua reference left.

#### Syntax
`ws:connect(url)`

#### Returns
`nil`. Raises an error if the client is already connected or connecting, if
`url` does not start with `ws://` or `wss://` (`url must start with ws:// or
wss://`) or if the URL cannot be parsed (`failed to create websocket client`).
Errors while connecting (unknown host, unreachable port, ...) are not raised
but reported through the `close` callback.

## websocket.client:on(event, function)

Registers a callback. `nil` unregisters it.

| Event | Callback |
| :---- | :------- |
| `connection` | `function(ws)` |
| `receive` | `function(ws, msg, opcode)` — `opcode` 1 = text, 2 = binary |
| `close` | `function(ws, status [, server_code])` |

`status` codes (ESP8266-compatible):

| Status | Meaning |
| :----- | :------ |
| 0 | closed by `ws:close()` or by a clean close handshake |
| -5 | DNS lookup failed |
| -6 | server terminated the connection |
| -7 | invalid handshake response |
| -17 | server did not switch protocols (HTTP status ≠ 101) |
| -18 | connect failed / timeout |
| -19 | server not responding (no pong / transport error); also reported when the server process is killed while connected |
| -99 | other error |

## websocket.client:send(message [, opcode])

Sends a message. `opcode`: `1`/`websocket.TEXT` (default), `2`/`websocket.BINARY`
or `9` (ping).

#### Returns
`nil`. Raises an error if not connected or if sending fails.

## websocket.client:close()

Closes the connection gracefully (close frame, waits up to ~3 s, then forces).
Can be called when not connected. The `close` callback follows.

## Example

```lua
local ws = websocket.createClient()
ws:config({ headers = { ['User-Agent'] = 'NodeMCU-ESP32' } })

ws:on("connection", function(ws)
  print("connected")
  ws:send("hello")
end)
ws:on("receive", function(_, msg, opcode)
  print("got", msg, opcode)
end)
ws:on("close", function(_, status, code)
  print("closed", status, code)
  ws = nil   -- let Lua collect the client
end)

ws:connect("ws://192.168.1.10:8080/")
```

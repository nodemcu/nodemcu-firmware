# MQTT Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2019-01-28 | [Javier Peletier](https://github.com/jpeletier) | | [mqtt.c](../../components/modules/mqtt.c)|
| 2018-10-08 | [Tuan PM](https://github.com/tuanpmt/esp_mqtt), [Espressif](https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/protocols/mqtt.html) | | [mqtt.c](../../components/modules/mqtt.c)|

The client supports version 3.1 and 3.1.1 of the [MQTT](https://en.wikipedia.org/wiki/MQTT) protocol. Make sure that the correct version is set with `make menuconfig` -> "Component config" -> "ESP-MQTT Configurations" -> "Enable MQTT protocol 3.1.1".

!!! note "Unsupported transport modes"
    Even though the MQTT configuration offers the transport modes Websocket and Websocket Secure, they are currently not supported by the `mqtt` module.

## mqtt.Client()

Creates a MQTT client.

#### Syntax
`mqtt.Client(clientid, keepalive[, username, password, cleansession])`

#### Parameters
- `clientid` client ID
- `keepalive` keepalive seconds
- `username` user name
- `password` user password
- `cleansession` 0/1 for `false`/`true`. Default is 1 (`true`).

#### Returns
MQTT client

#### Example
```lua
-- init mqtt client without logins, keepalive timer 120s
m = mqtt.Client("clientid", 120)

-- init mqtt client with logins, keepalive timer 120sec
m = mqtt.Client("clientid", 120, "user", "password")

-- setup Last Will and Testament (optional)
-- Broker will publish a message with qos = 0, retain = 0, data = "offline" 
-- to topic "/lwt" if client don't send keepalive packet
m:lwt("/lwt", "offline", 0, 0)

m:on("connect", function(client) print ("connected") end)
m:on("offline", function(client) print ("offline") end)

-- on publish message receive event
m:on("message", function(client, topic, data) 
  print(topic .. ":" ) 
  if data ~= nil then
    print(data)
  end
end)

-- for TLS: m:connect("192.168.11.118", secure-port, 1)
m:connect("192.168.11.118", 1883, 0, function(client)
  print("connected")
  -- Calling subscribe/publish only makes sense once the connection
  -- was successfully established. You can do that either here in the
  -- 'connect' callback or you need to otherwise make sure the
  -- connection was established (e.g. tracking connection status or in
  -- m:on("connect", function)).

  -- subscribe topic with qos = 0
  client:subscribe("/topic", 0, function(client) print("subscribe success") end)
  -- publish a message with data = hello, QoS = 0, retain = 0
  client:publish("/topic", "hello", 0, 0, function(client) print("sent") end)
end,
function(client, reason)
  print("failed reason: " .. reason)
end)

m:close();
-- you can call m:connect again
```

# MQTT Client


## mqtt.client:close()

Closes connection to the broker.

#### Syntax
`mqtt:close()`

#### Parameters
none

#### Returns
`nil`

## mqtt.client:connect()

Connects to the broker specified by the given host, port, and secure options.

#### Syntax
`mqtt:connect(host[, port[, secure[, autoreconnect]]][, function(client)[, function(client, reason)]])`

#### Parameters
- `host` host, domain or IP (string)
- `port` broker port (number), default 1883
- `secure` either an interger with 0/1 for `false`/`true` (default 0),
    or a table with optional entries
    - `ca_cert` CA certificate data in PEM format for server verify with SSL
    - `client_cert` client certificate data in PEM format for SSL mutual authentication
    - `client_key` client private key data in PEM format for SSL mutual authentication
    Note that *both* `client_cert` and `client_key` have to be provided for mutual authentication.
- `autoreconnect` 0/1 for `false`/`true`, default 0. This option is *deprecated*.
- `function(client)` callback function for when the connection was established
- `function(client, reason)` callback function for when the connection could not be established. No further callbacks should be called.

#### Returns
`true` on success, `false` otherwise

#### Notes

Don't use `autoreconnect`. Let me repeat that, don't use `autoreconnect`. You should handle the errors explicitly and appropriately for
your application. In particular, the default for `cleansession` above is `true`, so all subscriptions are destroyed when the connection
is lost for any reason.

In order to acheive a consistent connection, handle errors in the error callback. For example:

```
function handle_mqtt_error(client, reason) 
  tmr.create():alarm(10 * 1000, tmr.ALARM_SINGLE, do_mqtt_connect)
end

function do_mqtt_connect()
  mqtt:connect("server", function(client) print("connected") end, handle_mqtt_error)
end
```

In reality, the connected function should do something useful!

This is the description of how the `autoreconnect` functionality may (or may not) work.

> When `autoreconnect` is set, then the connection will be re-established when it breaks. No error indication will be given (but all the
> subscriptions may be lost if `cleansession` is true). However, if the
> very first connection fails, then no reconnect attempt is made, and the error is signalled through the callback (if any). The first connection
> is considered a success if the client connects to a server and gets back a good response packet in response to its MQTT connection request.
> This implies (for example) that the username and password are correct.

## mqtt.client:lwt()

Setup [Last Will and Testament](http://www.hivemq.com/blog/mqtt-essentials-part-9-last-will-and-testament) (optional). A broker will publish a message with qos = 0, retain = 0, data = "offline" to topic "/lwt" if client does not send keepalive packet.

As the last will is sent to the broker when connecting, `lwt()` must be called BEFORE calling `connect()`.  

The broker will publish a client's last will message once he NOTICES that the connection to the client is broken. The broker will notice this when:
  - The client fails to send a keepalive packet for as long as specified in `mqtt.Client()`
  - The tcp-connection is properly closed (without closing the mqtt-connection before)
  - The broker tries to send data to the client and fails to do so, because the tcp-connection is not longer open.

This means if you specified 120 as keepalive timer, just turn off the client device and the broker does not send any data to the client, the last will message will be published 120s after turning off the device.

#### Syntax
`mqtt:lwt(topic, message[, qos[, retain]])`

#### Parameters
- `topic` the topic to publish to (string)
- `message` the message to publish, (buffer or string)
- `qos` QoS level, default 0
- `retain` retain flag, default 0

#### Returns
`nil`

## mqtt.client:on()

Registers a callback function for an event.

#### Syntax
`mqtt:on(event, function(client[, topic[, message]]))`

#### Parameters
- `event` can be "connect", "message" or "offline"
- `function(client[, topic[, message]])` callback function. The first parameter is the client. If event is "message", the 2nd and 3rd param are received topic and message (strings).

#### Returns
`nil`

## mqtt.client:publish()

Publishes a message.

#### Syntax
`mqtt:publish(topic, payload, qos, retain[, function(client)])`

#### Parameters
- `topic` the topic to publish to ([topic string](http://www.hivemq.com/blog/mqtt-essentials-part-5-mqtt-topics-best-practices))
- `payload` the message to publish, (buffer or string)
- `qos` QoS level
- `retain` retain flag
- `function(client)` optional callback fired when PUBACK received.  NOTE: When calling publish() more than once, the last callback function defined will be called for ALL publish commands.
  
#### Returns
`true` on success, `false` otherwise

## mqtt.client:subscribe()

Subscribes to one or several topics.

#### Syntax
`mqtt:subscribe(topic, qos[, function(client)])`

#### Parameters
- `topic` a [topic string](http://www.hivemq.com/blog/mqtt-essentials-part-5-mqtt-topics-best-practices)
- `qos` QoS subscription level, default 0
- `function(client)` optional callback fired when subscription(s) succeeded.  NOTE: When calling subscribe() more than once, the last callback function defined will be called for ALL subscribe commands.

#### Returns
`true` on success, `false` otherwise

#### Example
```lua
-- subscribe topic with qos = 0
m:subscribe("/topic",0, function(conn) print("subscribe success") end)
```

!!! caution

    Rather than calling `subscribe` multiple times in a row, you should call the next `subscribe` from within the callback of the previous. A generic example is provided in [mqtt_helpers.lua](../../lua_examples/mqtt/mqtt_helpers.lua).

## mqtt.client:unsubscribe()

Unsubscribes from one or several topics.

#### Syntax
`mqtt:unsubscribe(topic[, function(client)])`

#### Parameters
- `topic` a [topic string](http://www.hivemq.com/blog/mqtt-essentials-part-5-mqtt-topics-best-practices)
- `function(client)` optional callback fired when unsubscription(s) succeeded.  NOTE: When calling unsubscribe() more than once, the last callback function defined will be called for ALL unsubscribe commands.

!!! caution

    Rather than calling `unsubscribe` multiple times in a row, you should call the next `unsubscribe` from within the callback of the previous. A generic example is provided in [mqtt_helpers.lua](../../lua_examples/mqtt/mqtt_helpers.lua).

#### Returns
`true` on success, `false` otherwise

#### Example
```lua
-- unsubscribe topic
m:unsubscribe("/topic", function(conn) print("unsubscribe success") end)
```

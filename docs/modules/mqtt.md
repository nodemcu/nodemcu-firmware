# MQTT Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-01-23 | [Stephen Robinson](https://github.com/esar/contiki-mqtt), [Tuan PM](https://github.com/tuanpmt/esp_mqtt) | [Vowstar](https://github.com/vowstar) | [mqtt.c](../../app/modules/mqtt.c)|


The client adheres to version 3.1.1 of the [MQTT](https://en.wikipedia.org/wiki/MQTT) protocol. Make sure that your broker supports and is correctly configured for version 3.1.1. The client is backwards incompatible with brokers running MQTT 3.1.

## mqtt.Client()

Creates a MQTT client.

#### Syntax
`mqtt.Client(clientid, keepalive[, username, password, cleansession, max_message_length])`

#### Parameters
- `clientid` client ID
- `keepalive` keepalive seconds
- `username` user name
- `password` user password
- `cleansession` 0/1 for `false`/`true`. Default is 1 (`true`).
- `max_message_length`, how large messages to accept. Default is 1024.

#### Returns
MQTT client

#### Notes

According to MQTT specification the max PUBLISH length is 256Mb. This is too large for NodeMCU to realistically handle. To avoid
an out-of-memory situation, there is a limit on how big messages to accept. This is controlled by the `max_message_length` parameter.
In practice, this only affects incoming PUBLISH messages since all regular control packets are small.
The default 1024 was chosen as this was the implicit limit in NodeMCU 2.2.1 and older (where this was not handled at all).

Note that "message length" refers to the full MQTT message size, including fixed & variable headers, topic name, packet ID (if applicable),
and payload. For exact details, please see [the MQTT specification](http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718037).

Any message *larger* than `max_message_length` will be (partially) delivered to the `overflow` callback, if defined. The rest
of the message will be discarded. Any subsequent messages should be handled as expected.
Discarded messages will still be ACK'ed if QoS level 1 or 2 was requested, even if the application stack cannot handle them.

Heap memory will be used to buffer any message which spans more than a single TCP packet. A single allocation for the full
message will be performed when the message header is first seen, to avoid heap fragmentation.
If allocation fails, the MQTT session will be disconnected.
Naturally, messages larger than `max_message_length` will not be stored.

Note that heap allocation may occur even if the individual messages are not larger than the configured max! For example,
the broker may send multiple smaller messages in quick succession, which could go into the same TCP packet. If the last message
in the TCP packet did not fit fully, a heap buffer will be allocated to hold the incomplete message while waiting for the next TCP packet.

The typical maximum size for a message to fit into a single TCP packet is 1460 bytes, but this depends on the network's MTU
configuration, any packet fragmentation, and as described above, multiple messages in the same TCP packet.


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
m:on("connfail", function(client, reason) print ("connection failed", reason) end)
m:on("offline", function(client) print ("offline") end)

-- on publish message receive event
m:on("message", function(client, topic, data)
  print(topic .. ":" )
  if data ~= nil then
    print(data)
  end
end)

-- on publish overflow receive event
m:on("overflow", function(client, topic, data)
  print(topic .. " partial overflowed message: " .. data )
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
`true` on success, `false` otherwise

## mqtt.client:connect()

Connects to the broker specified by the given host, port, and secure options.

#### Syntax
`mqtt:connect(host[, port[, secure]][, function(client)[, function(client, reason)]])`

#### Parameters
- `host` host, domain or IP (string)
- `port` broker port (number), default 1883
- `secure` boolean: if `true`, use TLS. Take note of constraints documented in the [net module](net.md).
- `function(client)` callback function for when the connection was established
- `function(client, reason)` callback function for when the connection could not be established. No further callbacks should be called.

!!! attention

    Secure (`https`) connections come with quite a few limitations.  Please see
    the warnings in the [tls module](tls.md)'s documentation.

#### Returns
`true` on success, `false` otherwise

#### Notes

An application should watch for connection failures and handle errors in the error callback,
in order to achieve a reliable connection to the server.  For example:

```
function handle_mqtt_error(client, reason)
  tmr.create():alarm(10 * 1000, tmr.ALARM_SINGLE, do_mqtt_connect)
end

function do_mqtt_connect()
  mqtt:connect("server", function(client) print("connected") end, handle_mqtt_error)
end
```

In reality, the connected function should do something useful!

The first callback to `:connect()` aliases with the "connect" callback
available through `:on()` (the last passed callback to either of those are
used).  However, if `nil` is passed to `:connect()`, any existing callback
will be preserved, rather than removed.

The second (failure) callback aliases with the "connfail" callback available
through `:on()`.  (The "offline" callback is only called after an already
established connection becomes closed. If the `connect()` call fails to
establish a connection, the callback passed to `:connect()` is called and
nothing else.)

Previously, we instructed an application to pass either the *integer* 0 or
*integer* 1 for `secure`.  Now, this will trigger a deprecation warning; please
use the *boolean* `false` or `true` instead.

#### Connection failure callback reason codes:

| Constant | Value | Description |
|----------|-------|-------------|
|`mqtt.CONN_FAIL_SERVER_NOT_FOUND`|-5|There is no broker listening at the specified IP Address and Port|
|`mqtt.CONN_FAIL_NOT_A_CONNACK_MSG`|-4|The response from the broker was not a CONNACK as required by the protocol|
|`mqtt.CONN_FAIL_DNS`|-3|DNS Lookup failed|
|`mqtt.CONN_FAIL_TIMEOUT_RECEIVING`|-2|Timeout waiting for a CONNACK from the broker|
|`mqtt.CONN_FAIL_TIMEOUT_SENDING`|-1|Timeout trying to send the Connect message|
|`mqtt.CONNACK_ACCEPTED`|0|No errors. _Note: This will not trigger a failure callback._|
|`mqtt.CONNACK_REFUSED_PROTOCOL_VER`|1|The broker is not a 3.1.1 MQTT broker.|
|`mqtt.CONNACK_REFUSED_ID_REJECTED`|2|The specified ClientID was rejected by the broker. (See `mqtt.Client()`)|
|`mqtt.CONNACK_REFUSED_SERVER_UNAVAILABLE`|3|The server is unavailable.|
|`mqtt.CONNACK_REFUSED_BAD_USER_OR_PASS`|4|The broker refused the specified username or password.|
|`mqtt.CONNACK_REFUSED_NOT_AUTHORIZED`|5|The username is not authorized.|

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
- `event` can be "connect", "connfail", "suback", "unsuback", "puback", "message", "overflow", or "offline"
- callback function.  The first parameter is always the client object itself.
  Any remaining parameters passed differ by event:

  - If event is "message", the 2nd and 3rd parameters are received topic and
    message, respectively, as Lua strings.

  - If the event is "overflow", the parameters are as with "message", save
    that the message string is truncated to the maximum message size.

  - If the event is "connfail", the 2nd parameter will be the connection
    failure code; see above.

  - Other event types do not provide additional arguments.  This has some
    unfortunate consequences: the broker-provided subscription maximum QoS
    information is lost, and the application must, if it expects per-event
    acknowledgements, manage a queue or queues itself.

#### Returns
`nil`

## mqtt.client:publish()

Publishes a message.

#### Syntax
`mqtt:publish(topic, payload, qos, retain[, function(client)])`

#### Parameters
- `topic` the topic to publish to ([topic string](http://www.hivemq.com/blog/mqtt-essentials-part-5-mqtt-topics-best-practices))
- `message` the message to publish, (buffer or string)
- `qos` QoS level
- `retain` retain flag
- `function(client)` optional callback fired when PUBACK received (for QoS 1
  or 2) or when message sent (for QoS 0).

#### Notes

When calling publish() more than once, the last callback function defined will
be called for ALL publish commands.  This callback argument also aliases with
the "puback" callback for `:on()`.

#### Returns
`true` on success, `false` otherwise

## mqtt.client:subscribe()

Subscribes to one or several topics.

#### Syntax
`mqtt:subscribe(topic, qos[, function(client)])`
`mqtt:subscribe(table[, function(client)])`

#### Parameters
- `topic` a [topic string](http://www.hivemq.com/blog/mqtt-essentials-part-5-mqtt-topics-best-practices)
- `qos` QoS subscription level, default 0
- `table` array of 'topic, qos' pairs to subscribe to
- `function(client)` optional callback fired when subscription(s) succeeded.

#### Notes

When calling subscribe() more than once, the last callback function defined
will be called for ALL subscribe commands. This callback argument also aliases
with the "suback" callback for `:on()`.

#### Returns
`true` on success, `false` otherwise

#### Example
```lua
-- subscribe topic with qos = 0
m:subscribe("/topic",0, function(conn) print("subscribe success") end)

-- or subscribe multiple topic (topic/0, qos = 0; topic/1, qos = 1; topic2 , qos = 2)
m:subscribe({["topic/0"]=0,["topic/1"]=1,topic2=2}, function(conn) print("subscribe success") end)
```

!!! caution

    Rather than calling `subscribe` multiple times you should use the multiple topics syntax shown in the above example if you want to subscribe to more than one topic at once.

## mqtt.client:unsubscribe()

Unsubscribes from one or several topics.

#### Syntax
`mqtt:unsubscribe(topic[, function(client)])`
`mqtt:unsubscribe(table[, function(client)])`

#### Parameters
- `topic` a [topic string](http://www.hivemq.com/blog/mqtt-essentials-part-5-mqtt-topics-best-practices)
- `table` array of 'topic, anything' pairs to unsubscribe from
- `function(client)` optional callback fired when unsubscription(s) succeeded.

#### Notes

When calling subscribe() more than once, the last callback function defined
will be called for ALL subscribe commands. This callback argument also aliases
with the "unsuback" callback for `:on()`.

#### Returns
`true` on success, `false` otherwise

#### Example
```lua
-- unsubscribe topic
m:unsubscribe("/topic", function(conn) print("unsubscribe success") end)

-- or unsubscribe multiple topic (topic/0; topic/1; topic2)
m:unsubscribe({["topic/0"]=0,["topic/1"]=0,topic2="anything"}, function(conn) print("unsubscribe success") end)
```

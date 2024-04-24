# ESP-NOW Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2024-03-07 | [DiUS](https://github.com/DiUS) [Jade Mattsson](https://github.com/jmattsson) |[Jade Mattsson](https://github.com/jmattsson) | [espnow.c](../../components/modules/espnow.c)|

The `espnow` module provides an interface to Espressif's [ESP-NOW functionality](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html). To quote their documentation directly:

"ESP-NOW is a kind of connectionless Wi-Fi communication protocol that is defined by Espressif. In ESP-NOW, application data is encapsulated in a vendor-specific action frame and then transmitted from one Wi-Fi device to another without connection."

Packets can be sent to either individual peers, the whole list of defined peers, or broadcast to everyone in range. For non-broadcast packets, ESP-NOW provides optional encryption support to prevent eavesdropping. To use encryption, a "Primary Master Key" (PMK) should first be set. When registering a peer, a peer-specific "Local Master Key" (LMK) is then given, which is further encrypted by the PMK. All packets sent to that peer will be encrypted with the resulting key.

To broadcast packets, a peer with address 'ff:ff:ff:ff:ff:ff' must first be registered. Broadcast packets do not support encryption, and attempting to enable encrypting when registering the broadcast peer will result in an error.

ESP-NOW uses a WiFi vendor-specific action frame to transmit data, and as such it requires the WiFi stack to have been started before ESP-NOW packets can be sent and received.


## espnow.start

Starts the ESP-NOW stack. While this may be called prior to `wifi.start()`, packet transmission and reception will not be possible until after the WiFi stack has been started.

#### Syntax
```lua
espnow.start()
```

#### Parameters
None.

#### Returns
`nil`

An error will be raised if the ESP-NOW stack cannot be started.


## espnow.stop

Stops the ESP-NOW stack.

#### Syntax
```lua
espnow.stop()
```

#### Parameters
None.

#### Returns
`nil`

An error will be raised if the ESP-NOW stack cannot be stopped.


## espnow.getversion

Returns the raw version number enum value. Currently, it is `1`. Might be useful for checking version compatibility in the future.

#### Syntax
```lua
ver = espnow.getversion()
```

#### Parameters
None.

#### Returns
An integer representing the ESP-NOW version.


## espnow.setpmk

Sets the Primary Master Key (PMK). When using security, this should be done prior to adding any peers, as their LMKs will be encrypted by the current PMK.

#### Syntax
```lua
espnow.setpmk(pmk)
```
#### Parameters
`pmk`  The Primary Master Key, given as a hex-encoding of a 16-byte key (i.e. the `pmk` should consist of 32 hex digits.

#### Returns
`nil`

An error will be raised if the PMK cannot be set.

#### Example
```lua
espnow.setpmk('00112233445566778899aabbccddeeff')
```


## espnow.setwakewindow

Controls the wake window during which ESP-NOW listens. In most cases this should never need to be changed from the default. Refer to the Espressif documentation for further details.

#### Syntax
```lua
espnow.setwakewindow(window)
```

#### Parameters
`window`  An integer between 0 and 65535.

#### Returns
`nil`


## espnow.addpeer

Registers a peer MAC address. Optionally parameters for the peer may be included, such as encryption key.

#### Syntax
```lua
espnow.addpeer(mac, options)
```
#### Parameters
- `mac`  The peer mac address, given as a string in `00:11:22:33:44:55` format (colons optional, and may also be replaced by '-' or ' ').
- `options`  A table with with following entries:
  - `channel`  An integer indicating the WiFi channel to be used. The default is `0`, indicating that the current WiFi channel should be used. If non-zero, must match the current WiFi channel.
  - `lmk`  The LMK for the peer, if encryption is to be used.
  - `encrypt`  A non-zero integer to indicate encryption should be enabled. When set, makes `lmk` a required field.

#### Returns
`nil`

An error will be raised if a peer cannot be added, such as if the peer list if full, or the peer has already been added.

#### Examples

Adding a peer without encryption enabled.
```lua
espnow.addpeer('7c:df:a1:c1:4c:71')
```

Adding a peer with encryption enabled. Please use randomly generated keys instead of these easily guessable placeholders.
```lua
espnow.setpmk('ffeeddccbbaa99887766554433221100')
espnow.addpeer('7c:df:a1:c1:4c:71', { encrypt = 1, lmk = '00112233445566778899aabbccddeeff' })
```

## espnow.delpeer

Deletes a previously added peer from the internal peer list.

#### Syntax
```lua
espnow.delpeer(mac)
```

#### Parameters
`mac`  The MAC address of the peer to delete.

#### Returns
`nil`

Returns an error if the peer cannot be deleted.


## espnow.on

Registers or unregisters callback handlers for the ESP-NOW events.

There are two events available, `sent` which is issued in response to a packet send request and which reports the status of the send attempt, and 'receive' which is issued when an ESP-NOW packet is successfully received.

Only a single callback function can be registered for each event.

The callback function for the `sent` event is invoked with two parameters, the destination MAC address, and a `1`/`nil` to indicate whether the send was believed to be successful or not.

The callback function for the `receive` event is invoked with a single parameter, a table with the following keys:

- `src`  The sender MAC address
- `dst`  The destination MAC address (likely either the local MAC of the receiver, or the broadcast address)
- `rssi`  The RSSI value from the packet, indicating signal strength between the two devices
- `data`  The actual payload data, as a string. The string may contain binary data.

#### Syntax
```lua
espnow.on(event, callbackfn)
```

#### Parameters
- `event`  The event name, one of `sent` or `receive`.
- `callbackfn`  The callback function to register, or `nil` to unregister the previously set callback function for the event.

#### Returns
`nil`

Raises an error if invalid arguments are given.

#### Example
Registering callback handlers.
```lua
espnow.on('sent',
  function(mac, success) print(mac, success and 'Yay!' or 'Noooo') end)
espnow.on('receive',
  function(t) print(t.src, '->', t.dst, '@', t.rssi, ':', t.data) end)
```

Unregistering callback handlers.
```lua
espnow.on('sent') -- implicit nil
espnow.on('receive', nil)
```


## espnow.send

Attempts to send an ESP-NOW packet to one or more peers.

In general it is strongly recommended to use the encryption functionality, as this ensures not only secrecy but also prevent unintentional interference between different users of ESP-NOW.

If you do need to use broadcasts or multicasts, you should make sure to have a unique, recognisable marker at the start of the payload to make filtering out unwanted messages easy, both for you and other ESP-NOW users.

#### Syntax
```lua
espnow.send(peer, data)
```

#### Parameters
- `peer`  The peer MAC address to send to. Must have previously been added via `espnow.addpeer()`. If `peer` is given as `nil`, the packet is sent to all registered non-broadcast/multicast peers, and the `sent` callback is invoked for each of those peers.
- `data`  A string of data to send. May contain binary bytes. Maximum supported length at the time of writing is 250 bytes.

#### Returns
`nil`, but the `sent` callback is invoked with the status afterwards.

Raises an error if the peer is not valid, or other fatal errors preventing a send attempt from even being made. The `sent` callback will not be invoked in this case.

#### Example
Broadcasting a message to every single ESP-NOW device in range.
```lua
bcast='ff:ff:ff:ff:ff:ff'
espnow.addpeer(bcast)
espnow.send(bcast, '[NodeMCU] Hello, world!')
```

Sending a directed message to one specific ESP-NOW device.
```lua
peer='7c:df:a1:c1:4c:71'
espnow.addpeer(peer)
espnow.send(peer, 'Hello, you!')
```

Sending a message to all registered peers.
```lua
espnow.addpeer('7c:df:a1:c1:4c:71')
espnow.addpeer('7c:df:a1:c1:4c:47')
espnow.addpeer('7c:df:a1:c1:4f:12')
espnow.send(nil, 'Hello, peers!')
```

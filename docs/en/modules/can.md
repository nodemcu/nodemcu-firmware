# CAN Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-04-27 | [@ThomasBarth](https://github.com/ThomasBarth/ESP32-CAN-Driver/), zelll | | [can.c](../../../components/modules/can.c)|

The CAN module provides access to the in-built CAN controller.


## can.setup()
Configuration CAN controller.

#### Syntax
```lua
can.setup(config = {
  speed = 1000,
  tx = 5,
  rx = 4,
  dual_filter = false,
  code = 0,
  mask = 0xffffffff
}, callback)
```

#### Parameters
- `config`
  - `speed` kbps. One of following value: `1000`, `800`, `500`, `250`, `100`
  - `tx` Pin num for TX
  - `rx` Pin num for RX
  - `dual_filter` `true` dual filter mode, `false` single filter mode (default)
  - `code` 4-bytes integer. Use this with mask to filter CAN frame. See [SJA1000](http://www.nxp.com/documents/data_sheet/SJA1000.pdf)
  - `mask` 4-bytes integer.
- `callback` function to be called when CAN data received.
  - `msg_id` CAN Messge ID
  - `data` CAN data, up to 8 bytes
  
#### Returns
nil


## can.start()
Start CAN controller.

#### Syntax
`can.start()`

#### Parameters

#### Returns
nil


## can.stop()
Stop CAN controller.

#### Syntax
`can.start()`

#### Parameters

#### Returns
nil


#### Syntax
`can.send(msg_id, data)`

#### Parameters
- `msg_id` CAN Messge ID
- `data` CAN data, up to 8 bytes
  
#### Returns
nil

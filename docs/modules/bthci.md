# BT HCI Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-09-29 | [DiUS](https://github.com/DiUS), [Johny Mattsson](https://github.com/jmattsson) | [Johny Mattsson](https://github.com/jmattsson) | [bthci.c](../../components/modules/bthci.c)|

The BT HCI module provides a minimal HCI-level interface to BlueTooth
adverisements. Via this module you can set up BT LE advertisements and also
receive advertisements from other devices.

Advertisements are an easy way of publishing sensor data to e.g. a
smartphone app.


## bthci.rawhci(hcibytes, callback)

Sends a raw HCI command to the BlueTooth controller.

Only intended for development use.

#### Syntax
`bthci.rawhci(hcibytes [, callback])`

#### Parameters
- `hcibytes` raw HCI command bytes to send to the BlueTooth controller.
- `callback` optional function to be invoked when the reset completes. Its
  first argument is the HCI error code, or `nil` on success. The second
  argument contains any subsequent raw result bytes, or an empty string
  if the result only contained the status code.

##### Returns
`nil`

#### Example
```lua
-- Send a HCI reset command (it would be easier to use bthci.reset() though)
bthci.rawhci(encoder.fromHex("01030c00"), function(err) print(err or "Ok!"))
```

#### See also
[`encoder.fromHex()`](encoder.md#fromhex)
[`encoder.toHex()`](encoder.md#tohex)
[`struct.pack()`](struct.md#pack)



## bthci.reset(callback)

Resets the BlueTooth controller.

#### Syntax
`bthci.reset([callback])`

#### Parameters
- `callback` optional function to be invoked when the reset completes. Its
  only argument is the HCI error code, or `nil` on success.

#### Returns
`nil`

#### Example
```lua
bthci.reset(function(err) print(err or "Ok!") end)
```


## bthci.adv.enable(onoff, callback)

Enables or disables BlueTooth LE advertisements.

Before enabling advertisements, both parameters and data should be set.

#### Syntax
`bthci.adv.enable(onoff [, callback])`

#### Parameters
- `onoff` 1 or 0 to enable or disable advertisements, respectively.
- `callback` optional function to be invoked when the reset completes. Its
  only argument is the HCI error code, or `nil` on success.

##### Returns
`nil`

#### Example
```lua
bthci.adv.enable(1, function(err) print(err or "Ok!") end)
```


## bthci.adv.setdata(advbytes, callback)

Configures the data to advertise.

#### Syntax
`bthci.adv.setdata(advbytes [, callback])`

#### Parameters
- `advbytes` the raw bytes to advertise (up to 31 bytes), in the correct
  format (consult the BlueTooth specification for details).
- `callback` optional function to be invoked when the reset completes. Its
  only argument is the HCI error code, or `nil` on success.

##### Returns
`nil`

#### Example
```lua
-- Configure advertisements of a short name "abcdefg"
bthci.adv.setdata(encoder.fromHex("080861626364656667"), function(err) print(err or "Ok!") end)
```


## bthci.adv.setparams(paramtable, callback)

Configures advertisement parameters.

#### Syntax
`bthci.adv.setparams(paramtable [, callback])`

#### Parameters
- `paramtable` a table with zero or more of the following fields:
    - `interval_min` value in units of 0.625ms. Default 0x0400 (0.64s).
    - `interval_max` value in units of 0.625ms. Default 0x0800 (1.28s).
    - `type` advertising type, one of following constants:
        - `bthci.adv.CONN_UNDIR`, the default (ADV_IND in BT spec)
        - `bthci.adv.CONN_DIR_HI` (ADV_DIRECT_IND, high duty cycle in the BT spec)
        - `bthci.adv.SCAN_UNDIR` (ADV_SCAN_IND in the BT spec)
        - `bthci.adv.NONCONN_UNDIR` (ADV_NONCONN_IND in the BT spec)
        - `bthci.adv.CONN_DIR_LO` (ADV_DIRECT_IND, low duty cycle in the BT spec)
    - `own_addr_type` own address type. Default 0 (public address).
    - `peer_addr_type` peer address type. Default 0 (public address).
    - `peer_addr` TODO, not yet implemented
    - `channel_map` which channels to advertise on. The constants `bthci.adv.CHAN_37`, `bthci.adv.CHAN_38`, `bthci.adv.CHAN_39` or `bthci.adv.CHAN_ALL` may be used. Default is all channels.
    - `filter_policy` filter policy, default 0 (no filtering).
- `callback` optional function to be invoked when the reset completes. Its
  only argument is the HCI error code, or `nil` on success.

#### Returns
`nil`

#### Example
```lua
bthci.adv.setparams({type=bthci.adv.NONCONN_UNDIR}, function(err) print(err or "Ok!") end)
```


## bthci.scan.enable(onoff, callback)

Enables or disable scanning for advertisements from other BlueTooth devices.

#### Syntax
`bthci.scan.enable(onoff [, callback])`

#### Parameters
- `onoff` 1 or 0 to enable or disable advertisements, respectively.
- `callback` optional function to be invoked when the reset completes. Its
  only argument is the HCI error code, or `nil` on success.

##### Returns
`nil`

#### Example
```lua
bthci.scan.enable(1, function(err) print(err or "Ok!") end)
```


## bthci.scan.setparams(paramstable, callback)

Configures scan parameters.

Note that if configuring the scan window to be the same as the scan interval
this will fully occupy the radio and no other activity takes place.

#### Syntax
`bthci.scan.setparams(paramstable [, callback])`

#### Parameters
- `paramstable` a table with zero or more of the following fields:
    - `mode` scanning mode, 0 for passive, 1 for active. Default 0.
    - `interval` scanning interval in units of 0.625ms. Default 0x0010.
    - `window` length of scanning window in units of 0.625ms. Default 0x0010.
    - `own_addr_type` own address type. Default 0 (public).
    - `filter_policy` filtering policy. Default 0 (no filtering).
- `callback` optional function to be invoked when the reset completes. Its
  only argument is the HCI error code, or `nil` on success.

#### Returns
`nil`

#### Example
```lua
bthci.scan.setparams({mode=1,interval=40,window=20},function(err) print(err or "Ok!") end)
```

## bthci.scan.on(event, callback)

Registers the callback to be passed any received advertisements.

#### Syntax
`bthci.scan.on(event [, callback])`

#### Parameters
- `event` the string describing the event. Currently only "adv_report" is
  supported, to register for advertising reports.
- `callback` the callback function to receive the advertising reports, or
  `nil` to deregister the callback. This callback receives the raw bytes
  of the advertisement payload.

#### Returns
`nil`

#### Example
```lua
bthci.scan.on("adv_report", function(rep) print("ADV: "..encoder.toHex(rep))end)
```

#### See also
[`encoder.toHex()`](encoder.md#tohex)
[`struct.unpack()`](struct.md#unpack)


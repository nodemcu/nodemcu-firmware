# BT HCI Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2021-10-10 | [pjsg](https://github.com/pjsg) | [ble.c](../../components/modules/ble.c)|

The BLE module provides a simple interface to allow implementation of a simple GAP/GATT server.
This allows you to build simple gadgets that can be interrogated and controlled over BLE.

## ble.init(configuration)

This initializes the BlueTooth stack and starts advertising according to the data in the
configuration table. See below for a detailed description of this table.

#### Syntax
`ble.init(ble_config)`

#### Parameters
- `ble_config` A table with the keys as defined below.

##### Returns
`nil`

#### Example
```lua
local config = {name="MyGadget=", services={{uuid="0123456789abcdef", characteristics={{uuid="1234", value=0, type='c'}}}}}
ble.init(config)
```

## bthci.shutdown(callback)

Shuts down the BlueTooth controller and returns it to the state where another `init` can be performed.

#### Syntax
`ble.shutdown([callback])`

#### Parameters
- `callback` optional function to be invoked when the shutdown completes. Its
  only argument is an error code, or `nil` on success.

#### Returns
`nil`

#### Example
```lua
ble.shutdown(function(err) print(err or "Ok!") end)
```

## Conventions

## Configuration Table

The configuration table contains the following keys:

- `name` The name to use to advertise the gadget

- `services` This is a list of tables that define the individual services. The primary service is the first service. Many examples will only have a single service.


### Service table

The service table contains the following keys:

- `uuid` The UUID of the service. This is a 16 byte string (128 bits) that identifies the particular service. It can also be a two byte string for a well-known service.
- `characteristics` This is a list of tables, where each entry describes a characateristic (attribute)
- `preread` This is an optional function that is invoked just before a read of any characteristic in this service.
- `postwrite` This is an optional function that is invoked just after a write of any characteristic in this service.

### Characteristic table

The characteristic table contains the following keys:

- `uuid` The UUID of the characteristics. This can be either a 16 byte string or a 2 byte string that identifies the particular characteristic. Typically, 2 byte strings are used for well-known characteristics.
- `type` This is the optional type of the value. It has the same value as a unpack code in the `struct` module.
- `value` This is the actual value of the characteristic. This will be a string of bytes unless a `type` value is set.
- `read` This is a function that will be invoked to read the value (and so does not need the `value` entry). It should return a string of bytes (unless `type` is set)
- `write` This is a function that will be invoked to write the value (and so does not need the `value` entry). It is given a string of bytes (unless `type` is set)

The characteristics are treated as read/write unless only one of the `read` or `write` keys is present. 

The calling conventions for these functions are TBD.


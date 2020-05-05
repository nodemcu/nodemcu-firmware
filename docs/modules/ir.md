# IR Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2020-05-01 | [Steven Price](https://github.com/ecrips) | [Steven Price](https://github.com/ecrips) | [ir.c](../../app/modules/ir.c)|


This module provides functions to receive InfraRed (IR) signals from a IR receiver (e.g. TSOP38238). It can also decode RC-5 encoded signals.

## Functions

## ir.setup()
Setup the GPIO pin connected to the IR receiver. The GPIO pin will be configured to general interrupts on level changes. It returns a userdata handle to the IR device. When this is garbage collected the resources are freed. Currently only one IR device can be open at a time.

#### Syntax
`ir_dev = ir.setup([pin])`

#### Parameters
- `pin` Pin connected to the IR receiver. If omitted then the module is unregistered.

#### Returns
None

## ir.on()
Register a callback for when IR data is received. Two callbacks are available `raw` returns a table with the raw IR timing for decoding in Lua. `rc5` provides the decoded RC5 payload. Omitting the callback argument removes the callback.

#### Syntax
`ir_dev:on(event[, callback])`

#### Parameters
- event: Either `raw` or `rc5`
- callback: Function to call when IR data is received. The callback takes a single argument which is a table.

#### Returns
None

#### Example
```lua
ir_dev:on("raw", function(t)
	-- Handle raw IR data
end)
ir_dev:on("rc5", function(t)
	print(t.code, t.toggle, t.device, t.command)
end)
```

#### `raw` callback
The table passed to the `raw` callback is a simple array with the length(s) of the 'on' times and 'off' times. The least significant bit encodes whether the length is for an 'on' or 'off'. Lengths are in microseconds and are 16 bits (so periods longer than 65,535 microseconds are capped).

#### `rc5` callback
The table passed to the `rc5` callback has 4 fields. `code` is the raw code received (without the first start bit), including the toggle bit. `toggle` is a boolean which changes every time a button is pressed on the remote - this allows you to distinguish between key repeat and a button being repeatedly pressed. `device` and `command` are defined by the RC5 spec - in general all buttons on a remote will have the same 'device' but different 'commands'.

# wiegand26

This module allows you to communicate via the wiegand protocol. Currently only the 26 bit protocol is supported.

## Example

```lua
dataPin1 = 2
dataPin2 = 3

wiegand = require("wiegand")
wiegand.setup(dataPin1, dataPin2)

do
	keyCode = wiegand.read()
	if keyCode then
		print("Site: "..keyCode[1])
		print("Serial: "..keyCode[2])
	end
end
```

## Todo

Use the event emitter to fire event when the module successfully reads a RFID tag.

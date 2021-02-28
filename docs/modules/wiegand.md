# wiegand Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2020-07-08 | [Cody Cutrer](https://github.com/ccutrer) | [Cody Cutrer](https://github.com/ccutrer) | [wiegand.c](../../app/modules/wiegand.c)|

This module can read the input from RFID/keypad readers that support Wiegand outputs. 4 (keypress) and 26 (Wiegand standard) bit formats are supported. Wiegand requires three connections - two GPIOs connected to D0 and D1 datalines, and a ground connection.

## wiegand.create()
Creates a dynamic wiegand object that receives a callback when data is received.
Initialize the nodemcu to talk to a Wiegand keypad

#### Syntax
`wiegand.create(pinD0, pinD1, callback)`

#### Parameters
- `pinD0` This is a GPIO number (excluding 0) and connects to the D0 data line
- `pinD1` This is a GPIO number (excluding 0) and connects to the D1 data line
- `callback` This is a function that will invoked when a full card or keypress is read.

The callback will be invoked with two arguments when a card is received. The first argument is the received code,
the second is the number of bits in the format (4, 26). For 4-bit format, it's just an integer of the key they
pressed; * is 10, and # is 11. For 26-bit format, it's the raw code. If you want to separate it into site codes
and card numbers, you'll need to do the arithmetic yourself (top 8 bits are site code; bottom 16 are card
numbers).

#### Returns
`wiegand` object. If the arguments are in error, or the operation cannot be completed, then an error is thrown.

#### Example

    local w = wiegand.create(1, 2, function (card, bits)
      print("Card=" .. card .. " bits=" .. bits)
    end)
    w:close()

# Wiegand Object Methods

## wiegandobj:close()
Releases the resources associated with the card reader.

#### Syntax
`wiegandobj:close()`

#### Example

    wiegandobj:close()

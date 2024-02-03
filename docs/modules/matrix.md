# matrix Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2024-02-01 | [Philip Gladstone](https://github.com/pjsg) | [Philip Gladstone](https://github.com/pjsg) | [matrix.c](../../components/modules/matrix.c)|


This module processes key presses on matrixed keyboards such as cheap numeric keypads with the # and * keys. These are organized as a 3x4 matrix with 7 connections
in all.

## Sources for parts

- Amazon: This [search](http://www.amazon.com/s/ref=nb_sb_noss_1?url=search-alias%3Dindustrial&field-keywords=rotary+encoder+push+button&rh=n%3A16310091%2Ck%3Arotary+encoder+push+button) shows a variety.
- Ebay: Somewhat cheaper in this [search](http://www.ebay.com/sch/i.html?_from=R40&_trksid=p2050601.m570.l1313.TR0.TRC0.H0.Xrotary+encoder+push+button.TRS0&_nkw=rotary+encoder+push+button&_sacat=0)
- Adafruit: [rotary encoder](https://www.adafruit.com/products/377)
- Aliexpress: This [search](http://www.aliexpress.com/wholesale?catId=0&initiative_id=SB_20160217173657&SearchText=rotary+encoder+push+button) reveals all sorts of shapes and sizes.

## Constants
- `matrix.PRESS = 1` The eventtype for a keyboard key press
- `matrix.RELEASE = 2` The eventtype for keyboard key release.
- `matrix.ALL = 3` Covers all event types

## matrix.setup()
Initialize the nodemcu to talk to a matrixed keyboard.

#### Syntax
`keyboard = matrix.setup({column pins}, {row pins}, {key characters})`

#### Parameters
- `column pins` These are the GPIO numbers of the pins connected to the columns of the keyboard
- `row pins` These are the GPIO numbers of the pins connected to the rows of the keyboard
- `key characters` These are the characters (or strings) to be returned when a key is pressed. The first character corresponds to the first row and first column. The next character is the second column and first row, etc.

#### Returns
The keyboard object. 


#### Example

    keyboard = matrix.setup({5,6,7}, {8,9,10,11}, { "1", "2", "3", "4", "5", "6", "7", "8", "9", "#", "0", "*"})

#### Notes
If an entry in the key characters table is nil, then that key press will not be reported.

## keyboard:on()
Sets a callback on specific events.

#### Syntax
`keyboard:on(eventtype[, callback])`

#### Parameters
- `eventtype` This defines the type of event being registered. This can be one or more of `matrix.PRESS` and `matrix.RELEASE`.
- `callback` This is a function that will be invoked when the specified event happens.

If the callback is None or omitted, then the registration is cancelled.

The callback will be invoked with three arguments when the event happens. The first argument is the eventtype,
the second is the character, and the third is the time when the event happened.

The time is the number of microseconds represented in a 32-bit integer. Note that this wraps every hour or so.

#### Example

    keyboard:on(matrix.ALL, function (type, char, when)
      print("Character=" .. char .. " event type=" .. type .. " time=" .. when)
    end)

#### Errors
If an invalid `eventtype` is supplied, then an error will be thrown.

## keyboard:close()
Releases the resources associated with the matrix keyboard.

#### Syntax
`keyboard:close()`

#### Example

    keyboard:close()


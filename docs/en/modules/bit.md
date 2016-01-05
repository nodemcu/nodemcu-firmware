# bit Module

Bit manipulation support, on 32bit integers.

## bit.bnot()

Bitwise negation, equivalent to ~value in C.

####Syntax
`bit.bnot(value)`

####Parameters
value: the number to negate.

####Returns
number: the bitwise negated value of the number.
___
## bit.band()

Bitwise AND, equivalent to val1 & val2 & ... & valn in C.

####Syntax
`bit.band(val1, val2 [, ... valn])`

####Parameters
 - `val1`: first AND argument.
 - `val2`: second AND argument.
 - `...valn`: ...nth AND argument.

####Returns
number: the bitwise AND of all the arguments.
___
## bit.bor()
Bitwise OR, equivalent to val1 | val2 | ... | valn in C.

####Syntax
`bit.bor(val1, val2 [, ... valn])`

####Parameters
  - `val1`: first OR argument.
  - `val2`: second OR argument.
  - `...valn`: ...nth OR argument.

####Returns
number: the bitwise OR of all the arguments.
___
## bit.bxor()

Bitwise XOR, equivalent to val1 ^ val2 ^ ... ^ valn in C.

####Syntax
`bit.bxor(val1, val2 [, ... valn])`

####Parameters
  - `val1`: first XOR argument.
  - `val2`: second XOR argument.
  - `...valn`: ...nth XOR argument.

####Returns
number: the bitwise XOR of all the arguments.
___
## bit.lshift()
Left-shift a number, equivalent to value << shift in C.

####Syntax
`bit.lshift(value, shift)`

####Parameters
  - `value`: the value to shift.
  - `shift`: positions to shift.

####Returns
number: the number shifted left
___
## bit.rshift()

Logical right shift a number, equivalent to ( unsigned )value >> shift in C.

####Syntax
`bit.rshift(value, shift)`

####Parameters
  - `value`: the value to shift.
  - `shift`: positions to shift.

####Returns
number: the number shifted right (logically).
___
## bit.arshift()
Arithmetic right shift a number equivalent to value >> shift in C.

####Syntax
`bit.arshift(value, shift)`

####Parameters
  - `value`: the value to shift.
  - `shift`: positions to shift.

####Returns
number: the number shifted right (arithmetically).
___
## bit.bit()

Generate a number with a 1 bit (used for mask generation). Equivalent to 1 << position in C.

####Syntax
`bit.bit(position)`

####Parameters
  - `position`: position of the bit that will be set to 1.

####Returns
number: a number with only one 1 bit at position (the rest are set to 0).
___
## bit.set()

Set bits in a number.

####Syntax
`bit.set(value, pos1 [, ... posn ])`

####Parameters
  - `value`: the base number.
  - `pos1`: position of the first bit to set.
  - `...posn`: position of the nth bit to set.

####Returns
number: the number with the bit(s) set in the given position(s).
___
## bit.clear()
Clear bits in a number.

####Syntax
`bit.clear(value, pos1 [, ... posn])`

####Parameters
  - `value`: the base number.
  - `pos1`: position of the first bit to clear.
  - `...posn`: position of thet nth bit to clear.

####Returns
number: the number with the bit(s) cleared in the given position(s).
___
## bit.isset()

Test if a given bit is set.

####Syntax
`bit.isset(value, position)`

####Parameters
  - `value`: the value to test.
  - `position`: bit position to test.

####Returns
boolean: true if the bit at the given position is 1, false otherwise.
___
## bit.isclear()

Test if a given bit is cleared.

####Syntax
`bit.isclear(value, position)`

####Parameters
  - `value`: the value to test.
  - `position`: bit position to test.

####Returns
boolean: true if the bit at the given position is 0, false othewise.
___

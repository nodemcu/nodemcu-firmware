# rotary Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-03-01 | [Philip Gladstone](https://github.com/pjsg) | [Philip Gladstone](https://github.com/pjsg) | [rotary.c](../../../app/modules/rotary.c)|


This module can read the state of cheap rotary encoder switches. These are available at all the standard places for a dollar or two. They are five pin devices where three are used for a gray code encoder for rotation, and two are used for the push switch. These switches are commonly used in car audio systems. 

These switches do not have absolute positioning, but only encode the number of positions rotated clockwise / anti-clockwise. To make use of this module, connect the common pin on the quadrature encoder to ground and the A and B phases to the NodeMCU. One pin of the push switch should also be grounded and the other pin connected to the NodeMCU.

## Sources for parts

- Amazon: This [search](http://www.amazon.com/s/ref=nb_sb_noss_1?url=search-alias%3Dindustrial&field-keywords=rotary+encoder+push+button&rh=n%3A16310091%2Ck%3Arotary+encoder+push+button) shows a variety.
- Ebay: Somewhat cheaper in this [search](http://www.ebay.com/sch/i.html?_from=R40&_trksid=p2050601.m570.l1313.TR0.TRC0.H0.Xrotary+encoder+push+button.TRS0&_nkw=rotary+encoder+push+button&_sacat=0)
- Adafruit: [rotary encoder](https://www.adafruit.com/products/377)
- Aliexpress: This [search](http://www.aliexpress.com/wholesale?catId=0&initiative_id=SB_20160217173657&SearchText=rotary+encoder+push+button) reveals all sorts of shapes and sizes.

There is also a switch mounted on a board with standard 0.1" pins. 
This is the KY-040, and can also be found at [lots of places](https://www.google.com/webhp?sourceid=chrome-instant&ion=1&espv=2&ie=UTF-8#q=ky-040%20rotary%20encoder). 
Note that the pins are named somewhat eccentrically, and I suspect that it really does need the VCC connected.

## Constants
- `rotary.PRESS = 1` The eventtype for the switch press.
- `rotary.LONGPRESS = 2` The eventtype for a long press.
- `rotary.RELEASE = 4` The eventtype for the switch release.
- `rotary.TURN = 8` The eventtype for the switch rotation.
- `rotary.CLICK = 16` The eventtype for a single click (after release)
- `rotary.DBLCLICK = 32` The eventtype for a double click (after second release)
- `rotary.ALL = 63` All event types.

## rotary.setup()
Initialize the nodemcu to talk to a rotary encoder switch.

#### Syntax
`rotary.setup(channel, pina, pinb[, pinpress[, longpress_time_ms[, dblclick_time_ms]]])`

#### Parameters
- `channel` The rotary module supports three switches. The channel is either 0, 1 or 2.
- `pina` This is a GPIO number (excluding 0) and connects to pin phase A on the rotary switch.
- `pinb` This is a GPIO number (excluding 0) and connects to pin phase B on the rotary switch.
- `pinpress` (optional) This is a GPIO number (excluding 0) and connects to the press switch.
- `longpress_time_ms` (optional) The number of milliseconds (default 500) of press to be considered a long press.
- `dblclick_time_ms` (optional) The number of milliseconds (default 500) between a release and a press for the next release to be considered a double click.

#### Returns
Nothing. If the arguments are in error, or the operation cannot be completed, then an error is thrown.

For all API calls, if the channel number is out of range, then an error will be thrown.

#### Example

    rotary.setup(0, 5,6, 7)

## rotary.on()
Sets a callback on specific events.

#### Syntax
`rotary.on(channel, eventtype[, callback])`

#### Parameters
- `channel` The rotary module supports three switches. The channel is either 0, 1 or 2.
- `eventtype` This defines the type of event being registered. This is the logical or of one or more of `PRESS`, `LONGPRESS`, `RELEASE`, `TURN`, `CLICK` or `DBLCLICK`.
- `callback` This is a function that will be invoked when the specified event happens. 

If the callback is None or omitted, then the registration is cancelled.

The callback will be invoked with three arguments when the event happens. The first argument is the eventtype, 
the second is the current position of the rotary switch, and the third is the time when the event happened. 

The position is tracked
and is represented as a signed 32-bit integer. Increasing values indicate clockwise motion. The time is the number of microseconds represented
in a 32-bit integer. Note that this wraps every hour or so.

#### Example

    rotary.on(0, rotary.ALL, function (type, pos, when) 
      print "Position=" .. pos .. " event type=" .. type .. " time=" .. when
    end)

#### Notes

Events will be delivered in order, but there may be missing TURN events. If there is a long 
queue of events, then PRESS and RELEASE events may also be missed. Multiple pending TURN events 
are typically dispatched as one TURN callback with the final position as its parameter.

Some switches have 4 steps per detent. This means that, in practice, the application
should divide the position by 4 and use that to determine the number of clicks. It is
unlikely that a switch will ever reach 30 bits of rotation in either direction -- some
are rated for under 50,000 revolutions.

The `CLICK` and `LONGPRESS` events are delivered on a timeout. The `DBLCLICK` event is delivered after a `PRESS`, `RELEASE`, `PRESS`, `RELEASE` sequence
where this is a short time gap between the middle `RELEASE` and `PRESS`.

#### Errors
If an invalid `eventtype` is supplied, then an error will be thrown.

## rotary.getpos()
Gets the current position and press status of the switch

#### Syntax
`pos, press, queue = rotary.getpos(channel)`

#### Parameters
- `channel` The rotary module supports three switches. The channel is either 0, 1 or 2.

#### Returns
- `pos` The current position of the switch.
- `press` A boolean indicating if the switch is currently pressed.
- `queue` The number of undelivered callbacks (normally 0).

#### Example

    print rotary.getpos(0)

## rotary.close()
Releases the resources associated with the rotary switch.

#### Syntax
`rotary.close(channel)`

#### Parameters
- `channel` The rotary module supports three switches. The channel is either 0, 1 or 2.

#### Example

    rotary.close(0)


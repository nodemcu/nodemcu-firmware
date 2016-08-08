# Switec Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-06-26 |[Philip Gladstone](https://github.com/pjsg) | [Philip Gladstone](https://github.com/pjsg) | [switec.c](../../../app/modules/switec.c)|

This module controls a [Switec X.27](http://www.jukenswisstech.com/?page_id=103) (or compatible) instrument stepper motor. These are the 
stepper motors that are used in modern automotive instrument clusters. They are incredibly cheap
and can be found at your favorite auction site or Chinese shopping site. There are varieties
which are dual axis -- i.e. have two stepper motors driving two concentric shafts so you 
can mount two needles from the same axis.

These motors run off 5V (some may work off 3.3V). They draw under 20mA and are designed to be
driven directly from MCU pins. Since the nodemcu runs at 3.3V, a level translator is required.
An octal translator like the [74LVC4245A](http://www.nxp.com/products/discretes-and-logic/logic/voltage-level-translators/octal-dual-supply-translating-transceiver-3-state-based-on-pip-74lvc4245a:74LVC4245A) can perfom this translation. It also includes all the
protection diodes required. 

These motors can be driven off three pins, with `pin2` and `pin3` being the same GPIO pin. 
If the motor is directly connected to the MCU, then the current load is doubled and may exceed
the maximum ratings. If, however, a driver chip is being used, then the load on the MCU is negligible
and the same MCU pin can be connected to two driver pins. In order to do this, just specify
the same pin for `pin2` and `pin3`.

These motors do not have absolute positioning, but come with stops at both ends of the range.
The startup procedure is to drive the motor anti-clockwise until it is guaranteed that the needle
is on the step. Then this point can be set as zero. It is important not to let the motor
run into the endstops during normal operation as this will make the pointing inaccurate. 
This module does not enforce any range limiting.

!!! important

    This module uses the hardware timer interrupt and hence it cannot be used at the same time as the PWM module. Both modules can be compiled into the same firmware image, but an application can only use one. It may be possible for an application to alternate between `switec` and `pwm`, but care must be taken.

## switec.setup()
Initialize the nodemcu to talk to a switec X.27 or compatible instrument stepper motor. The default
slew rate is set so that it should work for most motors. Some motors can run at 600 degress per second. 

#### Syntax
`switec.setup(channel, pin1, pin2, pin3, pin4 [, maxDegPerSec])`

#### Parameters
- `channel` The switec module supports three stepper motors. The channel is either 0, 1 or 2.
- `pin1` This is a GPIO number and connects to pin 1 on the stepper.
- `pin2` This is a GPIO number and connects to pin 2 on the stepper.
- `pin3` This is a GPIO number and connects to pin 3 on the stepper.
- `pin4` This is a GPIO number and connects to pin 4 on the stepper.
- `maxDegPerSec` (optional) This can set to limit the maximum slew rate. The default is 400 degrees per second.

#### Returns
Nothing. If the arguments are in error, or the operation cannot be completed, then an error is thrown.

##### Note

Once a channel is setup, it cannot be re-setup until the needle has stopped moving. 

#### Example

```lua
switec.setup(0, 5, 6, 7, 8)
```

## switec.moveto()
Starts the needle moving to the specified position. If the needle is already moving, then the current
motion is cancelled, and the needle will move to the new position. It is possible to get a callback
when the needle stops moving. This is not normally required as multiple `moveto` operations can
be issued in quick succession. During the initial calibration, it is important. Note that the 
callback is not guaranteed to be called -- it is possible that the needle never stops at the
target location before another `moveto` operation is triggered.

#### Syntax
`switec.moveto(channel, position[, stoppedCallback)`

#### Parameters
- `channel` The switec module supports three stepper motors. The channel is either 0, 1 or 2.
- `position` The position (number of steps clockwise) to move the needle. Typically in the range 0 to around 1000.
- `stoppedCallback` (optional) callback to be invoked when the needle stops moving.

#### Errors
The channel must have been setup, otherwise an error is thrown.

#### Example

```lua
switec.moveto(0, 1000, function ()
    switec.moveto(0, 0)
end)
```

## switec.reset()
This sets the current position of the needle as being zero. The needle must be stationary.

#### Syntax
`switec.reset(channel)`

#### Parameters
- `channel` The switec module supports three stepper motors. The channel is either 0, 1 or 2.

#### Errors
The channel must have been setup and the needle must not be moving, otherwise an error is thrown.

## switec.getpos()
Gets the current position of the needle and whether it is moving.

#### Syntax
`switec.getpos(channel)`

#### Parameters
- `channel` The switec module supports three stepper motors. The channel is either 0, 1 or 2.

#### Returns
- `position` the current position of the needle
- `moving` 0 if the needle is stationary. 1 for clockwise, -1 for anti-clockwise.

## switec.close()
Releases the resources associated with the stepper.

#### Syntax
`switec.close(channel)`

#### Parameters
- `channel` The switec module supports three stepper motors. The channel is either 0, 1 or 2.

#### Errors
The needle must not be moving, otherwise an error is thrown.

## Calibration
In order to set the zero point correctly, the needle should be driven anti-clockwise until
it runs into the end stop. Then the zero point can be set. The value of -1000 is used as that is
larger than the range of the motor -- i.e. it drives anti-clockwise through the entire range and 
onto the end stop.

    switec.setup(0, 5,6,7,8)
    calibration = true
    switec.moveto(0, -1000, function() 
      switec.reset(0)
      calibration = false
    end)

Other `moveto` operations should not be performed while `calibration` is set.

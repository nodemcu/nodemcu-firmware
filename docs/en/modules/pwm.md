# PWM Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-22 | [Zeroday](https://github.com/funshine) | [Zeroday](https://github.com/funshine) | [pwm.c](../../../app/modules/pwm.c)|

## pwm.close()
Quit PWM mode for the specified GPIO pin.

#### Syntax
`pwm.close(pin)`

#### Parameters
`pin` 1~12, IO index

#### Returns
`nil`

#### See also
[pwm.start()](#pwmstart)

## pwm.getclock()
Get selected PWM frequency of pin.

#### Syntax
`pwm.getclock(pin)`

#### Parameters
`pin` 1~12, IO index

#### Returns
`number` PWM frequency of pin

#### See also
[pwm.setclock()](#pwmsetclock)

#### See also
[pwm.getduty()](#pwmgetduty)

## pwm.getduty()
Get selected duty cycle of pin.

#### Syntax
`pwm.getduty(pin)`

#### Parameters
`pin` 1~12, IO index

#### Returns
`number` duty cycle, max 1023

#### See also
[pwm.setduty()](#pwmsetduty)

## pwm.setclock()
Set PWM frequency.
**Note:** Setup of the PWM frequency will synchronously change other setups as well if there are any. Only one PWM frequency can be allowed for the system.

#### Syntax
`pwm.setclock(pin, clock)`

#### Parameters
- `pin` 1~12, IO index
- `clock` 1~1000, PWM frequency

#### Returns
`nil`

#### See also
[pwm.getclock()](#pwmgetclock)

## pwm.setduty()
Set duty cycle for a pin.

#### Syntax
`pwm.setduty(pin, duty)`

#### Parameters
- `pin` 1~12, IO index
- `duty` 0~1023, pwm duty cycle, max 1023 (10bit)

#### Returns
`nil`

#### Example
```lua
-- D1 is connected to green led
-- D2 is connected to blue led
-- D3 is connected to red led
pwm.setup(1, 500, 512)
pwm.setup(2, 500, 512)
pwm.setup(3, 500, 512)
pwm.start(1)
pwm.start(2)
pwm.start(3)
function led(r, g, b)
    pwm.setduty(1, g)
    pwm.setduty(2, b)
    pwm.setduty(3, r)
end
led(512, 0, 0) --  set led to red
led(0, 0, 512) -- set led to blue.
```

## pwm.setup()
Set pin to PWM mode. Only 6 pins can be set to PWM mode at the most.

#### Syntax
`pwm.setup(pin, clock, duty)`

#### Parameters
- `pin` 1~12, IO index
- `clock` 1~1000, pwm frequency
- `duty` 0~1023, pwm duty cycle, max 1023 (10bit)

#### Returns
`nil`

#### Example
```lua
-- set pin index 1 as pwm output, frequency is 100Hz, duty cycle is half.
pwm.setup(1, 100, 512)
```

#### See also
[pwm.start()](#pwmstart)

## pwm.start()
PWM starts, the waveform is applied to the GPIO pin.

#### Syntax
`pwm.start(pin)`

####Parameters
`pin` 1~12, IO index

#### Returns
`nil`

#### See also
[pwm.stop()](#pwmstop)

## pwm.stop()
Pause the output of the PWM waveform.

#### Syntax
`pwm.stop(pin)`

#### Parameters
`pin` 1~12, IO index

#### Returns
`nil`

#### See also
[pwm.start()](#pwmstart)

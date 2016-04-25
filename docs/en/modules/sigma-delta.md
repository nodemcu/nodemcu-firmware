# Sigma-delta Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-02-20 | [Espressif example](http://bbs.espressif.com/viewtopic.php?t=49), [Arnim Läuger](https://github.com/devsaurus) | [Arnim Läuger](https://github.com/devsaurus) | [sigma_delta.c](../../../app/modules/sigma_delta.c)|

This module provides access to the [sigma-delta](https://en.wikipedia.org/wiki/Delta-sigma_modulation) component. It's a hardware signal generator that can be routed to any of the GPIOs except pin 0.

The signal generation is controlled by the [`setprescale()`](#sigma_deltasetprescale) and [`settarget()`](#sigma_deltasettarget) functions.

  - 0 < target <= 128<br />
    t<sub>high</sub> = (prescale + 1) / 80 µs<br />
    t<sub>period</sub>  = t<sub>high</sub> * 256 / target
  - 128 < target < 256<br />
    t<sub>low</sub>  = (prescale + 1) / 80 µs<br />
    t<sub>period</sub> = t<sub>low</sub> * 256 / (256 - target)
  - target = 0<br />
    signal stopped at low

Fixed frequency PWM at ~312.5&nbsp;kHz is availble with the [`setpwmduty()`](#sigma_deltasetpwmduty) function.

## sigma_delta.close()
Stops signal generation and reenables GPIO functionality at the specified pin.

#### Syntax
`sigma_delta.close(pin)`

#### Parameters
`pin` 1~12, IO index

#### Returns
`nil`

## sigma_delta.setprescale()
Sets the prescale value.

#### Syntax
`sigma_delta.setprescale(value)

#### Parameters
`value` prescale 1 to 255

#### Returns
`nil`

#### See also
[`sigma_delta.settarget()`](#sigma_deltasettarget)

## sigma_delta.setpwmduty()
Operate the sigma-delta module in PWM-like mode with fixed base frequency.

#### Syntax
`sigma_delta.setpwmduty(ratio)`

#### Parameters
`ratio` 0...255 for duty cycle 0...100%, 0 stops the signal at low

#### Returns
`nil`

#### Example
```lua
-- attach generator to pin 2
sigma_delta.setup(2)
-- set 50% duty cycle ratio (and implicitly start signal)
sigma_delta.setpwmduty(128)
-- stop
sigma_delta.setpwmduty(0)
-- resume with ~99.6% ratio
sigma_delta.setpwmduty(255)
-- stop and detach generator from pin 2
sigma_delta.close(2)
```

## sigma_delta.settarget()
Sets the target value.

#### Syntax
`sigma_delta.settarget(value)`

#### Parameters
`value` target 0 to 255

#### Returns
`nil`

#### See also
[`sigma_delta.setprescale()`](#sigma_deltasetprescale)

## sigma_delta.setup()
Stops the signal generator and routes it to the specified pin.

#### Syntax
`sigma_delta.setup(pin)`

#### Parameters
`pin` 1~12, IO index

#### Returns
`nil`

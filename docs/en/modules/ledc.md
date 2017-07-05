# LEDC Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-07-05 | [Lars Stenberg](https://github.com/larsstenberg) | [Lars Stenberg](https://github.com/larsstenberg) | [ledc.c](../../../components/modules/ledc.c)|


This module provides access to the [LEDC](http://esp-idf.readthedocs.io/en/latest/api-reference/peripherals/ledc.html) PWM driver.

# LEDC Overview
The LED control module is primarily designed to control the intensity of LEDs, although it can be used to generate PWM signals for other purposes as well. It has 16 channels which can generate independent waveforms that can be used to drive e.g. RGB LED devices. For maximum flexibility, the high-speed as well as the low-speed channels can be driven from one of four high-speed/low-speed timers. The PWM controller also has the ability to automatically increase or decrease the duty cycle gradually, allowing for fades without any processor interference.


## ledc.config()
Configures a PIN to be controlled by the LEDC system.

#### Syntax
```lua
ledc.setup({
  gpio=x,
  bits=ledc.TIMER_10_BIT || ledc.TIMER_11_BIT || ledc.TIMER_12_BIT || ledc.TIMER_13_BIT || ledc.TIMER_14_BIT || ledc.TIMER_15_BIT,
  mode=ledc.HIGH_SPEED || ledc.LOW_SPEED,
  timer=ledc.TIMER_0 || ledc.TIMER_1 || ledc.TIMER_2 || ledc.TIMER_3,
  channel=ledc.CHANNEL_0 || ledc.CHANNEL_1 || ledc.CHANNEL_2 || ledc.CHANNEL_3 || ledc.CHANNEL_4 || ledc.CHANNEL_5 || ledc.CHANNEL_6 || ledc.CHANNEL_7,
  frequency=x,
  duty=x
});
```

#### Parameters
List of configuration tables:

- `gpio` one or more (given as list) pins, see [GPIO Overview](../gpio/#gpio-overview)
- `bits` Channel duty depth. Defaults to ledc.TIMER_13_BIT. Otherwise one of
    - `ledc.TIMER_10_BIT`
    - ...
    - `ledc.TIMER_15_BIT`
- `mode` High-speed mode or low-speed mode
    - `ledc.HIGH_SPEED`
    - `ledc.LOW_SPEED`
- `timer` The timer source of channel
    - `ledc.TIMER_0`
    - ...
    - `ledc.TIMER_3`
- `channel` The timer source of channel
    - `ledc.CHANNEL_0`
    - ...
    - `ledc.CHANNEL_7`
- `frequency` Timer frequency(Hz)
- `duty` Channel duty, the duty range is [0, (2**bit_num) - 1]. Example: if ledc.TIMER_13_BIT is used maximum value is 4096 x 2 -1 = 8091

#### Returns
`nil`

#### Example
```lua
ledc.setup({
  gpio=19,
  bits=ledc.TIMER_13_BIT,
  mode=ledc.HIGH_SPEED,
  timer=ledc.TIMER_0,
  channel=ledc.CHANNEL_0,
  frequency=1000,
  duty=4096
});
```

## ledc.stop()
Disable LEDC output, and set idle level.

#### Syntax
`ledc.stop(mode, channel, idleLevel)`

#### Parameters
- `mode` High-speed mode or low-speed mode
    - `ledc.HIGH_SPEED`
    - `ledc.LOW_SPEED`
- `channel` The timer source of channel
    - `ledc.CHANNEL_0`
    - ...
    - `ledc.CHANNEL_7`
- `idleLevel` Set output idle level after LEDC stops.
    - `ledc.IDLE_LOW`
    - `ledc.IDLE_HIGH`

#### Returns
nil

#### Example
```lua
ledc.stop(ledc.HIGH_SPEED, ledc.CHANNEL_0, ledc.IDLE_LOW)
```

## ledc.setfreq()
Set channel frequency (Hz)

#### Syntax
`ledc.setfreq(mode, channel, frequency)`

#### Parameters
- `mode` High-speed mode or low-speed mode
    - `ledc.HIGH_SPEED`
    - `ledc.LOW_SPEED`
- `channel` The timer source of channel
    - `ledc.CHANNEL_0`
    - ...
    - `ledc.CHANNEL_7`
- `frequency` What frequency should be set

#### Returns
nil

#### Example
```lua
ledc.setfreq(ledc.HIGH_SPEED, ledc.CHANNEL_0, 2000)
```


## ledc.getfreq()
Get channel frequency (Hz)

#### Syntax
`ledc.getfreq(mode, channel)`

#### Parameters
- `mode` High-speed mode or low-speed mode
    - `ledc.HIGH_SPEED`
    - `ledc.LOW_SPEED`
- `channel` The timer source of channel
    - `ledc.CHANNEL_0`
    - ...
    - `ledc.CHANNEL_7`

#### Returns
- 0 error
- Others current LEDC frequency

#### Example
```lua
ledc.getfreq(ledc.HIGH_SPEED, ledc.CHANNEL_0)
```


## ledc.setduty()
Set channel duty

#### Syntax
`ledc.setduty(mode, channel, duty)`

#### Parameters
- `mode` High-speed mode or low-speed mode
    - `ledc.HIGH_SPEED`
    - `ledc.LOW_SPEED`
- `channel` The timer source of channel
    - `ledc.CHANNEL_0`
    - ...
    - `ledc.CHANNEL_7`
- `duty` What duty should be set

#### Returns
nil

#### Example
```lua
ledc.setduty(ledc.HIGH_SPEED, ledc.CHANNEL_0, 4096)
```


## ledc.getduty()
Get channel duty

#### Syntax
`ledc.getduty(mode, channel)`

#### Parameters
- `mode` High-speed mode or low-speed mode
    - `ledc.HIGH_SPEED`
    - `ledc.LOW_SPEED`
- `channel` The timer source of channel
    - `ledc.CHANNEL_0`
    - ...
    - `ledc.CHANNEL_7`

#### Returns
- (-1) parameter error
- Others current LEDC duty

#### Example
```lua
ledc.getduty(ledc.HIGH_SPEED, ledc.CHANNEL_0)
```


## ledc.reset()
Resets the timer

#### Syntax
`ledc.reset(mode, timer)`

#### Parameters
- `mode` High-speed mode or low-speed mode
    - `ledc.HIGH_SPEED`
    - `ledc.LOW_SPEED`
- `timer` The timer source of channel
    - `ledc.TIMER_0`
    - ...
    - `ledc.TIMER_3`

#### Returns
nil


#### Example
```lua
ledc.reset(ledc.HIGH_SPEED, ledc.TIMER_0);
```


## ledc.pause()
Pauses the timer

#### Syntax
`ledc.pause(mode, timer)`

#### Parameters
- `mode` High-speed mode or low-speed mode
    - `ledc.HIGH_SPEED`
    - `ledc.LOW_SPEED`
- `timer` The timer source of channel
    - `ledc.TIMER_0`
    - ...
    - `ledc.TIMER_3`

#### Returns
nil

#### Example
```lua
ledc.pause(ledc.HIGH_SPEED, ledc.TIMER_0);
```


## ledc.resume()
Resumes a paused timer

#### Syntax
`ledc.resume(mode, timer)`

#### Parameters
- `mode` High-speed mode or low-speed mode
    - `ledc.HIGH_SPEED`
    - `ledc.LOW_SPEED`
- `timer` The timer source of channel
    - `ledc.TIMER_0`
    - ...
    - `ledc.TIMER_3`

#### Returns
nil

#### Example
```lua
ledc.resume(ledc.HIGH_SPEED, ledc.TIMER_0)
```


## ledc.fadewithtime()
Set LEDC fade function, with a limited time.

#### Syntax
`ledc.fadewithtime(mode, channel, targetDuty, maxFadeTime[, wait])`

#### Parameters
- `mode` High-speed mode or low-speed mode
    - `ledc.HIGH_SPEED`
    - `ledc.LOW_SPEED`
- `channel` The timer source of channel
    - `ledc.CHANNEL_0`
    - ...
    - `ledc.CHANNEL_7`
- `targetDuty` Target duty of fading.
- `maxFadeTime`  The maximum time of the fading ( ms ).
- `wait` Whether to block until fading done.
    - `ledc.FADE_NO_WAIT` (default)
    - `ledc.FADE_WAIT_DONE`

#### Returns
nil

#### Example
```lua
ledc.fadewithtime(ledc.HIGH_SPEED, ledc.CHANNEL_0, 4096, 1000);
```


## ledc.fadewithstep()
Set LEDC fade function, with step.

#### Syntax
`ledc.fadewithstep(mode, channel, targetDuty, scale, cycleNum[, wait])`

#### Parameters
- `mode` High-speed mode or low-speed mode
    - `ledc.HIGH_SPEED`
    - `ledc.LOW_SPEED`
- `channel` The timer source of channel
    - `ledc.CHANNEL_0`
    - ...
    - `ledc.CHANNEL_7`
- `targetDuty` Target duty of fading.
- `scale` Controls the increase or decrease step scale.
- `cycleNum` Increase or decrease the duty every cycle_num cycles
- `wait` Whether to block until fading done.
    - `ledc.FADE_NO_WAIT` (default)
    - `ledc.FADE_WAIT_DONE`

#### Returns
nil

#### Example
```lua
ledc.fadewithstep(ledc.HIGH_SPEED, ledc.CHANNEL_0, 1000, 10, 10);
```


## ledc.fade()
Set LEDC fade function.

#### Syntax
`ledc.fadewithstep(mode, channel, duty, direction, scale, cycleNum, stepNum [, wait])`

#### Parameters
- `mode` High-speed mode or low-speed mode
    - `ledc.HIGH_SPEED`
    - `ledc.LOW_SPEED`
- `channel` The timer source of channel
    - `ledc.CHANNEL_0`
    - ...
    - `ledc.CHANNEL_7`
- `duty` Set the start of the gradient duty.
- `direction` Set the direction of the gradient.
  - `ledc.FADE_DECREASE`
  - `ledc.FADE_INCREASE`
- `scale` Set gradient change amplitude.
- `cycleNum` Set how many LEDC tick each time the gradient lasts.
- `stepNum` Set the number of the gradient.
- `wait` Whether to block until fading done.
    - `ledc.FADE_NO_WAIT` (default)
    - `ledc.FADE_WAIT_DONE`

#### Returns
nil

#### Example
```lua
ledc.fade(ledc.HIGH_SPEED, ledc.CHANNEL_0, 0, ledc.FADE_INCREASE, 100, 100, 1000);
```

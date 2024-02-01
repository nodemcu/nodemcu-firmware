# LEDC Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-07-05 | [Lars Stenberg](https://github.com/larsstenberg) | [Lars Stenberg](https://github.com/larsstenberg) | [ledc.c](../../components/modules/ledc.c)|


This module provides access to the [LEDC](http://esp-idf.readthedocs.io/en/latest/api-reference/peripherals/ledc.html) PWM driver.

# LEDC Overview
The LED control module is primarily designed to control the intensity of LEDs, although it can be used to generate PWM signals for other purposes as well. It has 16 channels which can generate independent waveforms that can be used to drive e.g. RGB LED devices. For maximum flexibility, the high-speed as well as the low-speed channels can be driven from one of four high-speed/low-speed timers. The PWM controller also has the ability to automatically increase or decrease the duty cycle gradually, allowing for fades without any processor interference.


## ledc.newChannel()
Configures a PIN to be controlled by the LEDC system.

#### Syntax
```lua
myChannel = ledc.newChannel({
  gpio=x,
  bits=ledc.TIMER_10_BIT || ledc.TIMER_11_BIT || ledc.TIMER_12_BIT || ledc.TIMER_13_BIT || ledc.TIMER_14_BIT || ledc.TIMER_15_BIT,
  mode=ledc.HIGH_SPEED || ledc.LOW_SPEED,
  timer=ledc.TIMER_0 || ledc.TIMER_1 || ledc.TIMER_2 || ledc.TIMER_3,
  channel=ledc.CHANNEL_0 || ledc.CHANNEL_1 || ledc.CHANNEL_2 || ledc.CHANNEL_3 || ledc.CHANNEL_4 || ledc.CHANNEL_5 || ledc.CHANNEL_6 || ledc.CHANNEL_7,
  frequency=x,
  invert=false,
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
- `invert` Inverts the output. False, with duty 0, is always low.
- `duty` Channel duty, the duty range is [0, (2**bit_num) - 1]. Example: if ledc.TIMER_13_BIT is used maximum value is 4096 x 2 -1 = 8091

#### Returns
`channel` ledc.channel

#### Example
```lua
myChannel = ledc.newChannel({
  gpio=19,
  bits=ledc.TIMER_13_BIT,
  mode=ledc.HIGH_SPEED,
  timer=ledc.TIMER_0,
  channel=ledc.CHANNEL_0,
  frequency=1000,
  duty=4096
});
```

# LEDC channel module
All interactions with the LEDC subsystem is done on the channel object created using the newChannel method.

## ledc.channel:stop()
Disable LEDC output, and set idle level.

#### Syntax
`channel:stop(idleLevel)`

#### Parameters
- `idleLevel` Set output idle level after LEDC stops.
    - `ledc.IDLE_LOW`
    - `ledc.IDLE_HIGH`

#### Returns
`nil`

#### Example
```lua
channel:stop(ledc.IDLE_LOW)
```

## ledc.channel:setfreq()
Set channel frequency (Hz)

#### Syntax
`channel:setfreq(frequency)`

#### Parameters
- `frequency` What frequency should be set

#### Returns
`nil`

#### Example
```lua
channel:setfreq(2000)
```


## ledc.channel:getfreq()
Get channel frequency (Hz)

#### Syntax
`channel:getfreq()`

#### Parameters
None

#### Returns
- 0 error
- Others current LEDC frequency

#### Example
```lua
channel:getfreq()
```


## ledc.channel:setduty()
Set channel duty

#### Syntax
`channel:setdutyduty)`

#### Parameters
- `duty` What duty should be set

#### Returns
`nil`

#### Example
```lua
channel:setduty(4096)
```


## ledc.channel:getduty()
Get channel duty

#### Syntax
`channel:getduty()`

#### Parameters
None

#### Returns
- (-1) parameter error
- Others current LEDC duty

#### Example
```lua
channel:getduty()
```


## ledc.channel:reset()
Resets the timer

#### Syntax
`channel:reset()`

#### Parameters
None

#### Returns
`nil`


#### Example
```lua
channel:reset();
```


## ledc.channel:pause()
Pauses the timer

#### Syntax
`channel:pause()`

#### Parameters
None

#### Returns
`nil`

#### Example
```lua
channel:pause();
```


## ledc.channel:resume()
Resumes a paused timer

#### Syntax
`channel:resume()`

#### Parameters
None

#### Returns
`nil`

#### Example
```lua
channel:resume()
```


## ledc.channel:fadewithtime()
Set LEDC fade function, with a limited time.

#### Syntax
`channel:fadewithtime(targetDuty, maxFadeTime[, wait])`

#### Parameters
- `targetDuty` Target duty of fading.
- `maxFadeTime`  The maximum time of the fading ( ms ).
- `wait` Whether to block until fading done.
    - `ledc.FADE_NO_WAIT` (default)
    - `ledc.FADE_WAIT_DONE`

#### Returns
`nil`

#### Example
```lua
channel:fadewithtime(4096, 1000);
```


## ledc.channel:fadewithstep()
Set LEDC fade function, with step.

#### Syntax
`channel:fadewithstep(targetDuty, scale, cycleNum[, wait])`

#### Parameters
- `targetDuty` Target duty of fading.
- `scale` Controls the increase or decrease step scale.
- `cycleNum` Increase or decrease the duty every cycle_num cycles
- `wait` Whether to block until fading done.
    - `ledc.FADE_NO_WAIT` (default)
    - `ledc.FADE_WAIT_DONE`

#### Returns
`nil`

#### Example
```lua
channel:fadewithstep(1000, 10, 10);
```


## ledc.channel:fade()
Set LEDC fade function.

#### Syntax
`channel:fade(duty, direction, scale, cycleNum, stepNum [, wait])`

#### Parameters
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
`nil`

#### Example
```lua
channel:fade(0, ledc.FADE_INCREASE, 100, 100, 1000);
```

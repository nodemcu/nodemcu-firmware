# dimmer Module
| Since      | Origin / Contributor                            | Maintainer                                      | Source                                        |
| :--------- | :---------------------------------------------- | :---------------------------------------------- | :-------------------------------------------- |
| 2020-01-29 | [Javier Peletier](https://github.com/jpeletier) | [Javier Peletier](https://github.com/jpeletier) | [dimmer.c](../../components/modules/dimmer.c) |

This module implements phase-dimming for TRIAC-based mains dimmers. 

## How phase-dimming works
Find here a [brief description on how TRIAC-based dimming works](https://www.lamps-on-line.com/leading-trailing-edge-led-dimmers).

In essence, phase dimming implies generating a square-wave PWM signal, with the special characteristic that it must be perfectly synchronized with mains, modulating it, and thus delivering more or less effective power to the load depending on the PWM signal duty cycle.

In order to do this, a dimmer module or circuit must be used to a) detect when the mains signal crosses 0V, to use it as a synchronization point and b) modulate mains. The module must isolate mains from the microcontroller.

Some reference hardware: 
* [Schematics for a homemade module](https://hackaday.io/project/165927-a-digital-ac-dimmer-using-arduino/details)
* [Example commercial hardware](https://robotdyn.com/ac-light-dimmer-module-1-channel-3-3v-5v-logic-ac-50-60hz-220v-110v.html)

These modules come with a TRIAC whose gate is intended to be driven by a GPIO pin, isolated from mains by an optocoupler. These modules also come with a zero-crossing detector, also isolated, that raises a pin voltage when the AC mains sine wave signal crosses 0V.

## Architecture of this nodemcu module

The above scheme is implemented in this module by dedicating ESP32's CPU1 entirely to this purpose... Phase dimming requires very accurate timing. Configuring timer interrupts in the "busy" CPU0 does not work properly, with FreeRTOS scheduler, WiFi and so on demanding their share of the CPU at random intervals, which would make dimmed lamps flicker.

Once the dimmer module is started, by means of `dimmer.setup()`, a busy loop is launched on CPU1 that monitors zero-crossing signals from the dimmer and turns on/off the TRIAC at the appropriate time, with nanosecond precision.

## Required SDK `menuconfig` changes
To use this module, change the following in menuconfig (`make menuconfig`):

* Enable *FreeRTOS in both cores* by unselecting *"Component Config/FreeRTOS/Run FreeRTOS only on first core"*. This will allow the module to use CPU1.
* Unselect  *"Component Config/ESP32-specific/Also watch CPU1 tick interrupt"*
* Unselect  *"Component Config/ESP32-specific/Watch CPU1 idle task"*

The last two settings disable the reset watchdog for CPU1. This is necessary since CPU1 is purposefully going to be running an infinite loop.

## Example

```lua
dimmer.setup(14)     -- configure pin 14 as zero-crossing detector
dimmer.add(22)       -- TRIAC gate controlling lightbulb is connected to GPIO pin 22
dimmer.setLevel(300) -- set brightness to 300‰ (30%)
```


## dimmer.setup()

Initializes the dimmer module. This will start a task that continuously monitors for signal zero crossing and turns on/off the different dimmed pins at the right time.

#### Syntax

`dimmer.setup(zc_pin, [transition_speed])`

#### Parameters

* `zc_pin`: GPIO pin number where the zero crossing detector is connected. `dimmer.setup()` will configure this pin as input.
* `transition_speed`: integer. Specifies a transition time or fade to be applied every time a brightness level is changed. It is defined as a per mille (‰) brightness delta per half mains cycle (10ms if 50Hz). For example, if set to 20, a light would go from zero to full brightness in 1000 / 20 * 10ms = 500ms 
  *  Value must be between 1 and 1000 (instant change).
  *  Defaults to a quick ~100ms fade.

#### Returns

Throws errors if CPU1 task cannot be started or `zc_pin` cannot be configured as input.

It will reset if core 1 CPU is not active or their watchdogs are still enabled. See

## dimmer.add()

Adds the provided pin to the dimmer module. The pin will be configured as output.

#### Syntax

`dimmer.add(pin, [dimming_mode])`

#### Parameters

* `pin`: the GPIO pin to control.
* `dimming_mode`: Dimming mode, either `dimmer.LEADING_EDGE` (default) or `dimmer.TRAILING_EDGE`, depending on the type of load or lightbulb.

#### Returns

`dimmer.add()` will throw an error if the pin was already added or if an incorrect GPIO pin is provided.

## dimmer.remove()

Removes the given pin from the dimmer module

#### Syntax

`dimmer.remove(pin)`

#### Parameters

* `pin`: the GPIO pin to stop controlling.

#### Returns

Will throw an error if the pin is not currently controlled by the module.

## dimmer.setLevel()

Changes the brightness level for a dimmed pin

#### Syntax

`dimmer.setLevel(pin, brightness)`

#### Parameters

* `pin`: Pin to configure
* `brightness`: Integer. Per-mille (‰) brightness level or duty cycle, 0: off, 1000: fully on, or anything in between.

#### Returns

Will throw an error if attempting to configure a pin that was not added previously.

## dimmer.mainsFrequency()

Returns the last measured mains frequency, in cHz (100ths of Hz). You must call `dimmer.setup()` first. Note that at least half a mains cycle must have passed (10ms at 50Hz) after `dimmer.setup()` for this function to work!

#### Syntax

`dimmer.mainsFrequency()`

#### Returns

Integer with the measured mains frequency, in cHz. `0` if mains is not detected. 

#### Example

```lua
dimmer.setup(14) -- Zero crossing detector connected to GPIO14
tmr.create():alarm(1000, tmr.ALARM_AUTO, function()
    print("Mains frequency is %d Hz", dimmer.mainsFrequency() / 100)
end)
```

# WS2812 effects Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-11-01 | [Konrad Huebner](https://github.com/skycoders) | [Konrad Huebner](https://github.com/skycoders) | [ws2812_effects.c](../../../app/modules/ws2812_effects.c)|

This module provides effects based on the [WS2812 library](ws2812.md). Some effects are inspired by / based on the [WS2812FX Library](https://github.com/kitesurfer1404/WS2812FX) but have been adopted to the specifics of the ws2812 library. The effects library works based on a buffer created through the ws2812 library and performs the operations on this buffer.

Note that dual mode is currently not supported for effects.

!!! caution

    This module depends on the [color utils module](color-utils.md). Things **will** fail if that module is missing in the firmware!

#### Example usage
```lua
-- init the ws2812 module
ws2812.init(ws2812.MODE_SINGLE)
-- create a buffer, 60 LEDs with 3 color bytes
strip_buffer = ws2812.newBuffer(60, 3)
-- init the effects module, set color to red and start blinking
ws2812_effects.init(strip_buffer)
ws2812_effects.set_speed(100)
ws2812_effects.set_brightness(50)
ws2812_effects.set_color(0,255,0)
ws2812_effects.set_mode("blink")
ws2812_effects.start()
```

## ws2812_effects.init()
Initialize the effects library with the provided buffer for the connected LED strip.

#### Syntax
`ws2812_effects.init(buffer)`

#### Parameters
- `buffer` is a `ws2812.buffer` for the connected strip.

#### Returns
`nil`

## ws2812_effects.start()
Start the animation effect.

#### Syntax
`ws2812_effects.start()`

#### Parameters
`none`

#### Returns
`nil`

## ws2812_effects.stop()
Stop the animation effect.

#### Syntax
`ws2812_effects.stop()`

#### Parameters
`none`

#### Returns
`nil`


## ws2812_effects.set_brightness()
Set the brightness.

#### Syntax
`ws2812_effects.set_brightness(brightness)`

#### Parameters
- `brightness` brightness between 0 and 255

#### Returns
`nil`

## ws2812_effects.set_color()
Set the color.

#### Syntax
`ws2812_effects.set_color(g, r, b, [w])`

#### Parameters
- `g` is the green value between 0 and 255
- `r` is the red value between 0 and 255
- `b` is the blue value between 0 and 255
- `w` (optional) is the white value between 0 and 255

#### Returns
`nil`

## ws2812_effects.set_speed()
Set the speed.

#### Syntax
`ws2812_effects.set_speed(speed)`

#### Parameters
- `speed` speed between 0 and 255

#### Returns
`nil`


## ws2812_effects.get_speed()
Get current speed.

#### Syntax
`ws2812_effects.get_speed()`

#### Parameters
`none`

#### Returns
`speed` between 0 and 255


## ws2812_effects.set_delay()
Set the delay between two effect steps in milliseconds.

#### Syntax
`ws2812_effects.set_delay(delay)`

#### Parameters
- `delay` is the delay in milliseconds, minimum 10ms

#### Returns
`nil`

## ws2812_effects.get_delay()
Get current delay.

#### Syntax
`ws2812_effects.get_delay()`

#### Parameters
`none`

#### Returns
`delay` is the current effect delay in milliseconds


## ws2812_effects.set_mode()
Set the active effect mode.

#### Syntax
`ws2812_effects.set_mode(mode, [effect_param])`

#### Parameters
- `mode` is the effect mode as a string, can be one of
  - `static` fills the buffer with the color set through `ws2812_effects.set_color()`
  - `blink` fills the buffer with the color set through `ws2812_effects.set_color()` and starts blinking
  - `gradient` fills the buffer with a gradient defined by the color values provided with the `effect_param`. This parameter must be a string containing the color values with same pixel size as the current buffer configuration. Minimum two colors must be provided. If more are provided, the strip is split in equal parts and the colors are used as intermediate colors. The gradient is calculated based on HSV color space, so no greyscale colors are supported as those cannot be converted to HSV.
  - `gradient_rgb` similar to `gradient` but uses simple RGB value interpolation instead of conversions to the HSV color space.
  - `random_color` fills the buffer completely with a random color and changes this color constantly
  - `rainbow` animates through the full color spectrum, with the entire strip having the same color
  - `rainbow_cycle` fills the buffer with a rainbow gradient. The optional second parameter states the number of repetitions (integer).
  - `flicker` fills the buffer with the color set through `ws2812_effects.set_color()` and begins random flickering of pixels with a maximum flicker amount defined by the second parameter (integer, e.g. 50 to flicker with 50/255 of the color)
  - `fire` is a fire flickering effect
  - `fire_soft` is a soft fire flickering effect
  - `fire_intense` is an intense fire flickering effect
  - `halloween` fills the strip with purple and orange pixels and circles them
  - `circus_combustus` fills the strip with red/white/black pixels and circles them
  - `larson_scanner` is the K.I.T.T. scanner effect, based on the color set through `ws2812_effects.set_color()`
  - `color_wipe` fills the strip pixel by pixel with the color set through `ws2812_effects.set_color()` and then starts turning pixels off again from beginning to end.
  - `random_dot` sets random dots to the color set through `ws2812_effects.set_color()` and fades them out again
  - `cycle` takes the buffer as-is and cycles it. With the second parameter it can be defined how many pixels the shift will be. Negative values shift to opposite direction.
- `effect_param` is an optional effect parameter. See the effect modes for further explanations. It can be an integer value or a string.

#### Returns
`nil`

#### Examples
Full initialization code for the strip, a buffer and the effect library can be found at top of this documentation. Only effect examples are shown here.
```lua
-- rainbow cycle with two repetitions
ws2812_effects.set_mode("rainbow_cycle", 2)
-- gradient from red to yellow to red
ws2812_effects.set_mode("gradient", string.char(0,200,0,200,200,0,0,200,0))
-- random dots with fading
ws2812_effects.set_mode("random_dot",3)
```

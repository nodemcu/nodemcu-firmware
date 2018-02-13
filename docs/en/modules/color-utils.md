# color utils Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-12-30 | [Konrad Huebner](https://github.com/skycoders) | [Konrad Huebner](https://github.com/skycoders) | [color_utils.c](../../../app/modules/color_utils.c)|

This module provides basic color transformations useful for color LEDs. 

## color_utils.hsv2grb()
Convert HSV color to GRB color.

#### Syntax
`color_utils.hsv2grb(hue, saturation, value)`

#### Parameters
- `hue` is the hue value, between 0 and 360
- `saturation` is the saturation value, between 0 and 255
- `value` is the value value, between 0 and 255

#### Returns
`green`, `red`, `blue` as values between 0 and 255

## color\_utils.hsv2grbw()
Convert HSV color to GRB color and explicitly return a white value. This can be useful for RGB+W LED strips. The white value is simply calculated as min(g, r, b) and then removed from the colors. This does NOT take into account if the white chip used later creates an appropriate color.

#### Syntax
`color_utils.hsv2grbw(hue, saturation, value)`

#### Parameters
- `hue` is the hue value, between 0 and 360
- `saturation` is the saturation value, between 0 and 255
- `value` is the value value, between 0 and 255

#### Returns
`green`, `red`, `blue`, `white` as values between 0 and 255

## color\_utils.grb2hsv()
Convert GRB color to HSV color.

#### Syntax
`color_utils.grb2hsv(green, red, blue)`

#### Parameters
- `green` is the green value, between 0 and 255
- `red` is the red value, between 0 and 255
- `blue` is the blue value, between 0 and 255

#### Returns
`hue`, `saturation`, `value` as values between 0 and 360, respective 0 and 255

## color\_utils.colorWheel()
The color wheel function makes use of the HSV color space and calculates colors based on the color circle. The colors are created with full saturation and value. This function is a convenience function of the hsv2grb function and can be used to create rainbow colors.

#### Syntax
`color_utils.colorWheel(angle)`

#### Parameters
- `angle` is the angle on the color circle, between 0 and 359

#### Returns
`green`, `red`, `blue` as values between 0 and 255

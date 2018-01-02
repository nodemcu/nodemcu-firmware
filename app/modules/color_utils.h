#ifndef APP_MODULES_COLOR_UTILS_H_
#define APP_MODULES_COLOR_UTILS_H_

#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"
#include "c_stdlib.h"
#include "c_math.h"
#include "c_string.h"
#include "user_interface.h"
#include "osapi.h"

/**
* Convert hsv to grb
* hue is 0-360, sat and val are 0-255
*/
uint32_t hsv2grb(uint16_t hue, uint8_t sat, uint8_t val);
/**
* Convert hsv to grbw
* hue is 0-360, sat and val are 0-255
*/
uint32_t hsv2grbw(uint16_t hue, uint8_t sat, uint8_t val);
/**
* Convert grb to hsv
* g, r, b are 0-255
*/
uint32_t grb2hsv(uint8_t g, uint8_t r, uint8_t b);


/**
* The color wheel function provides colors from r -> g -> b -> r.
* They are fully saturated and with full brightness.
* degree is from 0-360
*/
uint32_t color_wheel(uint16_t degree);


#endif /* APP_MODULES_COLOR_UTILS_H_ */

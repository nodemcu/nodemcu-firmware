HDC1000 NodeMCU Library
=======================

Here's my NodeMCU library for the TI HDC1000 temperature and humidity sensor. It should work with the HDC1008 too but I haven't tested it.

### Setup your sensor:
First, require it:

`hdc1000 = require("hdc1000")`

Then, initialize it:

`hdc1000.init(sda, scl)`

Configure it:

`hdc1000.config()`

Default options set the address to 0x40 and enable both temperature and humidity readings at 14-bit resolution, with the integrated heater on. You can change them by initializing your sensor like this:

`mySensor.begin(address, resolution, heater);`

"resolution" can be set to 14 bits for both temperature and humidity (0x00 - default) 11 bits for temperature (0x40), 11 bits for humidity (0x01), 8 bits for humidity (0x20)
"heater" can be set to ON (0x20 - default) or OFF (0x00)

### Read some values
You can read temperature and humidity by using the following commands:

`temperature = hdc1000.getTemp()` in Celsius degrees.

`humidity = hdc1000.getHumi()` in %

### Check your battery

The following code returns true if the battery voltage is <2.8V, false otherwise.

`isDead = hdc1000.batteryDead();`

Happy making! Also, check out my Breakout Board and Arduino library for this chip: http://b.truzzi.me/hdc1000-temperature-and-humidity-sensor-breakout-with-arduino-library/.

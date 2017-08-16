# DS18B20 Module

This is a Lua module for the DS18B20 1-Wire digital thermometer. Note that NodeMCU offers both a Lua module (this one) and [a C module for this sensor](http://nodemcu.readthedocs.io/en/latest/en/modules/ds18b20/). See [#2003](https://github.com/nodemcu/nodemcu-firmware/pull/2003) for a discussion on the respective merits of them.

## Require
```lua
ds18b20 = require("ds18b20")
```
## Release
```lua
ds18b20 = nil
package.loaded["ds18b20"]=nil
```

# Methods

## read_temp()
Scans the bus for DS18B20 sensors (optional), starts a readout (conversion) for all sensors and calls a callback function when all temperatures are available. Powered sensors are read at once first. Parasite-powered sensors are read one by one. The first parasite-powered sensor is read together with all powered sensors.

The module requires `ow` module.

#### Syntax
`read_temp(callback, pin, unit, force_search, save_search)`

#### Parameters
- `callback` function that receives all results when all conversions finish. The callback function has one parameter - an array addressed by sensor addresses and a value of the temperature (string for integer version).
- `pin` pin of the one-wire bus. If nil, GPIO0 (3) is used.
- `unit` unit can be Celsius ("C" or ds18b20.C), Kelvin ("K" or ds18b20.K) or Fahrenheit ("F" or ds18b20.F). If not specified (nil) latest used unit is used.
- `force_search` if not nil a bus search for devices is performed before readout. If nil the existing list of sensors in memory is used. If the bus has not been searched yet the search performed as well.
- `save_search` if not nil found sensors are saved to the file `ds18b20_save.lc`. When `read_temp` is called, list of sensors in memory is empty and file `ds18b20_save.lc` is present then sensor addresses are loaded from file - usefull when running from batteries & deepsleep - immediate readout is performed (no bus scan).

#### Returns
nil

#### Example
```lua
local t = require("ds18b20")
local pin = 3 -- gpio0 = 3, gpio2 = 4

local function readout(temp)
  if t.sens then
    print("Total number of DS18B20 sensors: ".. #t.sens)
    for i, s in ipairs(t.sens) do
      print(string.format("  sensor #%d address: %s%s",  i, ('%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X'):format(s:byte(1,8)), s:byte(9) == 1 and " (parasite)" or ""))
    end
  end
  for addr, temp in pairs(temp) do
    print(string.format("Sensor %s: %s °C", ('%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X'):format(addr:byte(1,8)), temp))
  end

  -- Module can be released when it is no longer needed
  t = nil
  package.loaded["ds18b20"]=nil
end

t:read_temp(readout, pin, t.C)```

## enable_debug()
Enables debug output of the module.

# Properties

## sens
A table with sensors present on the bus. It includes its address (8 bytes) and information whether the sensor is parasite-powered (9-th byte, 0 or 1).

## temp
A table with readout values (also passed as a parameter to callback function). It is addressed by sensor addresses.
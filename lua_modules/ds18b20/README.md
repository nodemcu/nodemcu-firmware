# DS18B20 Module
## Require
```lua
ds18b20 = require("ds18b20")
```
## Release
```lua
ds18b20 = nil
package.loaded["ds18b20"]=nil
```
<a id="ds18b20_setup"></a>

## readTemp()
Scans the bus for DS18B20 sensors, starts a readout (conversion) for all sensors and calls a callback function when all temperatures are available. Powered sensors are read at once first. Parasite-powered sensors are read one by one. The first parasite-powered sensor is read together with all powered sensors.

The module requires `ow` module.

The also module uses `encoder` module for printing debug information with more readable representation of sensor address (`encoder.toHex()`).

#### Syntax
`readTemp(callback, pin)`

#### Parameters
- `callback` function that receives all results when all conversions finish. The callback function has one parameter - an array addressed by sensor addresses and a value of the temperature (string for integer version).
- `pin` pin of the one-wire bus. If nil, GPIO0 (3) is used.

#### Returns
nil

#### Example
```lua
t = require("ds18b20")
pin = 3 -- gpio0 = 3, gpio2 = 4

function readout(temp)
  for addr, temp in pairs(temp) do
    print(string.format("Sensor %s: %s 'C", encoder.toHex(addr), temp))
  end

  -- Module can be released when it is no longer needed
  t = nil
  package.loaded["ds18b20"]=nil
end

-- t:readTemp(readout) -- default pin value is 3
t:readTemp(readout, pin)
if t.sens then
  print("Total number of DS18B20 sensors: "..table.getn(t.sens))
  for i, s in ipairs(t.sens) do
    -- print(string.format("  sensor #%d address: %s%s", i, s.addr, s.parasite == 1 and " (parasite)" or ""))
    print(string.format("  sensor #%d address: %s%s",  i, encoder.toHex(s.addr), s.parasite == 1 and " (parasite)" or "")) -- readable address with Hex encoding is preferred when encoder module is available
  end
end
```

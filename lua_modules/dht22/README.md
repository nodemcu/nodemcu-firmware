# DHT22 module

This module is compatible with DHT22 and DHT21.  
No need to use a resistor to connect the pin data of DHT22 to ESP8266.

## Example  
```lua
PIN = 4 --  data pin, GPIO2

dht22 = require("dht22")
dht22.read(PIN)
t = dht22.getTemperature()
h = dht22.getHumidity()

if h == -1 then
  print("Error reading from DHT22")
else
  -- temperature in degrees Celsius  and Farenheit
  print("Temperature: "..(t / 10).."."..(t % 10).." deg C")
  print("Temperature: "..(9 * t / 50 + 32).."."..(9 * t / 5 % 10).." deg F")

  -- humidity
  print("Humidity: "..(h/10).."."..(h%10).."%")
end

-- release module
dht22 = nil
package.loaded["dht22"]=nil
```
## Functions
### read
read(pin) 
Read humidity and temperature from DHT22.

**Parameters:**

* pin - ESP8266 pin connect to data pin in DHT22

### getHumidity
getHumidity()  
Returns the humidity of the last reading.

**Returns:**  
* last humidity reading in per thousand

### getTemperature
getTemperature()  
Returns the temperature of the last reading.

**Returns:**  
* last temperature reading in 0.1ºC


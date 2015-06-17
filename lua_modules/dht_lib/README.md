# DHTxx module

This module is compatible with DHT11, DHT21 and DHT22.  
And is able to auto-select wheather you are using DHT11 or DHT2x

No need to use a resistor to connect the pin data of DHT22 to ESP8266.

##Integer Verison[When using DHT11, Float version is useless...]
### Example  
```lua
PIN = 4 --  data pin, GPIO2

DHT= require("dht_lib")

DHT.read(PIN)

t = DHT.getTemperature()
h = DHT.getHumidity()

if h == nil then
  print("Error reading from DHTxx")
else
  -- temperature in degrees Celsius  and Farenheit

  print("Temperature: "..((t-(t % 10)) / 10).."."..(t % 10).." deg C")

  print("Temperature: "..(9 * t / 50 + 32).."."..(9 * t / 5 % 10).." deg F")
  
  -- humidity

  print("Humidity: "..((h - (h % 10)) / 10).."."..(h % 10).."%")
end

-- release module
DHT = nil
package.loaded["dht_lib"]=nil
```
##Float Verison
###Example
```lua
PIN = 4 --  data pin, GPIO2

DHT= require("dht_lib")

DHT.read(PIN)

t = DHT.getTemperature()
h = DHT.getHumidity()

if h == nil then
  print("Error reading from DHT11/22")
else
  -- temperature in degrees Celsius  and Farenheit
  -- floating point and integer version:

  print("Temperature: "..(t/10).." deg C")
  print("Temperature: "..(9 * t / 50 + 32).." deg F")
  
  -- humidity
  print("Humidity: "..(h/10).."%")
end

-- release module
DHT = nil
package.loaded["dht_lib"]=nil
```
## Functions

###read
read(pin)
Read humidity and temperature from DHTxx(11,21,22...).
**Parameters:**

* pin - ESP8266 pin connect to data pin

### getHumidity
getHumidity()  
Returns the humidity of the last reading.

**Returns:**  
* last humidity reading in per thousand

### getTemperature
getTemperature()  
Returns the temperature of the last reading.

**Returns:**  
* last temperature reading in(dht22) 0.1ºC (dht11)1ºC
* 

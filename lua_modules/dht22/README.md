# DHTxx module

This module is compatible with DHT11, DHT21 and DHT22.  
No need to use a resistor to connect the pin data of DHT22 to ESP8266.

##Integer Verison[When using DHT11, Float version is useless...]
### Example  
```lua
PIN = 4 --  data pin, GPIO2

DHT= require("dht_lib")

--dht.read11(PIN)
DHT.read22(PIN)

t = DHT.getTemperature()
h = DHT.getHumidity()

if h == nil then
  print("Error reading from DHT11/22")
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

--dht.read11(PIN)
DHT.read22(PIN)

t = DHT.getTemperature()
h = DHT.getHumidity()

if h == nil then
  print("Error reading from DHT11/22")
else
  -- temperature in degrees Celsius  and Farenheit
  -- floating point and integer version:
  print("Temperature: "..t.." deg C")
  print("Temperature: "..(9 * t / 50 + 32).." deg F")
  
  -- humidity
  print("Humidity: "..h.."%")
end

-- release module
DHT = nil
package.loaded["dht_lib"]=nil
```
## Functions
### read11
read11(pin)  
Read humidity and temperature from DHT11.
###read22
read22(pin)
Read humidity and temperature from DHT22/21.
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


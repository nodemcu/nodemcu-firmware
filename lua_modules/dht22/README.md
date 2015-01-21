# DHT22 module

## Example  
```lua
PIN = 4 --  data pin, GPIO2

dht22 = require("dht22")
dht22.read(PIN)
t = dht22.getTemperature()
h = dht22.getHumidity()

-- temperature in degrees Celsius  and Farenheit
print("Temperature: "..(t/10).."."..(t%10).." deg C")
print("Temperature: "..(9 * t / 50 + 32).."."..(9 * t / 5 % 10).." deg F")

-- humidity
print("Humidity: "..(h/10).."."..(h%10).."%")

-- release module
dht22 = nil
package.loaded["dht22"]=nil
```

t = require("ds18b20")

-- ESP-01 GPIO Mapping
gpio0 = 3
gpio2 = 4

t.setup(gpio0)
addrs = t.addrs()
if (addrs ~= nil) then
  print("Total DS18B20 sensors: "..table.getn(addrs))
end

-- Just read temperature
print("Temperature: "..t.read().."'C")

-- Get temperature of first detected sensor in Fahrenheit
print("Temperature: "..t.read(nil,t.F).."'F")

-- Query the second detected sensor, get temperature in Kelvin
if (table.getn(addrs) >= 2) then
   print("Second sensor: "..t.read(addrs[2],t.K).."'K")
end

-- Don't forget to release it after use
t = nil
ds18b20 = nil
package.loaded["ds18b20"]=nil

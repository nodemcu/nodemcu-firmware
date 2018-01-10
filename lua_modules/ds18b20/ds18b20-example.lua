t = require("ds18b20")
pin = 3 -- gpio0 = 3, gpio2 = 4

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
  --t = nil
  --package.loaded["ds18b20"]=nil
end

t:enable_debug()
file.remove("ds18b20_save.lc") -- remove saved addresses
print("=============================================", node.heap())
print("first call, no addresses in flash, search is performed")
t:read_temp(readout, pin, t.C) 

tmr.create():alarm(2000, tmr.ALARM_SINGLE, function() 
    print("=============================================", node.heap())
    print("second readout, no new search, found addresses are used")
    t:read_temp(readout, pin) 

tmr.create():alarm(2000, tmr.ALARM_SINGLE, function() 
    print("=============================================", node.heap())
    print("force search again")
    t:read_temp(readout, pin, nil, true) 

tmr.create():alarm(2000, tmr.ALARM_SINGLE, function() 
    print("=============================================", node.heap())
    print("save search results")
    t:read_temp(readout, pin, nil, false, true)

tmr.create():alarm(2000, tmr.ALARM_SINGLE, function() 
    print("=============================================", node.heap())
    print("use saved addresses")
    t.sens={}
    t:read_temp(readout, pin)
end)

end)

end)

end)

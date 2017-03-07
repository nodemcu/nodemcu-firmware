-- encoder module is needed only for debug output; lines can be removed if no
-- debug output is needed and/or encoder module is missing

t = require("ds18b20")
pin = 3 -- gpio0 = 3, gpio2 = 4

function readout(temp)
  for addr, temp in pairs(temp) do
    -- print(string.format("Sensor %s: %s 'C", addr, temp))
    print(string.format("Sensor %s: %s °C", encoder.toHex(addr), temp)) -- readable address with base64 encoding is preferred when encoder module is available
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
    print(string.format("  sensor #%d address: %s%s",  i, encoder.toHex(s.addr), s.parasite == 1 and " (parasite)" or "")) -- readable address with base64 encoding is preferred when encoder module is available
  end
end

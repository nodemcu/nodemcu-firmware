local PIN = 4
local t, h = dht.read22(PIN)

if h == nil then
  print("Error reading from DHT11/22")
else
  -- temperature in Farenheit
  local temp = (9 * t / 50 + 32)
  print("Temperature: "..temp.." deg F")

  -- humidity
  local hum = h / 10
  print("Humidity: "..hum.."%")
  --print(temp, hum)
end

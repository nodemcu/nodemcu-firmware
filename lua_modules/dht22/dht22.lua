-- ***************************************************************************
-- DHT22 module for ESP8266 with nodeMCU
--
-- Written by Javier Yanez 
-- but based on a script of Pigs Fly from ESP8266.com forum
--
-- MIT license, http://opensource.org/licenses/MIT
-- ***************************************************************************

local moduleName = ...
local M = {}
_G[moduleName] = M

local humidity
local temperature

function M.read(pin)
  local checksum
  local checksumTest
  humidity = 0
  temperature = 0
  checksum = 0

  -- Use Markus Gritsch trick to speed up read/write on GPIO
  local gpio_read = gpio.read
  
  local bitStream = {}
  for j = 1, 40, 1 do
    bitStream[j] = 0
  end
  local bitlength = 0

  -- Step 1:  send out start signal to DHT22
  gpio.mode(pin, gpio.OUTPUT)
  gpio.write(pin, gpio.HIGH)
  tmr.delay(100)
  gpio.write(pin, gpio.LOW)
  tmr.delay(20000)
  gpio.write(pin, gpio.HIGH)
  gpio.mode(pin, gpio.INPUT)

  -- Step 2:  DHT22 send response signal 
  -- bus will always let up eventually, don't bother with timeout
  while (gpio_read(pin) == 0 ) do end
  local c=0
  while (gpio_read(pin) == 1 and c < 500) do c = c + 1 end
  -- bus will always let up eventually, don't bother with timeout
  while (gpio_read(pin) == 0 ) do end
  c=0
  while (gpio_read(pin) == 1 and c < 500) do c = c + 1 end
  
  -- Step 3: DHT22 send data
  for j = 1, 40, 1 do
    while (gpio_read(pin) == 1 and bitlength < 10 ) do
      bitlength = bitlength + 1
    end
    bitStream[j] = bitlength
    bitlength = 0
    -- bus will always let up eventually, don't bother with timeout
    while (gpio_read(pin) == 0) do end
  end

  --DHT data acquired, process.
  for i = 1, 16, 1 do
    if (bitStream[i] > 3) then
      humidity = humidity + 2 ^ (16 - i)
    end
  end
  for i = 1, 16, 1 do
    if (bitStream[i + 16] > 3) then
      temperature = temperature + 2 ^ (16 - i)
    end
  end
  for i = 1, 8, 1 do
    if (bitStream[i + 32] > 3) then
      checksum = checksum + 2 ^ (8 - i)
    end
  end

  checksumTest = (bit.band(humidity, 0xFF) + bit.rshift(humidity, 8) + bit.band(temperature, 0xFF) + bit.rshift(temperature, 8))
  checksumTest = bit.band(checksumTest, 0xFF)

  if temperature > 0x8000 then
    -- convert to negative format
    temperature = -(temperature - 0x8000)
  end

  -- conditions compatible con float point and integer
  if (checksumTest - checksum >= 1) or (checksum - checksumTest >= 1) then
    humidity = nil
  end
end

function M.getTemperature()
  return temperature
end

function M.getHumidity()
  return humidity
end

return M

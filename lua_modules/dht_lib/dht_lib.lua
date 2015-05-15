-- ***************************************************************************
-- DHTxx(11,21,22) module for ESP8266 with nodeMCU
--
-- Written by Javier Yanez mod by Martin
-- but based on a script of Pigs Fly from ESP8266.com forum
--
-- MIT license, http://opensource.org/licenses/MIT
-- ***************************************************************************

--Support list：

--DHT11 Tested 
--DHT21 Not Test yet
--DHT22(AM2302) Tested
--AM2320 Not Test yet

--Output format-> Real temperature times 10(or DHT22 will miss it float part in Int Version)
--==========================Module Part======================
local moduleName = ...
local M = {}
_G[moduleName] = M
--==========================Local the UMI and TEMP===========
local humidity
local temperature
--==========================Local the bitStream==============
local bitStream = {}

---------------------------Read bitStream from DHTXX--------------------------
local function read(pin)

  local bitlength = 0
  humidity = 0
  temperature = 0

  -- Use Markus Gritsch trick to speed up read/write on GPIO
  local gpio_read = gpio.read
  
  
  for j = 1, 40, 1 do
    bitStream[j] = 0
  end

  -- Step 1:  send out start signal to DHT22
  gpio.mode(pin, gpio.OUTPUT)
  gpio.write(pin, gpio.HIGH)
  tmr.delay(100)
  gpio.write(pin, gpio.LOW)
  tmr.delay(20000)
  gpio.write(pin, gpio.HIGH)
  gpio.mode(pin, gpio.INPUT)

  -- Step 2:  Receive bitStream from DHT11/22
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
end

---------------------------Check out the data--------------------------
----Auto Select the DHT11/DHT22 By check the byte[1] && byte[3] -------
---------------Which is empty when using DHT11-------------------------
function M.read(pin)
  read(pin)

  local byte_0 = 0
  local byte_1 = 0
  local byte_2 = 0
  local byte_3 = 0
  local byte_4 = 0

  for i = 1, 8, 1 do -- Byte[0]
    if (bitStream[i] > 3) then
      byte_0 = byte_0 + 2 ^ (8 - i)
    end
  end

  for i = 1, 8, 1 do -- Byte[1]
    if (bitStream[i+8] > 3) then
      byte_1 = byte_1 + 2 ^ (8 - i)
    end
  end

  for i = 1, 8, 1 do -- Byte[2]
    if (bitStream[i+16] > 3) then
      byte_2 = byte_2 + 2 ^ (8 - i)
    end
  end

  for i = 1, 8, 1 do -- Byte[3]
    if (bitStream[i+24] > 3) then
      byte_2 = byte_2 + 2 ^ (8 - i)
    end
  end

  for i = 1, 8, 1 do -- Byte[4]
    if (bitStream[i+32] > 3) then
      byte_4 = byte_4 + 2 ^ (8 - i)
    end
  end


  if byte_1==0 and byte_3 == 0 then
  ---------------------------Convert the bitStream into Number through DHT11's Way--------------------------
  --As for DHT11 40Bit is consisit of 5Bytes
  --First byte->Humidity Data's Int part
  --Sencond byte->Humidity Data's Float Part(Which should be empty)
  --Third byte->Temp Data;s Intpart
  --Forth byte->Temp Data's Float Part(Which should be empty)
  --Fifth byte->SUM Byte, Humi+Temp

    if(byte_4 ~= byte_0+byte_2) then
     humidity = nil
     temperature = nil
    else
     humidity = byte_0 *10 -- In order to universe with the DHT22
     temperature = byte_2 *10 
    end

  else ---------------------------Convert the bitStream into Number through DHT22's Way--------------------------
  --As for DHT22 40Bit is consisit of 5Bytes
  --First byte->Humidity Data's High Bit
  --Sencond byte->Humidity Data's Low Bit(And if over 0x8000, use complement)
  --Third byte->Temp Data's High Bit
  --Forth byte->Temp Data's Low Bit
  --Fifth byte->SUM Byte

  humidity = byte_0 * 256 + byte_1
  temperature = byte_2 * 256 + byte_3
  checksum = byte_4

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

   byte_0 = nil
   byte_1 = nil
   byte_2 = nil
   byte_3 = nil
   byte_4 = nil

end
--------------API for geting the data out------------------

function M.getTemperature()
  return temperature
end

function M.getHumidity()
  return humidity
end
-------------Return Index------------------------------------
return M

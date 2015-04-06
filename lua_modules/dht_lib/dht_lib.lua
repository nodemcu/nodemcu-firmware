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
--For example, the data read form DHT2x is 24.3 degree C, and the output will be 243
---------------the data read form DHT1x is 27 degree C, and the output will be 270
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
---------------------------Convert the bitStream into Number through DHT11 Ways--------------------------
local function bit2DHT11()
--As for DHT11 40Bit is consisit of 5Bytes
--First byte->Humidity Data's Int part
--Sencond byte->Humidity Data's Float Part(Which should be empty)
--Third byte->Temp Data;s Intpart
--Forth byte->Temp Data's Float Part(Which should be empty)
--Fifth byte->SUM Byte, Humi+Temp
 local checksum = 0
 local checksumTest
  --DHT data acquired, process.
  for i = 1, 8, 1 do -- Byte[0]
    if (bitStream[i] > 3) then
      humidity = humidity + 2 ^ (8 - i)
    end
  end
  for i = 1, 8, 1 do -- Byte[2]
    if (bitStream[i + 16] > 3) then
      temperature = temperature + 2 ^ (8 - i)
    end
  end
  for i = 1, 8, 1 do --Byte[4]
    if (bitStream[i + 32] > 3) then
      checksum = checksum + 2 ^ (8 - i)
    end
  end

  if(checksum ~= humidity+temperature) then
    humidity = nil
    temperature = nil
  else
    humidity = humidity *10 -- In order to universe the DHT22
    temperature = temperature *10 
  end


  
end
---------------------------Convert the bitStream into Number through DHT22 Ways--------------------------
local function bit2DHT22()
--As for DHT22 40Bit is consisit of 5Bytes
--First byte->Humidity Data's High Bit
--Sencond byte->Humidity Data's Low Bit(And if over 0x8000, use complement)
--Third byte->Temp Data's High Bit
--Forth byte->Temp Data's Low Bit
--Fifth byte->SUM Byte
  local checksum = 0
  local checksumTest
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

---------------------------Check out the data--------------------------
----Auto Select the DHT11/DHT22 by checking the byte[1]==0 && byte[3]==0 ---
---------------Which is empty when using DHT11-------------------------
function M.read(pin)

  read(pin)

  local byte_1 = 0
  local byte_2 = 0

  for i = 1, 8, 1 do -- Byte[1]
    if (bitStream[i+8] > 3) then
      byte_1 = byte_1 + 2 ^ (8 - i)
    end
  end

    for i = 1, 8, 1 do -- Byte[1]
    if (bitStream[i+24] > 3) then
      byte_2 = byte_2 + 2 ^ (8 - i)
    end
  end

  if byte_1==0 and byte_2 == 0 then
    bit2DHT11()
  else
    bit2DHT22()
  end

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

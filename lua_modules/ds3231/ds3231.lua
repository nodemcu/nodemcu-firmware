--------------------------------------------------------------------------------
-- DS3231 I2C module for NODEMCU
-- NODEMCU TEAM
-- LICENCE: http://opensource.org/licenses/MIT
-- Tobie Booth <tbooth@hindbra.in>
--------------------------------------------------------------------------------

require("bit")
require("i2c")

local moduleName = ...
local M = {}
_G[moduleName] = M

-- Constants:
M.EVERYSECOND = 6
M.EVERYMINUTE = 7
M.SECOND = 1
M.MINUTE = 2
M.HOUR = 3
M.DAY = 4
M.DATE = 5
M.DISABLE = 0

-- Default value for i2c communication
local id = 0

--device address
local dev_addr = 0x68

local function decToBcd(val)
  if val == nil then return 0 end
  return((((val/10) - ((val/10)%1)) *16) + (val%10))
end

local function bcdToDec(val)
  return((((val/16) - ((val/16)%1)) *10) + (val%16))
end

local function addAlarmBit(val,day)
  if day == 1 then return bit.bor(val,64) end
  return bit.bor(val,128)
end

--get time from DS3231
function M.getTime()
  i2c.start(id)
  i2c.address(id, dev_addr, i2c.TRANSMITTER)
  i2c.write(id, 0x00)
  i2c.stop(id)
  i2c.start(id)
  i2c.address(id, dev_addr, i2c.RECEIVER)
  local c=i2c.read(id, 7)
  i2c.stop(id)
  return bcdToDec(tonumber(string.byte(c, 1))),
  bcdToDec(tonumber(string.byte(c, 2))),
  bcdToDec(tonumber(string.byte(c, 3))),
  bcdToDec(tonumber(string.byte(c, 4))),
  bcdToDec(tonumber(string.byte(c, 5))),
  bcdToDec(tonumber(string.byte(c, 6))),
  bcdToDec(tonumber(string.byte(c, 7)))
end

--set time for DS3231
-- enosc setted to 1 disables oscilation on battery, stopping time
function M.setTime(second, minute, hour, day, date, month, year, disOsc)
  i2c.start(id)
  i2c.address(id, dev_addr, i2c.TRANSMITTER)
  i2c.write(id, 0x00)
  i2c.write(id, decToBcd(second))
  i2c.write(id, decToBcd(minute))
  i2c.write(id, decToBcd(hour))
  i2c.write(id, decToBcd(day))
  i2c.write(id, decToBcd(date))
  i2c.write(id, decToBcd(month))
  i2c.write(id, decToBcd(year))
  i2c.stop(id)

  i2c.start(id)
  i2c.address(id, dev_addr, i2c.TRANSMITTER)
  i2c.write(id, 0x0E)
  i2c.stop(id)
  i2c.start(id)
  i2c.address(id, dev_addr, i2c.RECEIVER)
  local c = string.byte(i2c.read(id, 1), 1)
  i2c.stop(id)
  if disOsc == 1 then c = bit.bor(c,128)
  else c = bit.band(c,127) end
  i2c.start(id)
  i2c.address(id, dev_addr, i2c.TRANSMITTER)
  i2c.write(id, 0x0E)
  i2c.write(id, c)
  i2c.stop(id)
end

-- Reset alarmId flag to let alarm to be triggered again
function M.reloadAlarms ()
  if bit == nil or bit.band == nil or bit.bor == nil then
    print("[ERROR] Module bit is required to use alarm function")
    return nil
  end
  i2c.start(id)
  i2c.address(id, dev_addr, i2c.TRANSMITTER)
  i2c.write(id, 0x0F)
  i2c.stop(id)
  i2c.start(id)
  i2c.address(id, dev_addr, i2c.RECEIVER)
  local d = string.byte(i2c.read(id, 1), 1)
  i2c.stop(id)
  -- Both flag needs to be 0 to let alarms trigger
  d = bit.band(d,252)
  i2c.start(id)
  i2c.address(id, dev_addr, i2c.TRANSMITTER)
  i2c.write(id, 0x0F)
  i2c.write(id, d)
  i2c.stop(id)
  print('[LOG] Alarm '..almId..' reloaded')
end

-- Enable alarmId bit. Let it to be triggered
function M.enableAlarm (almId)
  if bit == nil or bit.band == nil or bit.bor == nil then
    print("[ERROR] Module bit is required to use alarm function")
    return nil
  end
  if almId ~= 1 and almId ~= 2 then print('[ERROR] Wrong alarm id (1 or 2): '..almId) return end
  i2c.start(id)
  i2c.address(id, dev_addr, i2c.TRANSMITTER)
  i2c.write(id, 0x0E)
  i2c.stop(id)
  i2c.start(id)
  i2c.address(id, dev_addr, i2c.RECEIVER)
  local c = string.byte(i2c.read(id, 1), 1)
  i2c.stop(id)
  c = bit.bor(c,4)
  if almId == 1 then c = bit.bor(c,1)
  else c = bit.bor(c,2) end
  i2c.start(id)
  i2c.address(id, dev_addr, i2c.TRANSMITTER)
  i2c.write(id, 0x0E)
  i2c.write(id, c)
  i2c.stop(id)
  M.reloadAlarms()
  print('[LOG] Alarm '..almId..' enabled')
end
-- If almID equals 1 or 2 disable that alarm, otherwise disables both.
function M.disableAlarm (almId)
  if bit == nil or bit.band == nil or bit.bor == nil then
    print("[ERROR] Module bit is required to use alarm function")
    return nil
  end
  i2c.start(id)
  i2c.address(id, dev_addr, i2c.TRANSMITTER)
  i2c.write(id, 0x0E)
  i2c.stop(id)
  i2c.start(id)
  i2c.address(id, dev_addr, i2c.RECEIVER)
  local c = string.byte(i2c.read(id, 1), 1)
  i2c.stop(id)
  if almId == 1 then c = bit.band(c, 254)
  elseif almId == 2 then c = bit.band(c, 253)
  else
    almId = '1 and 2'
    c = bit.band(c, 252)
  end
  i2c.start(id)
  i2c.address(id, dev_addr, i2c.TRANSMITTER)
  i2c.write(id, 0x0E)
  i2c.write(id, c)
  i2c.stop(id)
  print('[LOG] Alarm '..almId..' disabled')
end

-- almId can be 1 or 2;
-- almType should be taken from constants
function M.setAlarm (almId, almType, second, minute, hour, date)
  if bit == nil or bit.band == nil or bit.bor == nil then
    print("[ERROR] Module bit is required to use alarm function")
    return nil
  end
  if almId ~= 1 and almId ~= 2 then print('[ERROR] Wrong alarm id (1 or 2): '..almId) return end
  M.enableAlarm(almId)
  second = decToBcd(second)
  minute = decToBcd(minute)
  hour = decToBcd(hour)
  date = decToBcd(date)
  if almType == M.EVERYSECOND or almType == M.EVERYMINUTE then
    second = addAlarmBit(second)
    minute = addAlarmBit(minute)
    hour = addAlarmBit(hour)
    date = addAlarmBit(date)
  elseif almType == M.SECOND then
    minute = addAlarmBit(minute)
    hour = addAlarmBit(hour)
    date = addAlarmBit(date)
  elseif almType == M.MINUTE then
    hour = addAlarmBit(hour)
    date = addAlarmBit(date)
  elseif almType == M.HOUR then
    date = addAlarmBit(date)
  elseif almType == M.DAY then
    date = addAlarmBit(date,1)
  end
  local almStart = 0x07
  if almId == 2 then almStart = 0x0B end
  i2c.start(id)
  i2c.address(id, dev_addr, i2c.TRANSMITTER)
  i2c.write(id, almStart)
  if almId == 1 then i2c.write(id, second) end
  i2c.write(id, minute)
  i2c.write(id, hour)
  i2c.write(id, date)
  i2c.stop(id)
  print('[LOG] Alarm '..almId..' setted')
end

-- Get Control and Status bytes
function M.getBytes ()
  i2c.start(id)
  i2c.address(id, dev_addr, i2c.TRANSMITTER)
  i2c.write(id, 0x0E)
  i2c.stop(id)
  i2c.start(id)
  i2c.address(id, dev_addr, i2c.RECEIVER)
  local c = i2c.read(id, 2)
  i2c.stop(id)
  return tonumber(string.byte(c, 1)), tonumber(string.byte(c, 2))
end

-- Resetting RTC Stop Flag
function M.resetStopFlag ()
  if bit == nil or bit.band == nil or bit.bor == nil then
    print("[ERROR] Module bit is required to reset stop flag")
    return nil
  end
  i2c.start(id)
  i2c.address(id, dev_addr, i2c.TRANSMITTER)
  i2c.write(id, 0x0F)
  i2c.stop(id)
  i2c.start(id)
  i2c.address(id, dev_addr, i2c.RECEIVER)
  local s = string.byte(i2c.read(id, 1))
  i2c.stop(id)
  s = bit.band(s,127)
  i2c.start(id)
  i2c.address(id, dev_addr, i2c.TRANSMITTER)
  i2c.write(id, 0x0F)
  i2c.write(id, s)
  i2c.stop(id)
  print('[LOG] RTC stop flag resetted')
end

return M

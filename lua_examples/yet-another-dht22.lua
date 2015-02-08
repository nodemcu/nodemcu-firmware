------------------------------------------------------------------------------
-- DHT11/22 query module
--
-- LICENCE: http://opensource.org/licenses/MIT
-- Vladimir Dronnikov <dronnikov@gmail.com>
--
-- Example:
-- print("DHT11", dofile("dht22.lua").read(4))
-- print("DHT22", dofile("dht22.lua").read(4, true))
-- NB: the very first read sometimes fails
------------------------------------------------------------------------------
local M
do
  -- cache
  local gpio = gpio
  local val = gpio.read
  local waitus = tmr.delay
  --
  local read = function(pin, dht22)
    -- wait for pin value
    local w = function(v)
      local c = 255
      while c > 0 and val(pin) ~= v do c = c - 1 end
      return c
    end
    -- NB: we preallocate incoming data buffer
    --     or precise timing in reader gets broken
    local b = { 0, 0, 0, 0, 0 }

    -- kick the device
    gpio.mode(pin, gpio.INPUT, gpio.PULLUP)
    gpio.write(pin, 1)
    waitus(10)
    gpio.mode(pin, gpio.OUTPUT)
    gpio.write(pin, 0)
    waitus(20000)
    gpio.write(pin, 1)
    gpio.mode(pin, gpio.INPUT, gpio.PULLUP)
    -- wait for device presense
    if w(0) == 0 or w(1) == 0 or w(0) == 0 then
      return nil, 0
    end
    -- receive 5 octets of data, msb first
    for i = 1, 5 do
      local x = 0
      for j = 1, 8 do
        x = x + x
        if w(1) == 0 then return nil, 1 end
        -- 70us for 1, 27 us for 0
        waitus(30)
        if val(pin) == 1 then
          x = x + 1
          if w(0) == 0 then return nil, 2 end
        end
      end
      b[i] = x
    end
    -- check crc. NB: calculating in receiver loop breaks timings
    local crc = 0
    for i = 1, 4 do
      crc = (crc + b[i]) % 256
    end
    if crc ~= b[5] then return nil, 3 end
    -- convert
    local t, h
    -- DHT22: values in tenths of unit, temperature can be negative
    if dht22 then
      h = b[1] * 256 + b[2]
      t = b[3] * 256 + b[4]
      if t > 0x8000 then t = -(t - 0x8000) end
    -- DHT11: no negative temperatures, only integers
    -- NB: return in 0.1 Celsius
    else
      h = 10 * b[1]
      t = 10 * b[3]
    end
    return t, h
  end
  -- expose interface
  M = {
    read = read,
  }
end
return M

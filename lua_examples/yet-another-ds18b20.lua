------------------------------------------------------------------------------
-- DS18B20 query module
--
-- LICENCE: http://opensource.org/licenses/MIT
-- Vladimir Dronnikov <dronnikov@gmail.com>
--
-- Example:
-- for k, v in pairs(require("ds18b20").read(4)) do print(k, v) end
------------------------------------------------------------------------------
local M
do
  local format_addr = function(a)
    return ("%02x-%02x%02x%02x%02x%02x%02x"):format(
        a:byte(1),
        a:byte(7), a:byte(6), a:byte(5),
        a:byte(4), a:byte(3), a:byte(2)
      )
  end
  local read = function(pin, delay)
    local ow = require("ow")
    -- get list of relevant devices
    local d = { }
    ow.setup(pin)
    ow.reset_search(pin)
    while true do
      tmr.wdclr()
      local a = ow.search(pin)
      if not a then break end
      if ow.crc8(a) == 0 and
        (a:byte(1) == 0x10 or a:byte(1) == 0x28)
      then
        d[#d + 1] = a
      end
    end
    -- conversion command for all
    ow.reset(pin)
    ow.skip(pin)
    ow.write(pin, 0x44, 1)
    -- wait a bit
    tmr.delay(delay or 100000)
    -- iterate over devices
    local r = { }
    for i = 1, #d do
      tmr.wdclr()
      -- read rom command
      ow.reset(pin)
      ow.select(pin, d[i])
      ow.write(pin, 0xBE, 1)
      -- read data
      local x = ow.read_bytes(pin, 9)
      if ow.crc8(x) == 0 then
        local t = (x:byte(1) + x:byte(2) * 256) * 625
        -- NB: temperature in Celcius * 10^4
        r[format_addr(d[i])] = t
        d[i] = nil
      end
    end
    return r
  end
  -- expose
  M = {
    read = read,
  }
end
return M

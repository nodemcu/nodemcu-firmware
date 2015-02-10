------------------------------------------------------------------------------
-- DS18B20 query module
--
-- LICENCE: http://opensource.org/licenses/MIT
-- Vladimir Dronnikov <dronnikov@gmail.com>
--
-- Example:
-- dofile("ds18b20.lua").read(4, function(r) for k, v in pairs(r) do print(k, v) end end)
------------------------------------------------------------------------------
local M
do
  local bit = bit
  local format_addr = function(a)
    return ("%02x-%02x%02x%02x%02x%02x%02x"):format(
        a:byte(1),
        a:byte(7), a:byte(6), a:byte(5),
        a:byte(4), a:byte(3), a:byte(2)
      )
  end
  local read = function(pin, cb, delay)
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
    tmr.alarm(0, delay or 100, 0, function()
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
          local t = (x:byte(1) + x:byte(2) * 256)
          -- negatives?
          if bit.isset(t, 15) then t = 1 - bit.bxor(t, 0xffff) end
          -- NB: temperature in Celsius * 10^4
          t = t * 625
          -- NB: due 850000 means bad pullup. ignore
          if t ~= 850000 then
            r[format_addr(d[i])] = t
          end
          d[i] = nil
        end
      end
      cb(r)
    end)
  end
  -- expose
  M = {
    read = read,
  }
end
return M

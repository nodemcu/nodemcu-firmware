--------------------------------------------------------------------------------
-- LM75 I2C module for NODEMCU
-- 
-- LICENCE: http://opensource.org/licenses/MIT
-- Zhiyuan Wan <rgwan@rocaloid.org>
--------------------------------------------------------------------------------

local moduleName = ...
local M = {}
_G[moduleName] = M

-- Default value for i2c communication
local id = 0

--device address
local dev_addr = 0x48

-- initialize i2c
--parameters:
--d: sda
--l: scl
function M.init(d, l)
  if (d ~= nil) and (l ~= nil) and (d >= 0) and (d <= 11) and (l >= 0) and ( l <= 11) and (d ~= l) then
    sda = d
    scl = l
  else
    print("iic config failed!") return nil
  end
    print("init done")
    i2c.setup(id, sda, scl, i2c.SLOW)
end

--get temperature from LM75
function M.getTemperature()
    i2c.start(id)
    i2c.address(id, dev_addr, i2c.TRANSMITTER)
    i2c.write(id, 0)
    i2c.stop(id)
    i2c.start(id)
    i2c.address(id, dev_addr, i2c.RECEIVER)
    a = i2c.read(id, 2)
    i2c.stop(id)
    return bit.lshift(tonumber(string.byte(a, 1)), 1) + bit.rshift(tonumber(string.byte(a, 2)), 7)) / 2
end

return M

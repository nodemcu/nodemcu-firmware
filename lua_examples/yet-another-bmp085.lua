------------------------------------------------------------------------------
-- BMP085 query module
--
-- LICENCE: http://opensource.org/licenses/MIT
-- Vladimir Dronnikov <dronnikov@gmail.com>
-- Heavily based on work of Christee <Christee@nodemcu.com>
--
-- Example:
-- dofile("bmp085.lua").read(sda, scl)
------------------------------------------------------------------------------
local M
do
  -- cache
  local i2c, tmr = i2c, tmr
  -- helpers
  local r8 = function(reg)
    i2c.start(0)
    i2c.address(0, 0x77, i2c.TRANSMITTER)
    i2c.write(0, reg)
    i2c.stop(0)
    i2c.start(0)
    i2c.address(0, 0x77, i2c.RECEIVER)
    local r = i2c.read(0, 1)
    i2c.stop(0)
    return r:byte(1)
  end
  local w8 = function(reg, val)
    i2c.start(0)
    i2c.address(0, 0x77, i2c.TRANSMITTER)
    i2c.write(0, reg)
    i2c.write(0, val)
    i2c.stop(0)
  end
  local r16u = function(reg)
    return r8(reg) * 256 + r8(reg + 1)
  end
  local r16 = function(reg)
    local r = r16u(reg)
    if r > 32767 then r = r - 65536 end
    return r
  end
  -- calibration data
  local AC1, AC2, AC3, AC4, AC5, AC6, B1, B2, MB, MC, MD
  -- read
  local read = function(sda, scl, oss)
    i2c.setup(0, sda, scl, i2c.SLOW)
    -- cache calibration data
    if not AC1 then
      AC1 = r16(0xAA)
      AC2 = r16(0xAC)
      AC3 = r16(0xAE)
      AC4 = r16u(0xB0)
      AC5 = r16u(0xB2)
      AC6 = r16u(0xB4)
      B1  = r16(0xB6)
      B2  = r16(0xB8)
      MB  = r16(0xBA)
      MC  = r16(0xBC)
      MD  = r16(0xBE)
    end
    -- get raw P
    if not oss then oss = 0 end
    if oss <= 0 then oss = 0 end
    if oss > 3 then oss = 3 end
    w8(0xF4, 0x34 + 64 * oss)
    tmr.delay((4 + 3 ^ oss) * 1000)
    local p = r8(0xF6) * 65536 + r8(0xF7) * 256 + r8(0xF8)
    p = p / 2 ^ (8 - oss)
    -- get T
    w8(0xF4, 0x2E)
    tmr.delay(5000)
    local t = r16(0xF6)
    local X1 = (t - AC6) * AC5 / 32768
    local X2 = MC * 2048 / (X1 + MD)
    t = (X2 + X1 + 8) / 16
    -- normalize P
    local B5 = t * 16 - 8;
    local B6 = B5 - 4000
    local X1 = B2 * (B6 * B6 / 4096) / 2048
    local X2 = AC2 * B6 / 2048
    local X3 = X1 + X2
    local B3 = ((AC1 * 4 + X3) * 2 ^ oss + 2) / 4
    X1 = AC3 * B6 / 8192
    X2 = (B1 * (B6 * B6 / 4096)) / 65536
    X3 = (X1 + X2 + 2) / 4
    local B4 = AC4 * (X3 + 32768) / 32768
    local B7 = (p - B3) * (50000 / 2 ^ oss)
    p = B7 / B4 * 2
    X1 = (p / 256) ^ 2
    X1 = (X1 * 3038) / 65536
    X2 = (-7357 * p) / 65536
    p = p + (X1 + X2 + 3791) / 16
    -- Celsius * 10, Hg mm * 10
    return t, p * 3 / 40
  end
  -- expose
  M = {
    read = read,
  }
end
return M

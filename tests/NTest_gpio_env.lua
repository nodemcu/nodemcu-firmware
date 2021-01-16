-- Walk the GPIO subsystem through its paces, using the attached I2C GPIO chip
--
--   Node GPIO 13 (index 7) is connected to I2C expander channel B6; node OUT
--   Node GPIO 15 (index 8) is connected to I2C expander channel B7; node IN

local N = ...
N = (N or require "NTest")("gpio-env")

-- TODO: Preflight test that we are in the correct environment with an I2C
-- expander in the right place with the right connections.

-- TODO: Use the mcp23017 module in the main tree rather than hand-coding
-- the commands

N.test('setup', function()
  -- Set gpio pin directions
  gpio.mode(8, gpio.INPUT)
  gpio.mode(7, gpio.OUTPUT, gpio.FLOAT)

  -- Configure the I2C bus
  i2c.setup(0, 2, 1, i2c.FAST)

  -- Set the IO expander port B to channel 7 as output, 6 as input
  i2c.start(0)
  ok(i2c.address(0, 0x20, i2c.TRANSMITTER))
  i2c.write(0, 0x01, 0x7F)
  i2c.stop(0)
end)

local function seti2cb7(v)
  i2c.start(0)
  i2c.address(0, 0x20, i2c.TRANSMITTER)
  i2c.write(0, 0x15, v and 0x80 or 0x00)
  i2c.stop(0)
end

local function geti2cb6()
  i2c.start(0)
  i2c.address(0, 0x20, i2c.TRANSMITTER)
  i2c.write(0, 0x13)
  i2c.start(0)
  i2c.address(0, 0x20, i2c.RECEIVER)
  local v = i2c.read(0, 1):byte(1)
  i2c.stop(0)
  return (bit.band(v,0x40) ~= 0)
end

N.test('gpio read 0', function()
  seti2cb7(false)
  ok(eq(0, gpio.read(8)))
end)

N.test('gpio read 1', function()
  seti2cb7(true)
  ok(eq(1, gpio.read(8)))
end)

N.test('i2c read 0', function()
  gpio.write(7, 0)
  ok(eq(false, geti2cb6()))
end)

N.test('i2c read 1', function()
  gpio.write(7, 1)
  ok(eq(true, geti2cb6()))
end)

N.testasync('gpio toggle trigger 1', function(next)
  seti2cb7(false)
  tmr.delay(10)
  gpio.trig(8, "both", function(l,_,c)
    ok(c == 1 and l == 1)
    return next()
  end)
  seti2cb7(true)
end, true)

N.testasync('gpio toggle trigger 2', function(next)
  gpio.trig(8, "both", function(l,_,c)
    ok(c == 1 and l == 0)
    return next()
  end)
  seti2cb7(false)
end, true)

N.test('gpio toggle trigger end', function()
  gpio.trig(8, "none")
  ok(true)
end)

-- Walk the ADC through a stepped triangle wave using the attached voltage
-- divider and I2C GPIO expander.

local N = require('NTest')("adc-env")

-- TODO: Preflight test that we are in the correct environment with an I2C
-- expander in the right place with the right connections.

-- TODO: Use the mcp23017 module in the main tree rather than hand-coding
-- the commands

N.test('setup', function()
  -- Configure the ADC
  if adc.force_init_mode(adc.INIT_ADC)
  then
    node.restart()
    error "Must reboot to get to ADC mode"
  end

  -- Configure the I2C bus
  i2c.setup(0, 2, 1, i2c.FAST)

  -- Set the IO expander port B to channels 0 and 1 as outputs
  i2c.start(0)
  ok(i2c.address(0, 0x20, i2c.TRANSMITTER))
  i2c.write(0, 0x01, 0xFC)
  i2c.stop(0)
end)

-- set the two-bit voltage divider output value to v (in 0,1,2,3)
local function setv(v)
  assert (0 <= v and v <= 3)
  i2c.start(0)
  i2c.address(0, 0x20, i2c.TRANSMITTER)
  i2c.write(0, 0x15, v)
  i2c.stop(0)
end

-- read out the ADC and compare to given range
local function checkadc(min, max)
  local v = adc.read(0)
  return ok(min <= v and v <= max, ("read adc: %d <= %d <= %d"):format(min,v,max))
end

-- Since we have a rail-to-rail 4-tap DAC, as it were, give us some one-sided
-- wiggle around either rail and some two-sided wiggle around both middle stops
local vmin = {  0, 300, 700, 1000 }
local vmax = { 24, 400, 800, 1024 }

-- Set the DAC, wait a brief while for physics, and then read the ADC
local function mktest(fs, i)
  N.test(fs:format(i), function()
    setv(i)
    tmr.delay(10)
    checkadc(vmin[i+1], vmax[i+1])
  end)
end

-- test all four stops on the way up, and the three to come back down
for i=0,3    do mktest("%d up", i) end
for i=2,0,-1 do mktest("%d down", i) end

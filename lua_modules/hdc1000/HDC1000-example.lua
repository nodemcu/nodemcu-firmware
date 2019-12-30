local HDC1000 = require("HDC1000")

local sda, scl = 1, 2
local drdyn = false

do
  i2c.setup(0, sda, scl, i2c.SLOW)  -- call i2c.setup() only once
  HDC1000.setup(drdyn)
  -- prototype is config(address, resolution, heater)
  HDC1000.config() -- default values are used if called with no arguments.

  print(string.format("Temperature: %.2f °C\nHumidity: %.2f %%", HDC1000.getTemp(), HDC1000.getHumi()))

  -- Don't forget to release it after use
  HDC1000 = nil  -- luacheck: no unused
  package.loaded["HDC1000"] = nil
end
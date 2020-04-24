local ds3231 = require("ds3231")

-- ESP-01 GPIO Mapping
local gpio0, gpio2 = 3, 4

do
  i2c.setup(0, gpio0, gpio2, i2c.SLOW) -- call i2c.setup() only once

  local second, minute, hour, day, date, month, year = ds3231.getTime();  -- luacheck: no unused

  -- Get current time
  print(string.format("Time & Date: %s:%s:%s %s/%s/%s", hour, minute, second, date, month, year))

  -- Don't forget to release it after use
  ds3231 = nil -- luacheck: no unused
  package.loaded["ds3231"] = nil
end
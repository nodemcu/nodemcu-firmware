local ds3231 = require("ds3231")

-- ESP-01 GPIO Mapping
local gpio0, gpio2 = 3, 4

do
  i2c.setup(0, gpio0, gpio2, i2c.SLOW) -- call i2c.setup() only once

  local second, minute, hour, _, date, month, year = ds3231.getTime();

  -- Get current time
  print(string.format("Time & Date: %s:%s:%s %s/%s/%s", hour, minute, second, date, month, year))

  -- Don't forget to release it after use
  -- luacheck: push ignore
  ds3231 = nil
  package.loaded["ds3231"] = nil
  -- luacheck: pop
end
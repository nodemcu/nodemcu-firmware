-- ***************************************************************************
-- BH1750 Example Program for ESP8266 with nodeMCU
-- BH1750 compatible tested 2015-1-30
--
-- Written by xiaohu
--
-- MIT license, http://opensource.org/licenses/MIT
-- ***************************************************************************
local bh1750 = require("bh1750")

local sda = 6 -- sda pin, GPIO12
local scl = 5 -- scl pin, GPIO14

do
  bh1750.init(sda, scl)

  tmr.create():alarm(10000, tmr.ALARM_AUTO, function()
      bh1750.read()
      local l = bh1750.getlux()
      print("lux: "..(l / 100).."."..(l % 100).." lx")
  end)
end

-- ***************************************************************************
-- BH1750 Example Program for ESP8266 with nodeMCU
-- BH1750 compatible tested 2015-1-30
--
-- Written by xiaohu
--
-- MIT license, http://opensource.org/licenses/MIT
-- ***************************************************************************
tmr.alarm(0, 10000, 1, function()

    SDA_PIN = 6 -- sda pin, GPIO12
    SCL_PIN = 5 -- scl pin, GPIO14

    bh1750 = require("bh1750")
    bh1750.init(SDA_PIN, SCL_PIN)
    bh1750.read(OSS)
    l = bh1750.getlux()
    print("lux: "..(l / 100).."."..(l % 100).." lx")

    -- release module
    bh1750 = nil
    package.loaded["bh1750"]=nil

end)

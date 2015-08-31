-- ***************************************************************************
-- TSL2561 Example Program for ESP8266 with nodeMCU
--
-- Written by Marius Schmeding
--
-- MIT license, http://opensource.org/licenses/MIT
-- ***************************************************************************
tmr.alarm(0, 5000, 1, function()

    SDA_PIN = 6 -- sda pin
    SCL_PIN = 5 -- scl pin

    -- init module
    tsl2561 = require("tsl2561")
    tsl2561.init(SDA_PIN, SCL_PIN)

    -- read value
    l = tsl2561.readVisibleLux()
    print("lux: "..l.." lx")

    -- release module
    tsl2561 = nil
    package.loaded["tsl2561"]=nil

end)
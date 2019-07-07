-- ***************************************************************************
-- Example for Yeelink Lib
--
-- Written by Martin
--
--
-- MIT license, http://opensource.org/licenses/MIT
-- ***************************************************************************

wifi.setmode(wifi.STATION)         --Step1: Connect to Wifi
wifi.sta.config("SSID","Password")

local dht = require("dht_lib")           --Step2: "Require" the libs
local yeelink = require("yeelink_lib")

yeelink.init(23333,23333,"You api-key",function()  --Step3: Register the callback function

  print("Yeelink Init OK...")
  tmr.create():alarm(60000, tmr.ALARM_AUTO, function()   --Step4: Have fun~ (Update your data)

    dht.read(4)
    yeelink.update(dht.getTemperature())

  end)
end)

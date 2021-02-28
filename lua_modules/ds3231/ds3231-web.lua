local ds3231 = require('ds3231')

-- ESP-01 GPIO Mapping
local gpio0, gpio2 = 3, 4
local port = 80
local days = {
  [1] = "Sunday",
  [2] = "Monday",
  [3] = "Tuesday",
  [4] = "Wednesday",
  [5] = "Thursday",
  [6] = "Friday",
  [7] = "Saturday"
}

local months = {
  [1] = "January",
  [2] = "Febuary",
  [3] = "March",
  [4] = "April",
  [5] = "May",
  [6] = "June",
  [7] = "July",
  [8] = "August",
  [9] = "September",
  [10] = "October",
  [11] = "November",
  [12] = "December"
}

do
  i2c.setup(0, gpio0, gpio2, i2c.SLOW) -- call i2c.setup() only once

  local srv = net.createServer(net.TCP)
  srv:listen(port, function(conn)
    local second, minute, hour, day, date, month, year = ds3231.getTime()
    local prettyTime = string.format("%s, %s %s %s %s:%s:%s",
      days[day], date, months[month], year, hour, minute, second)

    conn:send("HTTP/1.1 200 OK\nContent-Type: text/html\nRefresh: 5\n\n" ..
      "<!DOCTYPE HTML>" ..
      "<html><body>" ..
      "<b>ESP8266</b></br>" ..
      "Time and Date: " .. prettyTime .. "<br>" ..
      "Node ChipID : " .. node.chipid() .. "<br>" ..
      "Node MAC : " .. wifi.sta.getmac() .. "<br>" ..
      "Node Heap : " .. node.heap() .. "<br>" ..
      "Timer Ticks : " .. tmr.now() .. "<br>" ..
      "</html></body>")

    conn:on("sent",function(sck) sck:close() end)
  end)
end

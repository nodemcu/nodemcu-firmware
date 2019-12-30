local t = require('ds18b20')

local port = 80
local pin = 3 -- gpio0 = 3, gpio2 = 4
local gconn = {} -- local variable for connection

local function readout(temps)
  local resp = "HTTP/1.1 200 OK\nContent-Type: text/html\nRefresh: 5\n\n" ..
    "<!DOCTYPE HTML>" ..
    "<html><body>" ..
    "<b>ESP8266</b></br>"

  for addr, temp in pairs(temps) do
    resp = resp .. string.format("Sensor %s: %s &#8451</br>",
      ('%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X '):format(addr:byte(1,8)), temp)
  end

  resp = resp ..
    "Node ChipID: " .. node.chipid() .. "<br>" ..
    "Node MAC: " .. wifi.sta.getmac() .. "<br>" ..
    "Node Heap: " .. node.heap() .. "<br>" ..
    "Timer Ticks: " .. tmr.now() .. "<br>" ..
    "</html></body>"

  gconn:send(resp)
  gconn:on("sent",function(conn) conn:close() end)
end

do
  local srv = net.createServer(net.TCP)
  srv:listen(port,
    function(conn)
      gconn = conn
      -- t:read_temp(readout) -- default pin value is 3
      t:read_temp(readout, pin)
    end)
end

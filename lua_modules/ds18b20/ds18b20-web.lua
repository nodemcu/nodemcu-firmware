require('ds18b20')

port = 80

-- ESP-01 GPIO Mapping
gpio0, gpio2 = 3, 4

ds18b20.setup(gpio0)

srv=net.createServer(net.TCP)
srv:listen(port,
     function(conn)
          conn:send("HTTP/1.1 200 OK\nContent-Type: text/html\nRefresh: 5\n\n" ..
              "<!DOCTYPE HTML>" ..
              "<html><body>" ..
              "<b>ESP8266</b></br>" ..
              "Temperature : " .. ds18b20.read() .. "<br>" ..
              "Node ChipID : " .. node.chipid() .. "<br>" ..
              "Node MAC : " .. wifi.sta.getmac() .. "<br>" ..
              "Node Heap : " .. node.heap() .. "<br>" ..
              "Timer Ticks : " .. tmr.now() .. "<br>" ..
              "</html></body>")          
          conn:on("sent",function(conn) conn:close() end)
     end
)



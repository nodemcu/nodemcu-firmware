-- ***************************************************************************
-- BH1750 Example Program for ESP8266 with nodeMCU
-- BH1750 compatible tested 2015-1-30
--
-- Written by xiaohu
--
-- MIT license, http://opensource.org/licenses/MIT
-- ***************************************************************************
--Updata to Lelian

--Ps 需要改动的地方LW_GATEWAY（乐联的设备标示），USERKEY（乐联userkey）
--Ps You nees to rewrite the LW_GATEWAY（Lelian's Device ID），USERKEY（Lelian's userkey）

tmr.alarm(0, 60000, 1, function()
            SDA_PIN = 6 -- sda pin, GPIO12
            SCL_PIN = 5 -- scl pin, GPIO14

			BH1750 = require("BH1750")
			BH1750.init(SDA_PIN, SCL_PIN)
			BH1750.read(OSS)
			l = BH1750.getlux()

        --定义数据变量格式 Define the veriables formate
        PostData = "[{\"Name\":\"T\",\"Value\":\"" ..(l / 100).."."..(l % 100).."\"}]"
        --创建一个TCP连接 Create a TCP Connection
        socket=net.createConnection(net.TCP, 0)
        --域名解析IP地址并赋值  DNS...it 
        socket:dns("www.lewei50.com", function(conn, ip)
                ServerIP = ip
                print("Connection IP:" .. ServerIP)
                end)

--开始连接服务器  Connect the sever
socket:connect(80, ServerIP)
 socket:on("connection", function(sck) end)

--HTTP请求头定义  HTTP Head
socket:send("POST /api/V1/gateway/UpdateSensors/LW_GATEWAY HTTP/1.1\r\n" ..
 "Host: www.lewei50.com\r\n" ..
 "Content-Length: " .. string.len(PostData) .. "\r\n" ..
 "userkey: USERKEY\r\n\r\n" ..
 PostData .. "\r\n")

--HTTP响应内容  Print the HTTP response
socket:on("receive", function(sck, response)
 print(response)
 end)
 end)




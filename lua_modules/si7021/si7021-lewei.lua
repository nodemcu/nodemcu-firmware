
    --创建一个定时器
    tmr.alarm(0, 60000, 1, function()

		SDA_PIN = 6 -- sda pin, GPIO12
		SCL_PIN = 5 -- scl pin, GPIO14

		si7021 = require("si7021")
		si7021.init(SDA_PIN, SCL_PIN)
		si7021.read(OSS)
		Hum = si7021.getHumidity()
		Temp = si7021.getTemperature()

    	--定义数据变量格式
    	PostData = "[{\"Name\":\"T\",\"Value\":\"" .. (Temp/100).."."..(Temp%100) .. "\"},{\"Name\":\"H\",\"Value\":\"" .. (Hum/100).."."..(Hum%100) .. "\"}]"
    	--创建一个TCP连接
    	socket=net.createConnection(net.TCP, 0)
    	--域名解析IP地址并赋值
    	socket:dns("www.lewei50.com", function(conn, ip)
    		ServerIP = ip
    		print("Connection IP:" .. ServerIP)
    		end)
    	--开始连接服务器
    	socket:connect(80, ServerIP)
    	socket:on("connection", function(sck) end)
    	--HTTP请求头定义
    	socket:send("POST /api/V1/gateway/UpdateSensors/yourID HTTP/1.1\r\n" ..
    				"Host: www.lewei50.com\r\n" ..
    				"Content-Length: " .. string.len(PostData) .. "\r\n" ..
    				"userkey: yourKEY\r\n\r\n" ..
    				PostData .. "\r\n")
    	--HTTP响应内容
    	socket:on("receive", function(sck, response)
    		print(response)
    		end)

		-- release module
		si7021 = nil
		package.loaded["si7021"]=nil

	    end)

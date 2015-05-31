--
-- Simple NodeMCU web server (done is a not so nodeie fashion :-)
--
-- Highly modified by Bruce Meacham, based on work by Scott Beasley 2015
-- Open and free to change and use. Enjoy. [Beasley/Meacham 2015]
--
-- Meacham Update: I streamlined/improved the parsing to focus on simple HTTP GET request and their simple parameters
--  Also added the code to drive a servo/light.  Comment out as you see fit.
--
-- Usage: 
--  Change SSID and SSID_PASSPHRASE for your wifi network
--  Download to NodeMCU
--  node.compile("http_server.lua")
--  dofile("http_server.lc")
--  When the server is esablished it will output the IP address.
--  	http://{ip address}/?s0=1200&light=1
--  s0 is the servo position (actually the PWM hertz), 500 - 2000 are all good values
--   light chanel high(1)/low(0), some evaluation boards have LEDs pre-wired in a "pulled high" confguration, so '0' ground the emitter and turns it on backwards.
-- 
--  Add to init.lua if you want it to autoboot.
--

-- Your Wifi connection data
local SSID = "YOUR WIFI SSID"
local SSID_PASSWORD = "YOUR SSID PASSPHRASE"

-- General setup
local pinLight = 2 -- this is GPIO4
gpio.mode(pinLight,gpio.OUTPUT)
gpio.write(pinLight,gpio.HIGH)

servo = {}
servo.pin = 4 --this is GPIO2
servo.value = 1500
servo.id = "servo"
gpio.mode(servo.pin, gpio.OUTPUT)
gpio.write(servo.pin, gpio.LOW)

-- This alarm drives the servo
tmr.alarm(0,10,1,function() -- 50Hz 
    if servo.value then -- generate pulse
        gpio.write(servo.pin, gpio.HIGH)
        tmr.delay(servo.value)
        gpio.write(servo.pin, gpio.LOW)
    end
end)

local function connect (conn, data)
   local query_data

   conn:on ("receive",
   	function (cn, req_data)
	  params = get_http_req (req_data)
         cn:send("HTTP/1.1 200/OK\r\nServer: NodeLuau\r\nContent-Type: text/html\r\n\r\n")
	  cn:send ("<h1>ESP8266 Servo &amp; Light Server</h1>\r\n")
	  if (params["light"] ~= nil) then
	         if ("0" == params["light"]) then
	         	gpio.write(pinLight, gpio.LOW)
	         else
			gpio.write(pinLight, gpio.HIGH)	         
	         end
         end
         
         if (params["s0"] ~= nil) then
         	servo.value = tonumber(params["s0"]);
         end

	  -- Close the connection for the request
         cn:close ( )
      end)
end

-- Build and return a table of the http request data
function get_http_req (instr)
	local t = {}
	local str = string.sub(instr, 0, 200)
	local v = string.gsub(split(str, ' ')[2], '+', ' ')
	parts = split(v, '?')
	local params = {}
	if (table.maxn(parts) > 1) then
		for idx,part in ipairs(split(parts[2], '&'))  do
			parmPart = split(part, '=')
			params[parmPart[1]] = parmPart[2]
		end
	end
	return params
end

-- Source: http://lua-users.org/wiki/MakingLuaLikePhp
-- Credit: http://richard.warburton.it/
function split(str, splitOn)
    if (splitOn=='') then return false end
    local pos,arr = 0,{}
    for st,sp in function() return string.find(str,splitOn,pos,true) end do
        table.insert(arr,string.sub(str,pos,st-1))
        pos = sp + 1
    end
    table.insert(arr,string.sub(str,pos))
    return arr
end

-- Configure the ESP as a station (client)
wifi.setmode (wifi.STATION)
wifi.sta.config (SSID, SSID_PASSWORD)
wifi.sta.autoconnect (1)

-- Hang out until we get a wifi connection before the httpd server is started.
tmr.alarm (1, 800, 1, function ( )
  if wifi.sta.getip ( ) == nil then
     print ("Waiting for Wifi connection")
  else
     tmr.stop (1)
     print ("Config done, IP is " .. wifi.sta.getip ( ))
  end
end)

-- Create the httpd server
svr = net.createServer (net.TCP, 30)

-- Server listening on port 80, call connect function if a request is received
svr:listen (80, connect)

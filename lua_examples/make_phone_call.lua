--[[
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
documentation files (the "Software"), to deal in the Software without restriction, 
including without limitation the rights to use, copy, modify, merge, publish, distribute, 
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is 
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be 
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR 
IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
]]--

-- Your access point's SSID and password
local SSID = "xxxxxx"
local SSID_PASSWORD = "xxxxxx"

-- configure ESP as a station
wifi.setmode(wifi.STATION)
wifi.sta.config(SSID,SSID_PASSWORD)
wifi.sta.autoconnect(1)

local TWILIO_ACCOUNT_SID = "xxxxxx"
local TWILIO_TOKEN = "xxxxxx"

local HOST = "iot-https-relay.appspot.com" -- visit http://iot-https-relay.appspot.com/ to learn more about this service
                                           -- Please be sure to understand the security issues of using this relay app and use at your own risk.
local URI = "/twilio/Calls.json"

function build_post_request(host, uri, data_table)

     local data = ""
     
     for param,value in pairs(data_table) do
          data = data .. param.."="..value.."&"
     end

     request = "POST "..uri.." HTTP/1.1\r\n"..
     "Host: "..host.."\r\n"..
     "Connection: close\r\n"..
     "Content-Type: application/x-www-form-urlencoded\r\n"..
     "Content-Length: "..string.len(data).."\r\n"..
     "\r\n"..
     data

     print(request)
     
     return request
end

local function display(sck,response)
     print(response)
end

-- When using send_sms: the "from" number HAS to be your twilio number.
-- If you have a free twilio account the "to" number HAS to be your twilio verified number.
local function make_call(from,to,body)

     local data = {
      sid = TWILIO_ACCOUNT_SID,
      token = TWILIO_TOKEN,
      Body = string.gsub(body," ","+"),
      From = from,
      To = to
     }
     
     socket = net.createConnection(net.TCP,0)
     socket:on("receive",display)
     socket:connect(80,HOST)

     socket:on("connection",function(sck) 
       
          local post_request = build_post_request(HOST,URI,data)
          sck:send(post_request)
     end)     
end

function check_wifi()
 local ip = wifi.sta.getip()

 if(ip==nil) then
   print("Connecting...")
 else
  tmr.stop(0)
  print("Connected to AP!")
  print(ip)
     -- make a call with a voice message "your house is on fire"
  make_call("15558976687","1334856679","Your house is on fire!") 
 end
 
end

tmr.alarm(0,2000,1,check_wifi)

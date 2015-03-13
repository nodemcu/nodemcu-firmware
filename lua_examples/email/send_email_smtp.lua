require("base64")

local MY_EMAIL = "esp8266@domain.com"
local EMAIL_PASSWORD = "123456"

local SMTP_SERVER = "smtp.server.com"
local SMTP_PORT = "587"

local mail_to = "to_email@domain.com"

local SSID = "ssid"
local SSID_PASSWORD = "password"

wifi.setmode(wifi.STATION)
wifi.sta.config(SSID,SSID_PASSWORD)
wifi.sta.autoconnect(1)

local email_subject = ""
local email_body = ""
local receive_processed = false
local count = 0

local smtp_socket = net.createConnection(net.TCP,0)

function display(sck,response)
     print(response)
end

function do_next()
            if(count == 0)then
                count = count+1
                local IP_ADDRESS = wifi.sta.getip()
                smtp_socket:send("HELO "..IP_ADDRESS.."\r\n")
            elseif(count==1) then
                count = count+1
                smtp_socket:send("AUTH LOGIN\r\n")
            elseif(count == 2) then
                count = count + 1
                smtp_socket:send(base64.enc(MY_EMAIL).."\r\n")
            elseif(count == 3) then
                count = count + 1
                smtp_socket:send(base64.enc(EMAIL_PASSWORD).."\r\n")
            elseif(count==4) then
                count = count+1
               smtp_socket:send("MAIL FROM:<" .. MY_EMAIL .. ">\r\n")
            elseif(count==5) then
                count = count+1
               smtp_socket:send("RCPT TO:<" .. mail_to ..">\r\n")
            elseif(count==6) then
                count = count+1
               smtp_socket:send("DATA\r\n")
            elseif(count==7) then
                count = count+1
                local message = string.gsub(
                "From: \"".. MY_EMAIL .."\"<"..MY_EMAIL..">\r\n" ..
                "To: \"".. mail_to .. "\"<".. mail_to..">\r\n"..
                "Subject: ".. email_subject .. "\r\n\r\n"  ..
                email_body,"\r\n.\r\n","")
                
                smtp_socket:send(message.."\r\n.\r\n")
            elseif(count==8) then
               count = count+1
                 tmr.stop(0)
                 smtp_socket:send("QUIT\r\n")
            else
               smtp_socket:close()
            end
end

function connected(sck)
    tmr.alarm(0,5000,1,do_next)
end

function send_email(subject,body)
     email_subject = subject
     email_body = body
     smtp_socket:on("connection",connected)
     smtp_socket:on("receive",display)
     smtp_socket:connect(SMTP_PORT,SMTP_SERVER)
end

send_email(
     "ESP8266",
[[Hi,
How are your IoT projects coming along?
Best Wishes,
ESP8266]])










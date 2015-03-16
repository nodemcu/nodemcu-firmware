---
-- Working Example: https://www.youtube.com/watch?v=CcRbFIJ8aeU
-- @description a basic SMTP email example. You must use an account which can provide unencrypted authenticated access.
-- This example was tested with an AOL and Time Warner email accounts. GMail does not offer unecrypted authenticated access.
-- To obtain your email's SMTP server and port simply Google it e.g. [my email domain] SMTP settings
-- For example for timewarner you'll get to this page http://www.timewarnercable.com/en/support/faqs/faqs-internet/e-mailacco/incoming-outgoing-server-addresses.html
-- To Learn more about SMTP email visit:
-- SMTP Commands Reference - http://www.samlogic.net/articles/smtp-commands-reference.htm
-- See "SMTP transport example" in this page http://en.wikipedia.org/wiki/Simple_Mail_Transfer_Protocol
-- @author Miguel

require("base64")

-- The email and password from the account you want to send emails from
local MY_EMAIL = "esp8266@domain.com"
local EMAIL_PASSWORD = "123456"

-- The SMTP server and port of your email provider.
-- If you don't know it google [my email provider] SMTP settings
local SMTP_SERVER = "smtp.server.com"
local SMTP_PORT = "587"

-- The account you want to send email to
local mail_to = "to_email@domain.com"

-- Your access point's SSID and password
local SSID = "ssid"
local SSID_PASSWORD = "password"

-- configure ESP as a station
wifi.setmode(wifi.STATION)
wifi.sta.config(SSID,SSID_PASSWORD)
wifi.sta.autoconnect(1)

-- These are global variables. Don't change their values
-- they will be changed in the functions below
local email_subject = ""
local email_body = ""
local count = 0


local smtp_socket = nil -- will be used as socket to email server

-- The display() function will be used to print the SMTP server's response
function display(sck,response)
     print(response)
end

-- The do_next() function is used to send the SMTP commands to the SMTP server in the required sequence.
-- I was going to use socket callbacks but the code would not run callbacks after the first 3.
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

-- The connectted() function is executed when the SMTP socket is connected to the SMTP server.
-- This function will create a timer to call the do_next function which will send the SMTP commands
-- in sequence, one by one, every 5000 seconds. 
-- You can change the time to be smaller if that works for you, I used 5000ms just because.
function connected(sck)
    tmr.alarm(0,5000,1,do_next)
end

-- @name send_email
-- @description Will initiated a socket connection to the SMTP server and trigger the connected() function
-- @param subject The email's subject
-- @param body The email's body
function send_email(subject,body)
     count = 0
     email_subject = subject
     email_body = body
     smtp_socket = net.createConnection(net.TCP,0)
     smtp_socket:on("connection",connected)
     smtp_socket:on("receive",display)
     smtp_socket:connect(SMTP_PORT,SMTP_SERVER)
end

-- Send an email
send_email(
     "ESP8266",
[[Hi,
How are your IoT projects coming along?
Best Wishes,
ESP8266]])










---
-- @author Miguel (AllAboutEE.com)
-- @description Reads email via IMAP

-- this could be your email (and it is in most cases even in you have a "username")
local IMAP_USERNAME = "myemail@xxx.rr.com" 
local IMAP_PASSWORD = "mypassword"

local IMAP_SERVER = "mail.twc.com"
local IMAP_PORT = "143"
local IMAP_TAG = "t1"

local SSID = "ssid"
local SSID_PASSWORD = "password"

wifi.setmode(wifi.STATION)
wifi.sta.config(SSID,SSID_PASSWORD)
wifi.sta.autoconnect(1)


function print_logout(sck,logout_reply)
    print("=========== LOGOUT REPLY ==========\r\n")
    print(logout_reply)
end

function logout(sck)
    print("Logging out...\r\n")
    sck:on("receive",print_logout)
    sck:send(IMAP_TAG .. " LOGOUT\r\n")
end

function print_body(sck,email_body)
    print("========== EMAIL BODY =========== \r\n")
    print(email_body)
    print("\r\n")
    logout(sck)
end

function fetch_body(sck)
    print("\r\nFetching first mail body...\r\n")
    sck:on("receive",print_body)
    sck:send(IMAP_TAG .. " FETCH 1 BODY[TEXT]\r\n")
end


function print_header(sck, email_header)
    print("============== EMAIL HEADERS ==========\r\n")
    print(email_header) -- prints the email headers
    print("\r\n")
    fetch_body(sck)
end

function fetch_header(sck)
    print("\r\nFetching first mail headers...\r\n")
    sck:on("receive",print_header)
    sck:send(IMAP_TAG .. " FETCH 1 BODY[HEADER]\r\n")
end

function print_login(sck,login_reply)
    print("========== LOGIN REPLY ==========\r\n")
    print(login_reply)
    select(sck,"INBOX")
    
end

function print_select(sck,response)
    print(response)
    fetch_header(sck)
end

function select(sck,mailbox)
    print("Selecting inbox...\r\n")
    sck:on("receive",print_select)
    sck:send(IMAP_TAG .. " SELECT "..mailbox.."\r\n")
end



function login(sck)
    print("Logging in...\r\n")
    sck:on("receive", print_login)
    sck:send(IMAP_TAG .. " LOGIN " .. IMAP_USERNAME .. " ".. IMAP_PASSWORD.."\r\n")
end


local socket = net.createConnection(net.TCP,0)
socket:on("connection",login)
socket:connect(IMAP_PORT,IMAP_SERVER)

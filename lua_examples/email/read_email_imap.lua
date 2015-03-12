---
-- @author Miguel (AllAboutEE.com)
-- @description Reads email via IMAP

-- this could be your email (and it is in most cases even in you have a "username")
require("imap")

local IMAP_USERNAME = "myemail@domain.com" 
local IMAP_PASSWORD = "myemailpassword"

local IMAP_SERVER = "imap.domain.com"
local IMAP_PORT = "143"
local IMAP_TAG = "t1"

local SSID = "ssid"
local SSID_PASSWORD = "ssidpassword"


local count = 0

wifi.setmode(wifi.STATION)
wifi.sta.config(SSID,SSID_PASSWORD)
wifi.sta.autoconnect(1)

local imap_socket = net.createConnection(net.TCP,0)


function setup(sck)
    imap.config(IMAP_USERNAME,
            IMAP_PASSWORD,
            IMAP_TAG,
            sck)

    imap.login(imap_socket)
end

imap_socket:on("connection",setup)
imap_socket:connect(IMAP_PORT,IMAP_SERVER)

function do_next()

    if(imap.receive_complete() == true) then
    print("receive complete")

        if (count == 0) then
            print("Examine:\r\n")
            imap.examine(imap_socket,"INBOX")
            count = count + 1
        elseif (count == 1) then
            imap.fetch_header(imap_socket,1,"SUBJECT")
            count = count + 1
        elseif (count == 2) then
            imap.fetch_header(imap_socket,1,"FROM")
            count = count + 1
        elseif (count == 3) then
            imap.fetch_body_plain_text(imap_socket,1)
            count = count + 1
        elseif (count == 4) then
            imap.logout(imap_socket)
            count = count + 1
        else 
            print("The body is: ".. imap.get_body())
            tmr.stop(0)
            imap_socket:close()
            collectgarbage()
        end
    end

end

tmr.alarm(0,1000,1, do_next)

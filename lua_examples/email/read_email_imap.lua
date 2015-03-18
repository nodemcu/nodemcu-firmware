---
-- Working Example: https://www.youtube.com/watch?v=PDxTR_KJLhc
-- @author Miguel (AllAboutEE.com)
-- @description This example will read the first email in your inbox using IMAP and 
-- display it through serial. The email server must provided unecrypted access. The code
-- was tested with an AOL and Time Warner cable email accounts (GMail and other services who do
-- not support no SSL access will not work).

require("imap")

local IMAP_USERNAME = "email@domain.com" 
local IMAP_PASSWORD = "password"

-- find out your unencrypted imap server and port
-- from your email provided i.e. google "[my email service] imap settings" for example
local IMAP_SERVER = "imap.service.com"
local IMAP_PORT = "143"

local IMAP_TAG = "t1" -- You do not need to change this
local IMAP_DEBUG = true -- change to true if you would like to see the entire conversation between
                         -- the ESP8266 and IMAP server

local SSID = "ssid"
local SSID_PASSWORD = "password"


local count = 0 -- we will send several IMAP commands/requests, this variable helps keep track of which one to send

-- configure the ESP8266 as a station
wifi.setmode(wifi.STATION)
wifi.sta.config(SSID,SSID_PASSWORD)
wifi.sta.autoconnect(1)

-- create an unencrypted connection
local imap_socket = net.createConnection(net.TCP,0)


---
-- @name setup
-- @description A call back function used to begin reading email 
-- upon sucessfull connection to the IMAP server
function setup(sck)
    -- Set the email user name and password, IMAP tag, and if debugging output is needed
    imap.config(IMAP_USERNAME,
            IMAP_PASSWORD,
            IMAP_TAG,
            IMAP_DEBUG)

    imap.login(sck)
end

imap_socket:on("connection",setup) -- call setup() upon connection
imap_socket:connect(IMAP_PORT,IMAP_SERVER) -- connect to the IMAP server

local subject = ""
local from = ""
local message = ""

---
-- @name do_next
-- @description A call back function for a timer alarm used to check if the previous
-- IMAP command reply has been processed. If the IMAP reply has been processed
-- this function will call the next IMAP command function necessary to read the email
function do_next()

    -- Check if the IMAP reply was processed
    if(imap.response_processed() == true) then

        -- The IMAP reply was processed

        if (count == 0) then
            -- After logging in we need to select the email folder from which we wish to read
            -- in this case the INBOX folder
            imap.examine(imap_socket,"INBOX")
            count = count + 1
        elseif (count == 1) then
            -- After examining/selecting the INBOX folder we can begin to retrieve emails.
            imap.fetch_header(imap_socket,imap.get_most_recent_num(),"SUBJECT") -- Retrieve the SUBJECT of the first/newest email
            count = count + 1
        elseif (count == 2) then
            subject = imap.get_header() -- store the SUBJECT response in subject
            imap.fetch_header(imap_socket,imap.get_most_recent_num(),"FROM") -- Retrieve the FROM of the first/newest email
            count = count + 1
        elseif (count == 3) then
            from = imap.get_header() -- store the FROM response in from
            imap.fetch_body_plain_text(imap_socket,imap.get_most_recent_num()) -- Retrieve the BODY of the first/newest email
            count = count + 1
        elseif (count == 4) then
            body = imap.get_body() -- store the BODY response in body
            imap.logout(imap_socket) -- Logout of the email account
            count = count + 1
        else 
            -- display the email contents

            -- create patterns to strip away IMAP protocl text from actual message
            pattern1 = "(\*.+\}\r\n)" -- to remove "* n command (BODY[n] {n}"
            pattern2 = "(%)\r\n.+)" -- to remove ") t1 OK command completed"

            from = string.gsub(from,pattern1,"")
            from = string.gsub(from,pattern2,"")
            print(from)

            subject = string.gsub(subject,pattern1,"")
            subject = string.gsub(subject,pattern2,"")
            print(subject)

            body = string.gsub(body,pattern1,"")
            body = string.gsub(body,pattern2,"")
            print("Message: " .. body)
            
            tmr.stop(0) -- Stop the timer alarm
            imap_socket:close() -- close the IMAP socket
            collectgarbage() -- clean up
        end
    end

end

-- A timer alarm is sued to check if an IMAP reply has been processed
tmr.alarm(0,1000,1, do_next)

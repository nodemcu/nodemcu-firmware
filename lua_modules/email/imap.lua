---
-- Working Example: https://www.youtube.com/watch?v=PDxTR_KJLhc
-- IMPORTANT: run node.compile("imap.lua") after uploading this script 
-- to create a compiled module. Then run file.remove("imap.lua")
-- @name imap
-- @description An IMAP 4rev1 module that can be used to read email. 
-- Tested on NodeMCU 0.9.5 build 20150213.
-- @date March 12, 2015
-- @author Miguel 
--  GitHub: https://github.com/AllAboutEE 
--  YouTube: https://www.youtube.com/user/AllAboutEE
--  Website: http://AllAboutEE.com
--
-- Visit the following URLs to learn more about IMAP:
-- "How to test an IMAP server by using telnet" http://www.anta.net/misc/telnet-troubleshooting/imap.shtml
-- "RFC 2060 - Internet Message Access Protocol - Version 4rev1" http://www.faqs.org/rfcs/rfc2060.html
-------------------------------------------------------------------------------------------------------------
local moduleName = ... 
local M = {}
_G[moduleName] = M 

local USERNAME = "" 
local PASSWORD = ""

local SERVER = ""
local PORT = ""
local TAG = ""

local DEBUG = false

local body = "" -- used to store an email's body / main text
local header = "" -- used to store an email's last requested header field e.g. SUBJECT, FROM, DATA etc.
local most_recent_num = 1 -- used to store the latest/newest email number/id


local response_processed = false -- used to know if the last IMAP response has been processed

---
-- @name response_processed
-- @returns The response process status of the last IMAP command sent
function M.response_processed()
    return response_processed
end

---
-- @name display
-- @description A generic IMAP response processing function. 
-- Can disply the IMAP response if DEBUG is set to true.
-- Sets the reponse processed variable to true when the string "complete"
-- is found in the IMAP reply/response
local function display(socket, response)

    -- If debuggins is enabled print the IMAP response
    if(DEBUG) then
        print(response)
    end

    -- Some IMAP responses are long enough that they will cause the display 
    -- function to be called several times. One thing is certain, IMAP will replay with
    -- "<tag> OK <command> complete" when it's done sending data back.
    if(string.match(response,'complete') ~= nil) then
        response_processed = true
    end

end

--- 
-- @name config
-- @description Initiates the IMAP settings
function M.config(username,password,tag,debug)
    USERNAME = username
    PASSWORD = password
    TAG = tag
    DEBUG = debug
end

---
-- @name login
-- @descrpiton Logs into a new email session
function M.login(socket)
    response_processed = false -- we are sending a new command 
                               -- which means that the response for it has not been processed
    socket:send(TAG .. " LOGIN " .. USERNAME .. " " .. PASSWORD .. "\r\n")
    socket:on("receive",display)
end

---
-- @name get_most_recent_num
-- @returns The most recent email number. Should only be called after examine()
function M.get_most_recent_num()
    return most_recent_num
end

---
-- @name set_most_recent_num 
-- @description Gets the most recent email number from the EXAMINE command.
-- i.e. if EXAMINE returns "* 4 EXISTS" this means that there are 4 emails,
-- so the latest/newest will be identified by the number 4
local function set_most_recent_num(socket,response)

    if(DEBUG) then
        print(response)
    end

    local _, _, num = string.find(response,"([0-9]+) EXISTS(\.)") -- the _ and _ keep the index of the string found
                                                                  -- but we don't care about that.

    if(num~=nil) then 
        most_recent_num = num
    end

    if(string.match(response,'complete') ~= nil) then
        response_processed = true
    end
end

---
-- @name examine
-- @description IMAP examines the given mailbox/folder. Sends the IMAP EXAMINE command
function M.examine(socket,mailbox)

    response_processed = false
    socket:send(TAG .. " EXAMINE " .. mailbox .. "\r\n")
    socket:on("receive",set_most_recent_num)
end

---
-- @name get_header
-- @returns The last fetched header field
function M.get_header()
    return header
end

---
-- @name set_header
-- @description Records the IMAP header field response in a variable 
-- so that it may be read later
local function set_header(socket,response)
    if(DEBUG) then
        print(response)
    end

    header = header .. response
    if(string.match(response,'complete') ~= nil) then
        response_processed = true
    end
end

---
-- @name fetch_header
-- @description Fetches an emails header field e.g. SUBJECT, FROM, DATE
-- @param socket The IMAP socket to use
-- @param msg_number The email number to read e.g. 1 will read fetch the latest/newest email
-- @param field A header field such as SUBJECT, FROM, or DATE
function M.fetch_header(socket,msg_number,field)
    header = "" -- we are getting a new header so clear this variable
    response_processed = false
    socket:send(TAG .. " FETCH " .. msg_number .. " BODY[HEADER.FIELDS (" .. field .. ")]\r\n")
    socket:on("receive",set_header)
end


---
-- @name get_body
-- @return The last email read's body 
function M.get_body()
    return body
end

---
-- @name set_body
-- @description Records the IMAP body response in a variable 
-- so that it may be read later
local function set_body(socket,response)

    if(DEBUG) then
        print(response)
    end

    body = body .. response
    if(string.match(response,'complete') ~= nil) then
        response_processed = true
    end
end

---
-- @name fetch_body_plain_text
-- @description Sends the IMAP command to fetch a plain text version of the email's body
-- @param socket The IMAP socket to use
-- @param msg_number The email number to obtain e.g. 1 will obtain the latest email
function M.fetch_body_plain_text(socket,msg_number)
    response_processed = false
    body = "" -- clear the body variable since we'll be fetching a  new email
    socket:send(TAG .. " FETCH " .. msg_number .. " BODY[1]\r\n")
    socket:on("receive",set_body)
end

---
-- @name logout
-- @description Sends the IMAP command to logout of the email session
function M.logout(socket)
    response_processed = false
    socket:send(TAG .. " LOGOUT\r\n")
    socket:on("receive",display)
end

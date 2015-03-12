local moduleName = ... 
local M = {}
_G[moduleName] = M 

local USERNAME = "" 
local PASSWORD = ""

local SERVER = ""
local PORT = ""
local TAG = ""

local body = ""


local receive_complete = false

function M.receive_complete()
    return receive_complete
end


local function display(socket, response)
    print(response)
    if(string.match(response,'complete') ~= nil) then
        receive_complete = true
    end
end

function M.config(username,password,tag,sk)
    USERNAME = username
    PASSWORD = password
    TAG = tag
end

function M.login(socket)
    receive_complete = false
    socket:send(TAG .. " LOGIN " .. USERNAME .. " " .. PASSWORD .. "\r\n")
    socket:on("receive",display)
end

function M.examine(socket,mailbox)

    receive_complete = false
    socket:send(TAG .. " EXAMINE " .. mailbox .. "\r\n")
    socket:on("receive",display)
end

function M.fetch_header(socket,msg_number,field)

    receive_complete = false
    socket:send(TAG .. " FETCH " .. msg_number .. " BODY[HEADER.FIELDS (" .. field .. ")]\r\n")
    socket:on("receive",display)
end

function M.get_body()
    return body
end


local function set_body(socket,response)
    print(response)
    body = body .. response
    if(string.match(response,'complete') ~= nil) then
        receive_complete = true
    end
end

function M.fetch_body_plain_text(socket,msg_number)
receive_complete = false
    body = ""
    socket:send(TAG .. " FETCH " .. msg_number .. " BODY[1]\r\n")
    socket:on("receive",set_body)
end

function M.logout(socket)
    receive_complete = false
    socket:send(TAG .. " LOGOUT\r\n")
    socket:on("receive",display)
end

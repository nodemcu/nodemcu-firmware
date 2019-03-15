-- ChiliPeppr Websocket client library for Lua ESP32
-- This library lets you talk over a websocket using pure Lua
-- 
-- Visit http://chilipeppr.com/esp32
-- By John Lauer
--
-- There is no license needed to use/modify this software. It is given freely
-- to the open source community. Modify at will.
-- 
-- Working example of using this library:
-- https://www.youtube.com/watch?v=ITgh5epyPRk&t=119s
--
-- To use this library:
-- ws = require('websocket')
-- ws.on("receive", function(data, fin)
--   -- fin of 1 or nil means you got all data
--   -- fin of 0 means extended data will come in
--   print("Got data:" .. data .. ", fin:", fin)
-- end)
-- ws.on("connection", function(host, port, path)
--   print("Websocket connected to host:", host, "port:", port, "path:", path)
--   ws.send("list")
-- end)
-- ws.on("disconnection", function()
--   print("Websocket got disconnect from:", ws.wsUrl)
-- end)
-- ws.on("pingsend", function()
--   print("Ping")
-- end)
-- ws.on("pongrecv", function()
--   print("Got pong. We're alive.")
-- end)

-- -- Use ChiliPeppr wifi library to auto-connect to wifi
-- wf = require("esp32_wifi")
-- wf.on("connection", function(info)
--   print("Got wifi. IP:", info.ip, "Netmask:", info.netmask, "GW:", info.gw)
--   ws.init(info.ip)
--   -- This sample websocket is to Serial Port JSON server
--   -- Set this to your own 2nd SPJS, not the local one, or you'll get loopbacks
--   -- ws.connect("ws://10.0.0.201:8989/ws")
--   -- Example public websocket server
--   ws.connect("ws://demos.kaazing.com/echo")
-- end)
-- wf.init()

local m = {}
-- m = {}

-- provide your websocket url 
m.wsUrl = "ws://10.0.0.104:8980/ws"

-- private properties
m.isInitted = false
m.sock = nil
m.myIp = nil
m.isConnected = false
m.pingTmr = nil

---
-- @name init
-- @description Call this first with your ESP32's IP address which you
-- can get from the wifi module.
-- @param myip A string of your IP address like the format 10.0.0.5
-- @returns nil
function m.init(myip)
  
  if myip then m.myIp = myip end

  if m.myIp == nil then
    print("Websocket: You need to connect to wifi, or if you are, give me the IP address of this ESP32 in the init method.")
  else 
    print("Websocket initted. My IP: " .. m.myIp)
    m.isInitted = true
  end
end

m.onDataCallback = nil
m.onConnectedCallback = nil 
m.onDisconnectCallback = nil
m.onPingRecvCallback = nil
m.onPingSendCallback = nil
m.onPongRecvCallback = nil
---
-- @name on
-- @description Attach to the callback events available from this websocket library
-- @param event_name A string of the event you want a callback on. "receive", "connection", "disconnection", "pingrecv", "pingsend", "pongrecv"
-- @param func The callback to receive after the event
-- @returns nil
function m.on(method, func)
  if method == "receive" then
    m.onDataCallback = func 
  elseif method == "connection" then 
    m.onConnectedCallback = func 
  elseif method == "disconnection" then
    m.onDisconnectCallback = func
  elseif method == "pingrecv" then
    m.onPingRecvCallback = func
  elseif method == "pingsend" then
    m.onPingSendCallback = func
  elseif method == "pongrecv" then
    m.onPongRecvCallback = func
  end 
end

---
-- @name connect
-- @description Ask the websocket to connect to the websocket URL
-- @param wsurl A string of the websocket URL to connect like "ws://10.0.0.104:8980/ws"
-- @param callback A function to get called back after a connection is established to the websocket URL
-- @returns nil
function m.connect(wsurl, callbackOnConnected)
  
  if wsurl ~= nil then m.wsUrl = wsurl end
  if callbackOnConnected ~= nil then m.callbackOnConnected = callbackOnConnected end 
  
  -- Get host from websocket url 
  local host, port, path = string.match(m.wsUrl, 'ws://(.-):(.-)/(.*)')
  if host and port and path then
    path = "/" .. path 
  else
    host, path = string.match(m.wsUrl, 'ws://(.-)/(.*)')
    if host and path then
      port = 80
      path = "/" .. path 
    else
      host = string.match(m.wsUrl, 'ws://(.-)')
      if host then
        port = 80
        path = "/"
      else
        error("Could not parse websocket URL?", m.wsUrl)
      end
      
    end
  end
  
  print("Websocket connecting to:", host, "port:", port, "path:", path)
  
  local body = "GET " .. path .. " HTTP/1.1\r\n"
  body = body .. "Host: " .. host .. "\r\n"
  body = body .. "Upgrade: websocket\r\n"
  body = body .. "Connection: Upgrade\r\n"
  body = body .. "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
  body = body .. "Sec-WebSocket-Protocol: chat, superchat\r\n"
  body = body .. "Sec-WebSocket-Version: 13\r\n"
  body = body .. "Origin: esp32\r\n"
  body = body .. "\r\n"
  
  local sk = net.createConnection(net.TCP)
  
  local buffer
  local isHdrRecvd = false
  sk:on("receive", function(sck, c) 
    -- print("Got receive: " .. c)
    
    -- see if we are good to go here 
    if isHdrRecvd == false then 
      -- look for header    
      -- look for HTTP/1.1 101
      if string.match(c, "HTTP/1.1 101(.*)\r\n\r\n") then 
        print("Websocket found hdr")
        isHdrRecvd = true
        m.isConnected = true
        if m.onConnectedCallback ~= nil then 
          node.task.post(node.task.LOW_PRIORITY, function()
            m.onConnectedCallback(host, port, path) 
          end)
        end 
      end 
    else 
      -- we can start to deliver incoming data 
  
      m.decodeFrame(c)
    end 
    
  end)
  sk:on("sent", function()
    -- print("Websocket sent data")
  end)
  sk:on('disconnection', function(errcode)
    print("Websocket disconnection. err:", errcode)
    m.isConnected = false
    if m.onDisconnectCallback then m.onDisconnectCallback() end 
  end)
  sk:on('reconnection', function(errcode)
    print('Websocket reconnection. err:', errcode)
  end)
  sk:on("connection", function(sck, c)
    -- print("Websocket got connection. Sending TCP msg. body:")
    print(body)
    -- m.sock = sck 
    sck:send(body)
    
    m.pingStart()
  end)
  
  m.sock = sk
  sk:connect(port, host)
  
end

-- currently only supports minimal frame decode
m.lenExpected = 0
m.buffer = ""
-- private method
function m.decodeFrame(frame)
  
  local data, fin, opcode
  
  if m.lenExpected > 0 then 
    -- we should just raw append
    data = frame 
    -- print("Raw len:", string.len(data))
    -- m.lenExpected = m.lenExpected - string.len(data)
    
    -- append the new data to the previous data
    m.buffer = m.buffer .. data

  else
    -- we need to decode the headr from the frame 
  
    -- get FIN. 1 means msg is complete. 0 means multi-part
    fin = string.byte(frame, 1)
    fin = bit.isset(fin, 7)
    -- print("FIN:", fin)
    
    -- get opcode
    opcode = string.byte(frame, 1)
    opcode = bit.clear(opcode, 4, 5, 6, 7) -- clear FIN and RSV
    -- print("Opcode:", opcode)
    

    -- get 2nd byte as it has the payload length
    -- msb of byte is mask, remaining 7 bytes is len 
    local plen = string.byte(frame, 2)
    local mask = bit.isset(plen, 7)
    plen = bit.clear(plen, 7) -- remove the mask from the length
    -- print("Frame len:", plen, " Mask:", mask)
    m.lenExpected = plen
    
    if mask then 
      -- print("We should not get a mask from server. Error.")
      return
    end
    
    data = string.sub(frame, 3) -- remove first 2 bytes, i.e. start at 3rd char
  
    if plen == 126 then 
      -- read next 2 bytes
      local extlen = string.byte(frame, 3)
      local extlen2 = string.byte(frame, 4)
      -- bit shift by one byte extlen 
      extlen = bit.lshift(extlen, 8)
      local actualLen = extlen + extlen2
      -- print("ActualLen:", actualLen)
      m.lenExpected = actualLen
      
      data = string.sub(data, 3) -- remove first 2 bytes
      
    elseif plen == 127 then 
      error("Websocket lib does not support longer payloads yet")
      -- return
      data = string.sub(data, 5) -- remove first 4 bytes
    end
    
    -- set the buffer to the current data since it's new
    m.buffer = data
  end 
  
  -- print("Our own count of len:", string.len(data))
  
  -- calc m.lenExpected for next time back into this method
  m.lenExpected = m.lenExpected - string.len(data)
  -- print("m.lenExpected:", m.lenExpected)
  
  -- we need to see if next time into decodeFrame we are just 
  -- expecting more raw data without a frame header, i.e. happens if 
  -- the TCP packet is > 1024
  if m.lenExpected > 0 then
    -- print("Websocket expecting " .. m.lenExpected .. " more chars of data")
  else 
    -- done with data. do callback 
    m.lenExpected = 0 
  
    -- print("Payload:", data, "opcode:", opcode)
    
    if opcode == 0x9 then
      -- print("ping received")
      m.onPingRecv(m.buffer, fin, opcode)
      -- return
    elseif opcode == 0xA then 
      -- print("pong received")
      m.onPongRecv(m.buffer, fin, opcode)
      -- return
    else
      -- this is normal data, handle normally
      if m.onDataCallback ~= nil then 
        m.onDataCallback(m.buffer, fin, opcode)
      end 
    end
    -- set buffer to empty for next time into this method
    m.buffer = ""
  end

end

---
-- @name send
-- @description Send data on the websocket
-- @param data A string of data (Currently only short frames of less than 126 chars allowed. Would welcome any coders to help contribute to tweaking this library to support larger frames.)
-- @returns nil
function m.send(data)
  
  print("Websocket doing send. data:", data)
  
  -- data = data .. "\n"
  
  if m.isConnected == false then 
    print("Websocket not connected, so cannot send.")
    return
  end 
  
  local binstr, payloadLen 
  
  -- we need to create the frame headers
  if string.len(data) > 126 then 
    print("Websocket lib only supports max len 126 currently")
    return
  end
  
  -- print("Len: ", string.len(data))
  
  -- 1st byte
  -- binstr = string.char(0x1) -- opcode set to 0x1 for txt
  binstr = string.char(bit.set(0x1, 7)) -- set FIN to 1 meaning we will not multi-part this msg

  -- 2nd byte mask and payload length
  payloadLen = string.len(data) 
  payloadLen = bit.set(payloadLen, 7) -- set mask to on for 8th msb
  binstr = binstr .. string.char(payloadLen)
  
  -- 3rd, 4th, 5th, and 6th byte is masking key
  -- just use mask of 0 to cheat so no need to xor
  binstr = binstr .. string.char(0x0,0x0,0x0,0x0)
  
  -- Now add payload 
  binstr = binstr .. data

  -- print out the bytes in decimal
  -- print(string.byte(binstr, 1, string.len(binstr)))
  -- print("Len binstr:", string.len(binstr))
  -- print(binstr)

  m.sock:send(binstr)  
end 

-- Change timer to 60 seconds, or even 10 minutes if you want
-- Maximum timer duration value is 6870947 (1:54:30.947)
-- private method
function m.pingStart()
  -- check if timer running
  if m.pingTmr then 
    local running, mode = m.pingTmr:state()
    if running then 
      print("Websocket being asked to run tmr a 2nd time. huh?")
      return 
    end
  end 
  
  m.pingTmr = tmr.create()
  m.pingTmr:alarm(10000, tmr.ALARM_AUTO, m.onPingSend)
end 

-- private method
function m.pingStop()
  m.pingTmr:stop()
  m.pingTmr:unregister()
end

m.isGotPongBack = true
-- private method
function m.onPingSend()
  -- print("Websocket sending ping")
  
  -- See if we got a pong back from last time
  if m.isGotPongBack then
    -- good to go
  else
    -- we are dead
    m.isConnected = false
    print("Websocket is dead. Reconnecting...")
    m.reconnect()
    return
  end
  
  if m.isConnected == false then
    print("We are not connected, can't send ping")
    m.reconnect()
    return
  end
  
  -- set to false so we get this set back to true
  -- when pong is received back
  m.isGotPongBack = false
  
  -- send ping
  local data = "ping"
  -- 1st byte
  -- opcode set to 0x9 for ping
  local binstr = string.char(bit.set(0x9, 7)) -- set FIN to 1 meaning we will not multi-part this msg
  -- 2nd byte mask and payload length
  local payloadLen = string.len(data)
  payloadLen = bit.set(payloadLen, 7) -- set mask to on for 8th msb
  binstr = binstr .. string.char(payloadLen)
  
  -- 3rd, 4th, 5th, and 6th byte is masking key
  -- just use mask of 0 to cheat so no need to xor
  binstr = binstr .. string.char(0x0,0x0,0x0,0x0)
  -- Now add payload 
  binstr = binstr .. data
  
  -- print out the bytes in decimal
  -- print(string.byte(binstr, 1, string.len(binstr)))
  -- print("Len binstr:", string.len(binstr))
  -- print(binstr)
  
  m.sock:send(binstr)
  if m.onPingSendCallback then m.onPingSendCallback() end
end

-- private method
function m.onPingRecv()
  print("Ping received")
  -- as a client, this is unlikely, but do callback
  if m.onPingRecvCallback then m.onPingRecvCallback() end
end

-- private method
function m.onPongRecv(data, fin, opcode)
  -- print("Pong received. data:", data, "fin:", fin, "opcode:", opcode)
  m.isGotPongBack = true
  if m.onPongRecvCallback then m.onPongRecvCallback() end
end

-- private method
function m.reconnect()
  print("Reconnecting")
  if isConnected then 
    m.sock:close()
    isConnected = false
  end
  m.connect()
end  

---
-- @name disconnect
-- @description Disconnect from the websocket URL
-- @returns nil
function m.disconnect()
  if m.isConnected then 
    print("Websocket closing...")
    
    m.pingStop()
    
    -- 1st byte
    -- opcode set to 0x8 for close
    binstr = string.char(bit.set(0x8, 7)) -- set FIN to 1 meaning we will not multi-part this msg
    m.sock:send(binstr)  
    m.sock:close()
    m.isConnected = false
    print("Websocket now closed")
  else 
    print("Websocket was not connected, so not closing")
  end 
end 

---
-- @name getUrl
-- @description Get the URL for this websocket
-- @returns String of the websocket URL
function m.getUrl()
  return m.wsUrl
end 

---
-- @name isConnected
-- @description Check if the websocket is connected
-- @returns boolean True if websocket is connected. False otherwise.
function m.isConnected()
  return m.isConnected
end

return m

                                
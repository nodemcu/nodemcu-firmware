--[[ A simple ftp server

 This is my implementation of a FTP server using Github user Neronix's
 example as inspriration, but as a cleaner Lua implementation that has been
 optimised for use in LFS. The coding style adopted here is more similar to
 best practice for normal (PC) module implementations, as using LFS enables
 me to bias towards clarity of coding over brevity. It includes extra logic
 to handle some of the edge case issues more robustly. It also uses a
 standard forward reference coding pattern to allow the code to be laid out
 in main routine, subroutine order. 

 The app will only call one FTP.open() or FTP.createServer() at any time, 
 with any multiple calls requected, so FTP is a singleton static object. 
 However there is nothing to stop multiple clients connecting to the FTP 
 listener at the same time, and indeed some FTP clients do use multiple 
 connections, so this server can accept and create multiple CON objects. 
 Each CON object can also have a single DATA connection.

 Note that FTP also exposes a number of really private properties (which 
 could be stores in local / upvals) as FTP properties for debug purposes.
]]
local file,net,wifi,node,string,table,tmr,pairs,print,pcall, tostring = 
      file,net,wifi,node,string,table,tmr,pairs,print,pcall, tostring
local post = node.task.post
local FTP, cnt = {client = {}}, 0

-- Local functions

local processCommand    -- function(cxt, sock, data)
local processBareCmds   -- function(cxt, cmd)
local processSimpleCmds -- function(cxt, cmd, arg)
local processDataCmds   -- function(cxt, cmd, arg)
local dataServer        -- function(cxt, n)
local ftpDataOpen       -- function(dataSocket)

-- Note these routines all used hoisted locals such as table and debug as 
-- upvals for performance (ROTable lookup is slow on NodeMCU Lua), but
-- data upvals (e.g. FTP) are explicitly list is -- "upval:" comments.

-- Note that the space between debug and the arglist is there for a reason
-- so that a simple global edit "   debug(" -> "-- debug(" or v.v. to 
-- toggle debug compiled into the module.  

local function debug (fmt, ...) -- upval: cnt (, print, node, tmr)
  if not FTP.debug then return end
  if (...) then fmt = fmt:format(...) end
  print(node.heap(),fmt)
  cnt = cnt + 1
  if cnt % 10 then tmr.wdclr() end
end

--------------------------- Set up the FTP object ----------------------------
--       FTP has three static methods: open, createServer and close 
------------------------------------------------------------------------------

-- optional wrapper around createServer() which also starts the wifi session
function FTP.open(user, pass, ssid, pwd, dbgFlag) -- upval: FTP (, wifi, tmr, print)
  if ssid then
    wifi.setmode(wifi.STATION, false)
    wifi.sta.config { ssid = ssid, pwd  = pwd, save = false }
  end
  tmr.alarm(0, 500, tmr.ALARM_AUTO, function()
    if (wifi.sta.status() == wifi.STA_GOTIP) then
      tmr.unregister(0)
      print("Welcome to NodeMCU world", node.heap(), wifi.sta.getip())
      return FTP.createServer(user, pass, dbgFlag)
    else
      uart.write(0,".")
    end
  end)
end


function FTP.createServer(user, pass, dbgFlag)  -- upval: FTP (, debug, tostring, pcall, type, processCommand)
  FTP.user, FTP.pass, FTP.debug = user, pass, dbgFlag
  FTP.server = net.createServer(net.TCP, 180)
  _G.FTP = FTP
  debug("Server created: (userdata) %s", tostring(FTP.server))

  FTP.server:listen(21, function(sock) -- upval: FTP (, debug, pcall, type, processCommand)
      -- since a server can have multiple connections, each connection
      -- has a CNX table to store connection-wide globals.
      local client = FTP.client      
      local CNX; CNX = {
        validUser = false, 
        cmdSocket = sock,
        send      = function(rec, cb) -- upval: CNX (,debug)
         -- debug("Sending: %s", rec)
            return CNX.cmdSocket:send(rec.."\r\n", cb)
          end, --- send()
        close    = function(sock) -- upval: client, CNX (,debug, pcall, type)
         -- debug("Closing CNX.socket=%s, sock=%s", tostring(CNX.socket), tostring(sock))
            for _,s in ipairs{'cmdSocket', 'dataServer', 'dataSocket'} do
              local sck; sck,CNX[s] = CNX[s], nil
           -- debug("closing CNX.%s=%s", s, tostring(sck))
              if type(sck)=='userdata' then pcall(sck.close, sck) end
            end
            client[sock] = nil
          end -- CNX.close()
        }

      local function validateUser(sock, data) -- upval: CNX, FTP (, debug, processCommand)
        -- validate the logon and if then switch to processing commands

     -- debug("Authorising: %s", data)
        local cmd, arg = data:match('([A-Za-z]+) *([^\r\n]*)')
        local msg =  "530 Not logged in, authorization required"
        cmd = cmd:upper() 

        if   cmd == 'USER' then
          CNX.validUser = (arg == FTP.user)
          msg = CNX.validUser and  
                 "331 OK. Password required" or
                 "530 user not found"

        elseif CNX.validUser and cmd == 'PASS' then
          if arg == FTP.pass then
            CNX.cwd = '/'
            sock:on("receive", function(sock,data)
                processCommand(CNX,sock,data)
              end) -- logged on so switch to command mode
            msg = "230 Login successful. Username & password correct; proceed."
          else
            msg = "530 Try again"
          end

        elseif cmd == 'AUTH' then
          msg = "500 AUTH not understood"

        end

        return CNX.send(msg)
      end
 
    local port,ip = sock:getpeer()
 -- debug("Connection accepted: (userdata) %s client %s:%u", tostring(sock), ip, port)
    sock:on("receive",       validateUser)
    sock:on("disconnection", CNX.close)
    FTP.client[sock]=CNX

    CNX.send("220 FTP server ready");
  end) -- FTP.server:listen()
end -- FTP.createServer()


function FTP.close() -- upval: FTP (, debug, post, tostring)
  local svr = FTP.server

  local function rollupClients(client, server) -- upval: FTP (,debug, post, tostring, rollupClients)
    -- this is done recursively so that we only close one client per task
    local skt,cxt = next(client)
    if skt then
   -- debug("Client close: %s", tostring(skt))
      cxt.close(skt)
      post(function() return rollupClients(client, server) end) -- upval: rollupClients, client, server
    else
   -- debug("Server close: %s", tostring(server))
      server:close()
      server:__gc()
      FTP,_G.FTP = nil, nil -- the upval FTP can only be zeroed once FTP.client is cleared.
    end
  end
 
  if svr then rollupClients(FTP.client, svr) end
  package.loaded.ftpserver=nil
end -- FTP.close()


----------------------------- Process Command --------------------------------
-- This splits the valid commands into one of three categories: 
--   *  bare commands (which take no arg)
--   *  simple commands (which take) a single arg; and 
--   *  data commands which initiate data transfer to or from the client and 
--      hence need to use CBs.
--
-- Find strings are used do this lookup and minimise long if chains.
------------------------------------------------------------------------------
processCommand = function(cxt, sock, data) -- upvals: (, debug, processBareCmds, processSimpleCmds, processDataCmds)

  debug("Command: %s", data)
  data = data:gsub('[\r\n]+$', '') -- chomp trailing CRLF
  local cmd, arg = data:match('([a-zA-Z]+) *(.*)')
  cmd = cmd:upper()
  local _cmd_ = '_'..cmd..'_'
  
  if ('_CDUP_NOOP_PASV_PWD_QUIT_SYST_'):find(_cmd_) then
    processBareCmds(cxt, cmd) 
  elseif ('_CWD_DELE_MODE_PORT_RNFR_RNTO_SIZE_TYPE_'):find(_cmd_) then
    processSimpleCmds(cxt, cmd, arg)
  elseif ('_LIST_NLST_RETR_STOR_'):find(_cmd_) then
    processDataCmds(cxt, cmd, arg)
  else
    cxt.send("500 Unknown error")
  end
end -- processCommand(sock, data)


-------------------------- Process Bare Commands -----------------------------
processBareCmds = function(cxt, cmd) -- upval: (dataServer)

  local send = cxt.send

  if cmd == 'CDUP' then
    return send("250 OK. Current directory is "..cxt.cwd)

  elseif cmd == 'NOOP' then
    return send("200 OK")

  elseif cmd == 'PASV' then
    -- This FTP implementation ONLY supports PASV mode, and the passive port
    -- listener is opened on receipt of the PASV command.  If any data xfer
    -- commands return an error if the PASV command hasn't been received.
    -- Note the listener service is closed on receipt of the next PASV or 
    -- quit.    
    local ip, port, pphi, pplo, i1, i2, i3, i4, _
    _,ip = cxt.cmdSocket:getaddr()
    port = 2121
    pplo = port % 256
    pphi = (port-pplo)/256
    i1,i2,i3,i4 = ip:match("(%d+).(%d+).(%d+).(%d+)")
    dataServer(cxt, port) 
    return send(
       ('227 Entering Passive Mode(%d,%d,%d,%d,%d,%d)'):format(
         i1,i2,i3,i4,pphi,pplo))

  elseif cmd == 'PWD' then
    return send('257 "/" is the current directory')

  elseif cmd == 'QUIT' then
    send("221 Goodbye", function() cxt.close(cxt.cmdSocket) end)
    return 

  elseif cmd == 'SYST' then
--  return send("215 UNKNOWN")
    return send("215 UNIX Type: L8") -- must be Unix so ls is parsed correctly

  else
    error('Oops.  Missed '..cmd)    
  end
end -- processBareCmds(cmd, send) 

------------------------- Process Simple Commands ----------------------------
local from  -- needs to persist between simple commands
processSimpleCmds = function(cxt, cmd, arg)  -- upval: from (, file, tostring, dataServer, debug)

  local send = cxt.send

  if cmd == 'MODE' then
    return send(arg == "S" and "200 S OK" or
                               "504 Only S(tream) is suported")

  elseif cmd == 'PORT' then
    dataServer(cxt,nil) -- clear down any PASV setting
    return send("502 Active mode not supported. PORT not implemented")

  elseif cmd == 'TYPE' then
    if arg == "A" then
      cxt.xferType = 0
      return send("200 TYPE is now ASII")
    elseif arg == "I" then
      cxt.xferType = 1
      return send("200 TYPE is now 8-bit binary")
    else
      return send("504 Unknown TYPE")
    end 
  end

  -- The remaining commands take a filename as an arg. Strip off leading / and ./
  arg = arg:gsub('^%.?/',''):gsub('^%.?/','') 
  debug("Filename is %s",arg)

  if cmd == 'CWD' then
    if arg:match('^[%./]*$') then
      return send("250 CWD command successful")
    end
    return send("550 "..arg..": No such file or directory")

  elseif cmd == 'DELE' then
    if file.exists(arg) then
      file.remove(arg) 
      if not file.exists(arg) then return send("250 Deleted "..arg) end
    end
    return send("550 Requested action not taken")

  elseif cmd == 'RNFR' then
    from = arg
    send("350 RNFR accepted")
    return

  elseif cmd == 'RNTO' then
    local status = from and file.rename(from, arg)
 -- debug("rename('%s','%s')=%s", tostring(from), tostring(arg), tostring(status))
    from = nil
    return send(status and "250 File renamed" or
                            "550 Requested action not taken")
  elseif cmd == "SIZE" then
    local st = file.stat(arg)
    return send(st and ("213 "..st.size) or 
                       "550 Could not get file size.")

  else
    error('Oops.  Missed '..cmd)    
  end
end -- processSimpleCmds(cmd, arg, send)


-------------------------- Process Data Commands -----------------------------
processDataCmds = function(cxt, cmd, arg)  -- upval: FTP (, pairs, file, tostring, debug, post)

  local send = cxt.send

  -- The data commands are only accepted if a PORT command is in scope
  if cxt.dataServer == nil and cxt.dataSocket == nil then
    return send("502 Active mode not supported. "..cmd.." not implemented")
  end

  cxt.getData, cxt.setData = nil, nil

  arg = arg:gsub('^%.?/',''):gsub('^%.?/','') 

  if cmd == "LIST" or cmd == "NLST" then  
    -- There are 
    local fileSize, nameList, pattern = file.list(), {}, '.'
 
    arg = arg:gsub('^-[a-z]* *', '') -- ignore any Unix style command parameters
    arg = arg:gsub('^/','')  -- ignore any leading /

    if #arg > 0 and arg ~= '.' then -- replace "*" by [^/%.]* that is any string not including / or .
      pattern = arg:gsub('*','[^/%%.]*')
    end

    for k,v in pairs(fileSize) do
      if k:match(pattern) then 
        nameList[#nameList+1] = k 
      else
        fileSize[k] = nil
      end
    end
    table.sort(nameList)
 
    function cxt.getData() -- upval: cmd, fileSize, nameList (, table)
      local list, user, v = {}, FTP.user
      for i = 1,10 do
        if #nameList == 0 then break end
        local f = table.remove(nameList, 1)
        list[#list+1] = (cmd == "LIST") and    
          ("-rw-r--r-- 1 %s %s %6u Jan  1 00:00 %s\r\n"):format(user, user, fileSize[f], f) or
          (f.."\r\n")
      end
      return table.concat(list)
    end
  
  elseif cmd == "RETR" then
    local f = file.open(arg, "r")
    if f then -- define a getter to read the file
      function cxt.getData()     -- upval: f
        local buf = f:read(1024)
        if not buf then f:close(); f = nil; end
        return buf
      end -- cxt.getData()
    end

  elseif cmd == "STOR" then
    local f = file.open(arg, "w")
    if f then -- define a setter to write the file
      function cxt.setData(rec) -- upval f, arg (, debug)
     -- debug("writing %u bytes to %s", #rec, arg)
        return f:write(rec)
      end -- cxt.saveData(rec)
      function cxt.fileClose() -- upval cxt, f, arg (,debug)
     -- debug("closing %s", arg)
        f:close(); cxt.fileClose, f = nil, nil
      end -- cxt.close()
    end

  end

  send((cxt.getData or cxt.setData) and "150 Accepted data connection" or
                                        "451 Can't open/create "..arg)
  if cxt.getData and cxt.dataSocket then 
    debug ("poking sender to initiate first xfer")
    post(function() cxt.sender(cxt.dataSocket) end)
  end

end -- processDataCmds(cmd, arg, send)


----------------------------- Data Port Routines -----------------------------
-- These are used to manage the data transfer over the data port.  This is
-- set up lazily either by a PASV or by the first LIST NLST RETR or STOR 
-- command that uses it.  These also provide a sendData / receiveData cb to
-- handle the actual xfer. Also note that the sending process can be primed in
--
----------------   Open a new data server and port ---------------------------
dataServer = function(cxt, n) -- upval: (pcall, net, ftpDataOpen, debug, tostring)
  local dataServer = cxt.dataServer
  if dataServer then -- close any existing listener 
    pcall(dataServer.close, dataServer) 
  end
  if n then 
    -- Open a new listener if needed. Note that this is only used to establish
    -- a single connection, so ftpDataOpen closes the server socket
    cxt.dataServer = net.createServer(net.TCP, 300)
    cxt.dataServer:listen(n, function(sock) -- upval: cxt, (ftpDataOpen)
      ftpDataOpen(cxt,sock)
      end)
 -- debug("Listening on Data port %u, server %s",n, tostring(cxt.dataServer))
  else
    cxt.dataServer = nil
 -- debug("Stopped listening on Data port",n)
  end
end -- dataServer(n)

----------------------- Connection on FTP data port ------------------------
ftpDataOpen = function(cxt, dataSocket) -- upval: (debug, tostring, post, pcall)

  local sport,sip = dataSocket:getaddr()
  local cport,cip = dataSocket:getpeer()
  debug("Opened data socket %s from %s:%u to %s:%u", tostring(dataSocket),sip,sport,cip,cport )
  cxt.dataSocket = dataSocket

  cxt.dataServer:close()
  cxt.dataServer = nil
    
  local function cleardown(skt,type) -- upval: cxt (, debug, tostring, post, pcall)
    type = type==1 and "disconnection" or "reconnection"
    local which = cxt.setData and "setData" or (cxt.getData and cxt.getData or "neither")
 -- debug("Cleardown entered from %s with %s", type, which)

    if cxt.setData then
      cxt.fileClose()
      cxt.setData = nil    
      cxt.send("226 Transfer complete.")
    else
      cxt.getData, cxt.sender = nil, nil
    end
 -- debug("Clearing down data socket %s", tostring(skt))
    post(function() -- upval: skt, cxt, (, pcall)
            pcall(skt.close, skt); skt=nil
            cxt.dataSocket = nil
         end)
  end

  local on_hold = false

  dataSocket:on("receive", function(skt, rec) --upval: cxt, on_hold (, debug, tstring, post, node, pcall)
    local which = cxt.setData and "setData" or (cxt.getData and cxt.getData or "neither")
 -- debug("Received %u data bytes with %s", #rec, which)

    if not cxt.setData then return end

    if not on_hold then
      -- Cludge to stop the client flooding the ESP SPIFFS on an upload of a
      -- large file. As soon as a record arrives assert a flow control hold.  
      -- This can take up to 5 packets to come into effect at which point the
      -- low priority unhold task is executed releasing the flow again.
   -- debug("Issuing hold on data socket %s", tostring(skt))
      skt:hold(); on_hold = true
      post(node.task.LOW_PRIORITY, 
           function() -- upval: skt, on_hold (, debug, tostring))
          -- debug("Issuing unhold on data socket %s", tostring(skt))
             pcall(skt.unhold, skt); on_hold = false
           end)
    end

    if not cxt.setData(rec) then
   -- debug("Error writing to SPIFFS")
      cxt.fileClose()
      cxt.setData = nil
      cxt.send("552 Upload aborted. Exceeded storage allocation")
    end
  end)

  function cxt.sender(skt) -- upval: cxt (, debug)
    debug ("entering sender")
    if not cxt.getData then return end
    local rec, skt = cxt.getData(), cxt.dataSocket
    if rec and #rec > 0 then
   -- debug("Sending %u data bytes", #rec)
      skt:send(rec)
    else
   -- debug("Send of data completed")
      skt:close()
      cxt.send("226 Transfer complete.")
      cxt.getData, cxt.dataSocket = nil, nil  
    end
  end

  dataSocket:on("sent", cxt.sender)
  dataSocket:on("disconnection", function(skt) return cleardown(skt,1) end)
  dataSocket:on("reconnection",  function(skt) return cleardown(skt,2) end)

  -- if we are sending to client then kick off the first send
  if cxt.getData then cxt.sender(cxt.dataSocket) end

end -- ftpDataOpen(socket)
  
------------------------------------------------ -----------------------------

return FTP

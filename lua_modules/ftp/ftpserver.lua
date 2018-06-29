-- a simple ftp server
local file,net,pairs,print,string,table = file,net,pairs,print,string,table
local ftp, ftp_data, ftp_srv
do
local createServer = function (user, pass)
local data_fnc, data_sock = nil, nil
ftp_data = net.createServer(net.TCP, 180)
ftp_data:listen(1024, function (s) if data_fnc then data_fnc(s) else data_sock = s end end)
ftp_srv = net.createServer(net.TCP, 180)
ftp_srv:listen(21, function(socket)
  local s = 0
  local cwd = "/"
  local buf = ""
  local t = 0
  socket:on("receive", function(c, d)
    local a = {}
    for i in string.gmatch(d, "([^ \r\n]+)") do
      table.insert(a,i)
    end
    local a1,a2 = unpack(a)
    if a1 == nil or a1 == "" then return end
    if s == 0 and a1 == "USER" then
      if a2 ~= user then
        return c:send("530 user not found\r\n")
      end
      s = 1
      return c:send("331 OK. Password required\r\n")
    end
    if s == 1 and a1 == "PASS" then
      if a2 ~= pass then
        return c:send("530 Try again\r\n")
      end
      s = 2
      return c:send("230 OK.\r\n")
    end
    if s ~= 2 then
      return c:send("530 Not logged in, authorization required\r\n")
    end
    if a1 == "SYST" then
      return c:send("250 UNIX Type: L8\r\n")
    end
    if a1 == "CDUP" then
      return c:send("250 OK. Current directory is "..cwd.."\r\n")
    end
    if a1 == "CWD" then
      if a2 == "." then
        return c:send("257 \""..cwd.."\" is your current directory\r\n")
      end
      if a2 == "/" then
        cwd = a2
        return c:send("250 OK. Current directory is "..cwd.."\r\n")
      end
      return c:send("550 Failed to change directory.\r\n")
    end
    if a1 == "PWD" then
      return c:send("257 \""..cwd.."\" is your current directory\r\n")
    end
    if a1 == "TYPE" then
      if a2 == "A" then
        t = 0
        return c:send("200 TYPE is now ASII\r\n")
      end
      if a2 == "I" then
        t = 1
        return c:send("200 TYPE is now 8-bit binary\r\n")
      end
      return c:send("504 Unknown TYPE")
    end
    if a1 == "MODE" then
      if a2 ~= "S" then
        return c:send("504 Only S(tream) is suported\r\n")
      end
      return c:send("200 S OK\r\n")
    end
    if a1 == "PASV" then
      local _,ip = c:getaddr()
      local _,_,i1,i2,i3,i4 = string.find(ip,"(%d+).(%d+).(%d+).(%d+)")
      return c:send("227 Entering Passive Mode ("..i1..","..i2..","..i3..","..i4..",4,0).\r\n")
    end
    if a1 == "LIST" or a1 == "NLST" then
      c:send("150 Accepted data connection\r\n")
      data_fnc = function(sd)
        local l = file.list();
        for k,v in pairs(l) do
          sd:send( a1 == "NLST" and k.."\r\n" or "-rwxrwxrwx 1 esp esp "..v.." Jan  1  2018 "..k.."\r\n")
        end
        sd:close()
        data_fnc = nil
        c:send("226 Transfer complete.\r\n")
      end
      if data_sock then
        node.task.post(function() data_fnc(data_sock);data_sock=nil end)
      end
      return
    end
    if a1 == "RETR" then
      local f = file.open(a2:gsub("%/",""),"r")
      if f == nil then
        return c:send("550 File "..a2.." not found\r\n")
      end
      c:send("150 Accepted data connection\r\n")
      data_fnc = function(sd)
        sd:on("sent", function(cd)
          b=f:read(1024)
          if b then
            cd:send(b)
            b=nil
          else
            cd:close()
            f:close()
            data_fnc = nil
            c:send("226 Transfer complete.\r\n")
          end
        end)
        local b=f:read(1024)
        sd:send(b)
        b=nil
      end
      if data_sock then
        node.task.post(function() data_fnc(data_sock);data_sock=nil end)
      end
      return
    end
    if a1 == "STOR" then
      local f = file.open(a2:gsub("%/",""),"w")
      if f == nil then
        return c:send("451 Can't open/create "..a2.."\r\n")
      end
      c:send("150 Accepted data connection\r\n")
      data_fnc = function(sd)
        sd:on("receive", function(cd, dd)
          f:write(dd)
        end)
        sd:on("disconnection", function(c)
          f:close()
          data_fnc = nil
        end)
        c:send("226 Transfer complete.\r\n")
      end
      if data_sock then
        node.task.post(function() data_fnc(data_sock);data_sock=nil end)
      end
      return
    end
    if a1 == "RNFR" then
      buf = a2
      return c:send("350 RNFR accepted\r\n")
    end
    if a1 == "RNTO" and buf ~= "" then
      file.rename(buf, a2)
      buf = ""
      return c:send("250 File renamed\r\n")
    end
    if a1 == "DELE" then
      if a2 == nil or a2 == "" then
        return c:send("501 No file name\r\n")
      end
      file.remove(a2:gsub("%/",""))
      return c:send("250 Deleted "..a2.."\r\n")
    end
    if a1 == "SIZE" then
      local f = file.stat(a2)
      return c:send("213 "..(f and f.size or 1).."\r\n")
    end
    if a1 == "NOOP" then
      return c:send("200 OK\r\n")
    end
    if a1 == "QUIT" then
      return c:send("221 Goodbye\r\n", function (s) s:close() end)
    end
    c:send("500 Unknown error\r\n")
  end)
  socket:send("220--- Welcome to FTP for ESP8266/ESP32 ---\r\n220---   By NeiroN   ---\r\n220 --   Version 1.8   --\r\n");
end)
end
local closeServer = function()
  ftp_data:close()
  ftp_srv:close()
end
ftp = { createServer = createServer, closeServer = closeServer }
end
return ftp

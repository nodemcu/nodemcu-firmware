-- a simple ftp server
USER = "test"
PASS = "12345"
local file,net,pairs,print,string,table = file,net,pairs,print,string,table
data_fnc = nil
ftp_data = net.createServer(net.TCP, 180)
ftp_data:listen(20, function (s)
  if data_fnc then data_fnc(s) end
end)
ftp_srv = net.createServer(net.TCP, 180)
ftp_srv:listen(21, function(socket)
  local s = 0
  local cwd = "/"
  local buf = ""
  local t = 0
  socket:on("receive", function(c, d)
    a = {}
    for i in string.gmatch(d, "([^ \r\n]+)") do
      table.insert(a,i)
    end
    if(a[1] == nil or a[1] == "")then return end
    if(s == 0 and a[1] == "USER")then
      if(a[2] ~= USER)then
        return c:send("530 user not found\r\n")
      end
      s = 1
      return c:send("331 OK. Password required\r\n")
    end
    if(s == 1 and a[1] == "PASS")then
      if(a[2] ~= PASS)then
        return c:send("530 \r\n")
      end
      s = 2
      return c:send("230 OK.\r\n")
    end
    if(a[1] == "CDUP")then
      return c:send("250 OK. Current directory is "..cwd.."\r\n")
    end
    if(a[1] == "CWD")then
      if(a[2] == ".")then
        return c:send("257 \""..cwd.."\" is your current directory\r\n")
      end
      cwd = a[2]
      return c:send("250 OK. Current directory is "..cwd.."\r\n")
    end
    if(a[1] == "PWD")then
      return c:send("257 \""..cwd.."\" is your current directory\r\n")
    end
    if(a[1] == "TYPE")then
      if(a[2] == "A")then
        t = 0
        return c:send("200 TYPE is now ASII\r\n")
      end
      if(a[2] == "I")then
        t = 1
        return c:send("200 TYPE is now 8-bit binary\r\n")
      end
      return c:send("504 Unknown TYPE")
    end
    if(a[1] == "MODE")then
      if(a[2] ~= "S")then
        return c:send("504 Only S(tream) is suported\r\n")
      end
      return c:send("200 S OK\r\n")
    end
    if(a[1] == "PASV")then
      local _,ip = socket:getaddr()
      local _,_,i1,i2,i3,i4 = string.find(ip,"(%d+).(%d+).(%d+).(%d+)")
      return c:send("227 Entering Passive Mode ("..i1..","..i2..","..i3..","..i4..",0,20).\r\n")
    end
    if(a[1] == "LIST" or a[1] == "NLST")then
      c:send("150 Accepted data connection\r\n")
      data_fnc = function(sd)
        local l = file.list();
        for k,v in pairs(l) do
          if(a[1] == "NLST")then
            sd:send(k.."\r\n")
          else
            sd:send("+r,s"..v..",\t"..k.."\r\n")
          end
        end
        sd:close()
        data_fnc = nil
        c:send("226 Transfer complete.\r\n")
      end
      return
    end
    if(a[1] == "RETR")then
      f = file.open(a[2]:gsub("%/",""),"r")
      if(f == nil)then
        return c:send("550 File "..a[2].." not found\r\n")
      end
      c:send("150 Accepted data connection\r\n")
      data_fnc = function(sd)
        local b=f:read(1024)
        sd:on("sent", function(cd)
          if b then
            sd:send(b)
            b=f:read(1024)
          else
            sd:close()
            f:close()
            data_fnc = nil
            c:send("226 Transfer complete.\r\n")
          end
        end)
        if b then
          sd:send(b)
          b=f:read(1024)
        end
      end
      return
    end
    if(a[1] == "STOR")then
      f = file.open(a[2]:gsub("%/",""),"w")
      if(f == nil)then
        return c:send("451 Can't open/create "..a[2].."\r\n")
      end
      c:send("150 Accepted data connection\r\n")
      data_fnc = function(sd)
        sd:on("receive", function(cd, dd)
          f:write(dd)
        end)
        socket:on("disconnection", function(c)
          f:close()
          data_fnc = nil
        end)
        c:send("226 Transfer complete.\r\n")
      end
      return
    end
    if(a[1] == "RNFR")then
      buf = a[2]
      return c:send("350 RNFR accepted\r\n")
    end
    if(a[1] == "RNTO" and buf ~= "")then
      file.rename(buf, a[2])
      buf = ""
      return c:send("250 File renamed\r\n")
    end
    if(a[1] == "DELE")then
      if(a[2] == nil or a[2] == "")then
        return c:send("501 No file name\r\n")
      end
      file.remove(a[2]:gsub("%/",""))
      return c:send("250 Deleted "..a[2].."\r\n")
    end
    if(a[1] == "NOOP")then
      return c:send("200 OK\r\n")
    end
    if(a[1] == "QUIT")then
      return c:send("221 Goodbye\r\n", function (s) s:close() end)
    end
    c:send("500 Unknown error\r\n")
  end)
  socket:send("220--- Welcome to FTP for ESP8266/ESP32 ---\r\n220---   By NeiroN   ---\r\n220 --   Version 1.5   --\r\n");
end)

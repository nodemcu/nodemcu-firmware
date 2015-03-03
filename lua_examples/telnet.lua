print("====Wicon, a LUA console over wifi.==========")
print("Author: openthings@163.com. copyright&GPL V2.")
print("Last modified 2014-11-19. V0.2")
print("Wicon Server starting ......")

function connected(conn)
   print("Wifi console connected.")
   function s_output(str)
      if (conn~=nil)    then
         conn:send(str)
      end
   end
   node.output(s_output,0)
   conn:on("receive", function(conn, pl) 
      node.input(pl) 
   end)
   conn:on("disconnection",function(conn) 
      node.output(nil) 
   end)
   print("Welcome to NodeMcu world.")
end

function startServer()
   print("Wifi AP connected. Wicon IP:")
   print(wifi.sta.getip())
   sv=net.createServer(net.TCP, 180)
   sv:listen(2323, connected)
   print("Telnet Server running at :2323")
   print("===Now, logon and input LUA.====")
end

tmr.alarm(1, 1000, 1, function() 
   if wifi.sta.getip()=="0.0.0.0" then
      print("Connect AP, Waiting...") 
   else
      startServer()
      tmr.stop(1)
   end
end)

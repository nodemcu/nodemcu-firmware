print("====Wicon, a LUA console over wifi.==========")
print("Author: openthings@163.com. copyright&GPL V2.")
print("Last modified 2014-11-19. V0.2")
print("Wicon Server starting ......")

function startServer()
   print("Wifi AP connected. Wicon IP:")
   print(wifi.sta.getip())
   sv=net.createServer(net.TCP, 180)
   sv:listen(8080,   function(conn)
      print("Wifi console connected.")
   
      function s_output(str)
         if (conn~=nil)    then
            conn:send(str)
         end
      end
      node.output(s_output,0)

      conn:on("receive", function(conn, pl) 
         node.input(pl) 
         if (conn==nil)    then 
            print("conn is nil.") 
         end
      end)
      conn:on("disconnection",function(conn) 
         node.output(nil) 
      end)
   end)   
   print("Wicon Server running at :8080")
   print("===Now,Using xcon_tcp logon and input LUA.====")
end

tmr.alarm(0, 1000, 1, function() 
   if wifi.sta.getip()=="0.0.0.0" then
      print("Connect AP, Waiting...") 
   else
      startServer()
      tmr.stop()
   end
end)

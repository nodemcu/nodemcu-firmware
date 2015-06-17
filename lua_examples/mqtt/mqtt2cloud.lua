-- test with cloudmqtt.com
m_dis={}
function dispatch(m,t,pl)
	if pl~=nil and m_dis[t] then
		m_dis[t](m,pl)
	end
end
function topic1func(m,pl)
	print("get1: "..pl)
end
function topic2func(m,pl)
	print("get2: "..pl)
end
m_dis["/topic1"]=topic1func
m_dis["/topic2"]=topic2func
-- Lua: mqtt.Client(clientid, keepalive, user, pass)
m=mqtt.Client("nodemcu1",60,"test","test123")
m:on("connect",function(m) 
	print("connection "..node.heap()) 
	m:subscribe("/topic1",0,function(m) print("sub done") end)
	m:subscribe("/topic2",0,function(m) print("sub done") end)
	m:publish("/topic1","hello",0,0) m:publish("/topic2","world",0,0)
	end )
m:on("offline", function(conn)
    print("disconnect to broker...")
    print(node.heap())
end)
m:on("message",dispatch )
-- Lua: mqtt:connect( host, port, secure, auto_reconnect, function(client) )
m:connect("m11.cloudmqtt.com",11214,0,1)
tmr.alarm(0,10000,1,function() local pl = "time: "..tmr.time() 
	m:publish("/topic1",pl,0,0)
	end)

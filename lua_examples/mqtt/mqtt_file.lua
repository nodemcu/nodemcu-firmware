-- test transfer files over mqtt.
local m_dis = {}

local function dispatch(m, t, pl)
	if pl ~= nil and m_dis[t] then
		m_dis[t](m,pl)
	end
end

local function pubfile(m,filename)
	file.close()
	file.open(filename)
	repeat
    local pl = file.read(1024)
    if pl then m:publish("/topic2", pl, 0, 0) end
  until not pl
    file.close()
end
-- payload(json): {"cmd":xxx,"content":xxx}
local function topic1func(m,pl)
	print("get1: "..pl)
	local pack = sjson.decode(pl)
	if pack.content then
		if pack.cmd == "open" then file.open(pack.content,"w+")
		elseif pack.cmd == "write" then file.write(pack.content)
		elseif pack.cmd == "close" then file.close()
		elseif pack.cmd == "remove" then file.remove(pack.content)
		elseif pack.cmd == "run" then dofile(pack.content)
		elseif pack.cmd == "read" then pubfile(m, pack.content)
		end
	end
end

do
  m_dis["/topic1"]=topic1func
  -- Lua: mqtt.Client(clientid, keepalive, user, pass)
  local m = mqtt.Client()
  m:on("connect",function(client)
    print("connection "..node.heap())
    client:subscribe("/topic1", 0, function() print("sub done") end)
    end)
  m:on("offline", function()
      print("disconnect to broker...")
      print(node.heap())
  end)
  m:on("message",dispatch )
  -- Lua: mqtt:connect( host, port, secure, function(client) )
  m:connect("192.168.18.88",1883,0)
end
-- usage:
-- another client(pc) subscribe to /topic2, will receive the test.lua content.
-- and publish below message to /topic1
-- {"cmd":"open","content":"test.lua"}
-- {"cmd":"write","content":"print([[hello world]])\n"}
-- {"cmd":"write","content":"print(\"hello2 world2\")\n"}
-- {"cmd":"write","content":"test.lua"}
-- {"cmd":"run","content":"test.lua"}
-- {"cmd":"read","content":"test.lua"}

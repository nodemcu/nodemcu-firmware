-- test with cloudmqtt.com
local m_dis = {}

local function dispatch(m,t,pl)
  if pl~=nil and m_dis[t] then
    m_dis[t](m,pl)
  end
end

local function topic1func(_,pl)
  print("get1: "..pl)
end

local function topic2func(_,pl)
  print("get2: "..pl)
end

do
  m_dis["/topic1"] = topic1func
  m_dis["/topic2"] = topic2func
  -- Lua: mqtt.Client(clientid, keepalive, user, pass)
  local m = mqtt.Client("nodemcu1", 60, "test", "test123")
  m:on("connect",function(client)
    print("connection "..node.heap())
    client:subscribe("/topic1",0,function() print("sub done") end)
    client:subscribe("/topic2",0,function() print("sub done") end)
    client:publish("/topic1","hello",0,0)
    client:publish("/topic2","world",0,0)
    end)
  m:on("offline", function()
    print("disconnect to broker...")
    print(node.heap())
  end)
  m:on("message",dispatch )
  -- Lua: mqtt:connect( host, port, secure, function(client) )
  m:connect("m11.cloudmqtt.com",11214,0)
  tmr.create():alarm(10000, tmr.ALARM_AUTO, function()
    local pl = "time: "..tmr.time()
    m:publish("/topic1",pl,0,0)
  end)
end
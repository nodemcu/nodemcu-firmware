-- need a wifi connection
-- enter your wifi credentials
local credentials = {SSID = "SSID", PASS = "PASS"};

-- push a message onto the network
-- this can also be done by changing gossip.networkState[gossip.ip].data = {temperature = 78};
local function sendAlarmingData()
  Gossip.pushGossip({temperature = 78});
  print('Pushed alarming data');
end

local function removeAlarmingData()
  Gossip.pushGossip(nil);
  print('Removed alarming data from the network.');
end

-- callback function for when gossip receives an update
local function treatAlarmingData(updateData)
  for k in pairs(updateData) do
    if updateData[k].data then
      if updateData[k].data.temperature and updateData[k].data.temperature > 30 then
        print('Warning, the temp is above 30 degrees at ' .. k);
      end
    end
  end
end

local function Startup()
  -- initialize all nodes with the seed except for the seed itself
  -- eventually they will all know about each other

  -- enter at least one ip that will be a start seed
  local startingSeed = '192.168.0.73';

  -- luacheck: push allow defined
  Gossip = require('gossip');
  -- luacheck: pop
  local config = {debug = true, seedList = {}};

  if wifi.sta.getip() ~= startingSeed then
    table.insert(config.seedList, startingSeed);
  end

  Gossip.setConfig(config);

  -- add the update callback
  Gossip.updateCallback = treatAlarmingData;

  -- start gossiping
  Gossip.start();

  -- send some alarming data timer
  if wifi.sta.getip() == startingSeed then
    tmr.create():alarm(50000, tmr.ALARM_SINGLE, sendAlarmingData);
    tmr.create():alarm(50000*3, tmr.ALARM_SINGLE, removeAlarmingData);
  end
end

local function startExample()
  wifi.eventmon.register(wifi.eventmon.STA_DISCONNECTED,
                         function() print('Diconnected') end);
  print("Connecting to WiFi access point...");

  if wifi.sta.getip() == nil then
    wifi.setmode(wifi.STATION);
    wifi.sta.config({ssid = credentials.SSID, pwd = credentials.PASS});
  end
  print('Ip: ' .. wifi.sta.getip() .. '. Starting in 5s ..');
  tmr.create():alarm(5000, tmr.ALARM_SINGLE, Startup);
end

startExample();


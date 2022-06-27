-- Gossip protocol implementation
-- https://github.com/alexandruantochi/
local gossip = {};
local constants = {};
local utils = {};
local network = {};
local state = {};

-- Utils

utils.contains = function(list, element)
  for k in pairs(list) do if list[k] == element then return true; end end
  return false;
end

utils.debug = function(message)
  if gossip.config.debug then
    gossip.config.debugOutput(message);
  end
end

utils.getNetworkState = function() return sjson.encode(gossip.networkState); end

utils.isNodeDataValid = function(nodeData)
  return (nodeData and nodeData.revision and nodeData.heartbeat and
             nodeData.state) ~= nil;
end

utils.compare = function(first, second)
  if first > second then return -1; end
  if first < second then return 1; end
  return 0;
end

utils.compareNodeData = function(first, second)
  local firstDataValid = utils.isNodeDataValid(first);
  local secondDataValid = utils.isNodeDataValid(second);
  if firstDataValid and secondDataValid then
    for index in ipairs(constants.comparisonFields) do
      local comparisonField = constants.comparisonFields[index];
      local comparisonResult = utils.compare(first[comparisonField],
                                             second[comparisonField]);
      if comparisonResult ~= 0 then return comparisonResult; end
    end
  elseif firstDataValid then
    return -1;
  elseif secondDataValid then
    return 1;
  end
  return 0;
end

-- computes data1 - data2 based on node compare function
utils.getMinus = function(data1, data2)
  local diff = {};
  for ip, nodeData1 in pairs(data1) do
    if utils.compareNodeData(nodeData1, data2[ip]) == -1 then
      diff[ip] = nodeData1;
    end
  end
  return diff;
end

utils.setConfig = function(userConfig)
  for k, v in pairs(userConfig) do
    if gossip.config[k] ~= nil and type(gossip.config[k]) == type(v) then
      gossip.config[k] = v;
    end
  end
end

-- State

state.setRev = function()
  local revision = 0;
  if file.exists(constants.revFileName) then
    revision = file.getcontents(constants.revFileName) + 1;
  end
  file.putcontents(constants.revFileName, revision);
  utils.debug('Revision set to ' .. revision);
  return revision;
end

state.setRevFileValue = function(revNumber)
  if revNumber then
    file.putcontents(constants.revFileName, revNumber);
    utils.debug('Revision overriden to ' .. revNumber);
  else
    utils.debug('Please provide a revision number.');
  end
end

state.start = function()
  if gossip.started then
    utils.debug('Gossip already started.');
    return;
  end
  gossip.ip = wifi.sta.getip();
  if not gossip.ip then
    utils.debug('Node not connected to network. Gossip will not start.');
    return;
  end

  -- sending to own IP makes no sense
  for index, value in ipairs(gossip.config.seedList) do
      if value == gossip.ip then
          table.remove(gossip.config.seedList, index)
          utils.debug('removing own ip from seed list')
      end
  end

  gossip.networkState[gossip.ip] = {};
  local localState = gossip.networkState[gossip.ip];
  localState.revision = state.setRev();
  localState.heartbeat = tmr.time();
  localState.state = constants.nodeState.UP;

  gossip.socket = net.createUDPSocket();
  gossip.socket:listen(gossip.config.comPort);
  gossip.socket:on('receive', network.receiveData);

  gossip.started = true;

  gossip.timer = tmr.create();
  gossip.timer:register(gossip.config.roundInterval, tmr.ALARM_AUTO,
                        network.sendSyn);
  gossip.timer:start();

  utils.debug('Gossip started.');
end

state.tickNodeState = function(ip)
  if gossip.networkState[ip] then
    local nodeState = gossip.networkState[ip].state;
    if nodeState < constants.nodeState.REMOVE then
      nodeState = nodeState + constants.nodeState.TICK;
      gossip.networkState[ip].state = nodeState;
    end
  end
end

-- Network

network.pushGossip = function(data, ip)
  if not gossip.started then
    utils.debug('Gossip not started.');
    return;
  end
  gossip.networkState[gossip.ip].data = data;
  network.sendSyn(nil, ip);
end

network.updateNetworkState = function(updateData)
  if gossip.updateCallback then gossip.updateCallback(updateData); end
  for ip, data in pairs(updateData) do
    if not utils.contains(gossip.config.seedList, ip) then
      table.insert(gossip.config.seedList, ip);
    end
    if ip ~= gossip.ip then
      gossip.networkState[ip] = data;
    end
  end
end

-- luacheck: push no unused
network.sendSyn = function(t, ip)
  local destination = ip or network.pickRandomNode();
  gossip.networkState[gossip.ip].heartbeat = tmr.time();
  if destination then
    network.sendData(destination, gossip.networkState, constants.updateType.SYN);
    state.tickNodeState(destination);
  end
end
-- luacheck: pop

network.pickRandomNode = function()
  if #gossip.config.seedList > 0 then
    local randomListPick = node.random(1, #gossip.config.seedList);
    utils.debug('Randomly picked: ' .. gossip.config.seedList[randomListPick]);
    return gossip.config.seedList[randomListPick];
  end
  utils.debug(
      'Seedlist is empty. Please provide one or wait for node to be contacted.');
  return nil;
end

network.sendData = function(ip, data, sendType)
  data.type = sendType
  local dataToSend = sjson.encode(data)
  data.type = nil
  gossip.socket:send(gossip.config.comPort, ip, dataToSend)
  utils.debug("Sent "..#dataToSend.." bytes")
end

network.receiveSyn = function(ip, synData)
  utils.debug('Received SYN from ' .. ip);
  local update = utils.getMinus(synData, gossip.networkState);
  local diff = utils.getMinus(gossip.networkState, synData);
  network.updateNetworkState(update);
  network.sendAck(ip, diff);
end

network.receiveAck = function(ip, ackData)
  utils.debug('Received ACK from ' .. ip);
  local update = utils.getMinus(ackData, gossip.networkState);
  network.updateNetworkState(update);
end

network.sendAck = function(ip, diff)
  local diffIps = '';
  for k in pairs(diff) do diffIps = diffIps .. ' ' .. k; end
  utils.debug('Sending ACK to ' .. ip .. ' with ' .. diffIps .. ' updates.');
  network.sendData(ip, diff, constants.updateType.ACK);
end

-- luacheck: push no unused
network.receiveData = function(socket, data, port, ip)
  if gossip.networkState[ip] then
    gossip.networkState[ip].state = constants.nodeState.UP;
  end
  local messageDecoded, updateData = pcall(sjson.decode, data);
  if not messageDecoded then
    utils.debug('Invalid JSON received from ' .. ip);
    return;
  end
  local updateType = updateData.type;
  updateData.type = nil;
  if updateType == constants.updateType.SYN then
    network.receiveSyn(ip, updateData);
  elseif updateType == constants.updateType.ACK then
    network.receiveAck(ip, updateData);
  else
    utils.debug('Invalid data comming from ip ' .. ip ..
                    '. No valid type specified.');
  end
end
-- luacheck: pop

-- Constants

constants.nodeState = {TICK = 1, UP = 0, SUSPECT = 2, DOWN = 3, REMOVE = 4};

constants.defaultConfig = {
  seedList = {},
  roundInterval = 15000,
  comPort = 5000,
  debug = false,
  debugOutput = print
};

constants.comparisonFields = {'revision', 'heartbeat', 'state'};

constants.updateType = {ACK = 'ACK', SYN = 'SYN'}

constants.revFileName = 'gossip/rev.dat';

-- Return

gossip = {
  started = false,
  config = constants.defaultConfig,
  setConfig = utils.setConfig,
  start = state.start,
  setRevFileValue = state.setRevFileValue,
  networkState = {},
  getNetworkState = utils.getNetworkState,
  pushGossip = network.pushGossip
};

-- return

if (... == 'test') then
  return {
    _gossip = gossip,
    _constants = constants,
    _utils = utils,
    _network = network,
    _state = state
  };
elseif net and file and tmr and wifi then
  return gossip;
else
  error('Gossip requires these modules to work: net, file, tmr, wifi');
end

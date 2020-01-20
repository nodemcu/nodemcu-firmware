-- Gossip protocol implementation
-- https://github.com/alexandruantochi/

local gossip = {};
local constants = {};
local utils = {};
local network = {};
local state = {};

-- Utils

utils.debug = function(message)
    if gossip.config.debug then
        if gossip.config.debugOutput ~= nil then
            gossip.config.debugOutput(message);
        else
            print(message);
        end
    end
end

utils.getNetworkState = function() return sjson.encode(gossip.networkState); end
utils.getSeedList = function() return sjson.encode(gossip.config.seedList); end

utils.isNodeDataValid = function(nodeData)
    return  nodeData.revision ~= nil
        and nodeData.heartbeat ~= nil
        and nodeData.state ~= nil;
end

utils.compareNodeData = function(data0, data1)
    if not utils.isNodeDataValid(data1) then return 0; end
    if data0.revision == data1.revision and data0.heartbeat == data1.heartbeat and
        data0.state == data1.state then return -1; end
    if data0.revision > data1.revision then return 0; end
    if data1.revision > data0.revision then return 1; end
    if data0.heartbeat > data1.heartbeat then return 0; end
    if data1.heartbeat > data0.heartbeat then return 1; end
    if data0.state > data1.state then return 0; end
    if data1.state > data0.state then return 1; end
    return 0;
end

utils.getNetworkStateDiff = function(synData)
    local diff = {};
    local diffUpdateList = '';
    for ip, nodeData in pairs(gossip.networkState) do
        if synData[ip] == nil or utils.compareNodeData(nodeData, synData[ip]) == 0 then
            diffUpdateList = diffUpdateList .. ip .. ' ';
            diff[ip] = nodeData;
        end
    end
    utils.debug('Computed diff: ' .. diffUpdateList);
    return diff;
end

utils.setConfig = function(userConfig)
    for k, v in pairs(userConfig) do
        if (gossip.config[k] ~= nil and type(gossip.config[k]) == type(v)) then
            gossip.config[k] = v;
            utils.debug('Set value for ' .. k);
        end
    end
end

-- State

state.setRev = function(revNumber)
    local revision = 0;
    local revFile = constants.revFileName;

    if revNumber ~= nil then
        revision = revNumber;
    elseif file.exists(revFile) then
        revision = file.getcontents(revFile) + 1;
    end
    file.putcontents(revFile, revision);
    gossip.currentState.revision = revision;
    utils.debug('Revision set to ' .. gossip.currentState.revision);
end

state.setRevManually = function(revNumber)
    state.setRev(revNumber);
    utils.debug('Revision overriden to ' .. revNumber);
end

state.start = function()
    if gossip.started then
        utils.debug('Gossip already started.');
        return;
    end
    gossip.ip = wifi.sta.getip();
    if gossip.ip == nil then
        utils.debug('Node not connected to network. Gossip will not start.');
        return;
    end
    state.setRev();
    gossip.networkState[gossip.ip] = gossip.currentState;

    gossip.inboundSocket = net.createUDPSocket();
    gossip.inboundSocket:listen(gossip.config.comPort);
    gossip.inboundSocket:on('receive', network.stateUpdate());

    gossip.started = true;
    gossip.timer = tmr.create();
    gossip.timer:register(gossip.config.roundInterval, tmr.ALARM_AUTO, network.sendSyn);
    gossip.timer:start();
end

-- Network

network.addNewNode = function(ip, nodeData)
    table.insert(gossip.config.seedList, ip);
    utils.debug('Inserted ' .. ip .. ' into seed list.');
    gossip.networkState[ip] = nodeData;
end

network.updateNetworkState = function(synData)
    local updatedNodes = '';
    for ip, synNodeData in pairs(synData) do
        if gossip.networkState[ip] ~= nil then
            if utils.compareNodeData(gossip.networkState[ip], synNodeData) == 1 then
                gossip.networkState[ip] = synNodeData;
                updatedNodes = updatedNodes .. ip .. ' ';
            end
        else
            network.addNewNode(ip, synNodeData);
            updatedNodes = updatedNodes .. ip .. ' ';
        end
    end
    utils.debug('Updated networkState with nodes: ' .. updatedNodes);
end

network.sendSyn = function()
    gossip.networkState[gossip.ip].heartbeat = tmr.time();
    local randomNode = network.pickRandomNode();
    if randomNode ~= nil then
        network.sendData(randomNode, gossip.networkState,
                         constants.updateType.SYN);
        if gossip.networkState[randomNode] ~= nil then
            local nodeState = gossip.networkState[randomNode].state;
            if nodeState < constants.nodeState.REMOVE then
                nodeState = nodeState + constants.nodeState.TICK;
                gossip.networkState[randomNode].state = nodeState;
            end
        end
    end
end

network.pickRandomNode = function()
    local randomListPick = {};
    if #gossip.config.seedList > 0 then
        randomListPick = node.random(1, #gossip.config.seedList);
        utils.debug('Randomly picked: ' .. gossip.config.seedList[randomListPick]);
        return gossip.config.seedList[randomListPick];
    end
    utils.debug('Seedlist is empty. Please provide one or wait for node to be contacted.');
    return nil;
end

network.sendData = function(ip, data, sendType)
    local outboundSocket = net.createUDPSocket();
    data.type = sendType;
    local dataToSend = sjson.encode(data);
    data.type = nil;
    outboundSocket:send(gossip.config.comPort, ip, dataToSend);
    utils.debug('Sent ' .. sendType .. ' to ' .. ip);
    outboundSocket:close();
end

network.receiveSyn = function(ip, updateData)
    utils.debug('Received SYN from ' .. ip);
    local diff = utils.getNetworkStateDiff(updateData);
    network.updateNetworkState(updateData);
    network.sendData(ip, diff, constants.updateType.ACK);
end

network.receiveAck = function(ackIp, updateData)
    utils.debug('Received ACK from ' .. ackIp);
    local dataToUpdate = '';
    local nodeUpdated = false;
    for ip, nodeData in pairs(updateData) do
        if gossip.networkState[ip] == nil then
            network.addNewNode(ip, nodeData);
            nodeUpdated = true;
        elseif utils.compareNodeData(gossip.networkState[ip], updateData[ip]) == 1 then
            gossip.networkState[ip] = nodeData;
            nodeUpdated = true;
        end
        if nodeUpdated then dataToUpdate = dataToUpdate .. ip .. ' '; end
    end
    if #dataToUpdate > 1 then
        utils.debug('Updated via ACK from peer : ' .. dataToUpdate);
    end
end

network.stateUpdate = function()
    return function(socket, data, port, ip)
        if gossip.networkState[ip] ~= nil then
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
                            '. No type specified.');
            return;
        end
    end
end

-- Constants

constants.nodeState = {
    TICK = 1,
    UP = 0,
    SUSPECT = 2,
    DOWN = 3,
    REMOVE = 4
};

constants.defaultConfig = {
    seedList = {},
    roundInterval = 10000,
    comPort = 5000,
    debug = false
};

constants.initialState = {
    revision = 1,
    heartbeat = 0,
    state = constants.nodeState.UP
};

constants.updateType = {
    ACK = 'ACK',
    SYN = 'SYN'
}

constants.revFileName = 'gossip/rev.dat';

-- Return

gossip = {
    started = false,
    config = constants.defaultConfig,
    currentState = constants.initialState,
    setConfig = utils.setConfig,
    start = state.start,
    setRevManually = state.setRevManually,
    networkState = {},
    getNetworkState = utils.getNetworkState
};

if (net == nil or file == nil or tmr == nil or wifi == nil) then
    error('Gossip requires these modules to work: net, file, tmr, wifi');
else
    return gossip;
end

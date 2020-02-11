-- Gossip protocol implementation tests
-- https://github.com/alexandruantochi/
local gossipSubmodules = loadfile('gossip.lua')('test');

local gossip = gossipSubmodules._gossip;
local constants = gossipSubmodules._constants;
local utils = gossipSubmodules._utils;
local network = gossipSubmodules._network;
local state = gossipSubmodules._state;

-- test constants and mocks

local function dummy() return nil; end

-- luacheck: push allow defined
tmr = {};
tmr.time = function() return 200; end
sjson = {};
sjson.decode = function(data) return data; end
file = {};
file.exists = dummy
file.putcontents = dummy
-- luacheck: pop

local Ip_1 = '192.168.0.1';
local Ip_2 = '192.168.0.2';

-- test runner

local Test = {};

local RunTests = function()
  local failures = {};
  print('\nRunning tests...\n');
  for testName, test in pairs(Test) do
    if type(test) == 'function' then
      local result = testName .. ': ';
      local passed, res = pcall(test);
      if passed then
        result = result .. ' Passed.';
      else
        result = result .. ' Failed ->';
        result = '>>>' .. result .. res;
        table.insert(failures, testName);
      end
      print(result);
    end
  end
  if (#failures ~= 0) then
    print('\n\n');
    print('Failed tests (' .. #failures .. '): \n');
    for k in pairs(failures) do print(failures[k]); end
    print('\n');
  end
end

-- utils

function Test.utils_contains()
    local seedList = {};
    assert(not utils.contains(seedList, Ip_1));
    table.insert(seedList, Ip_1);
    assert(utils.contains(seedList, Ip_1));
    table.insert(seedList, Ip_2);
    assert(utils.contains(seedList, Ip_1) and utils.contains(seedList, Ip_2));
end

function Test.utils_setConfig()
  local config = {
    seedList = {Ip_1},
    roundInterval = 1500,
    comPort = 8000,
    junk = 'junk'
  };
  gossip.config = constants.defaultConfig;
  utils.setConfig(config);

  assert(#gossip.config.seedList == 1, 'Config failed when adding seedList');
  assert(gossip.config.seedList[1] == Ip_1,
         'Config failed to add ip to seedList');
  assert(gossip.config.roundInterval == 1500,
         'Config failed to add round interval.');
  assert(gossip.config.comPort == 8000, 'Config failed to add comPort.');
  assert(gossip.config.debug == false, 'Debug should be false.');
  assert(gossip.config.junk == nil, 'Junk data inserted in config.');

  gossip.config = constants.defaultConfig;
end

function Test.utils_compare()
  assert(utils.compare(1, 2) == 1);
  assert(utils.compare(2, 1) == -1);
  assert(utils.compare(0, 0) == 0);
end

function Test.utils_compareNodeData_on_revision()
  local networkData_1 = {
    revision = 1,
    heartbeat = 500,
    state = constants.nodeState.UP
  };
  local networkData_2 = {
    revision = 2,
    heartbeat = 500,
    state = constants.nodeState.UP
  };
  assert(utils.compareNodeData(networkData_1, networkData_2) == 1);
  assert(utils.compareNodeData(networkData_2, networkData_1) == -1);
  networkData_1.revision = networkData_2.revision;
  assert(utils.compareNodeData(networkData_1, networkData_2) == 0);
end

function Test.utils_compareNodeData_on_heartbeat()
  local networkData_1 = {
    revision = 1,
    heartbeat = 500,
    state = constants.nodeState.UP
  };
  local networkData_2 = {
    revision = 1,
    heartbeat = 600,
    state = constants.nodeState.UP
  };
  assert(utils.compareNodeData(networkData_1, networkData_2) == 1);
  assert(utils.compareNodeData(networkData_2, networkData_1) == -1);
  networkData_1.heartbeat = networkData_2.heartbeat;
  assert(utils.compareNodeData(networkData_1, networkData_2) == 0);
end

function Test.utils_compareNodeData_on_state()
  local networkData_1 = {
    revision = 1,
    heartbeat = 500,
    state = constants.nodeState.UP
  };
  local networkData_2 = {
    revision = 1,
    heartbeat = 500,
    state = constants.nodeState.SUSPECT
  };
  assert(utils.compareNodeData(networkData_1, networkData_2) == 1);
  assert(utils.compareNodeData(networkData_2, networkData_1) == -1);
  networkData_1.state = networkData_2.state;
  assert(utils.compareNodeData(networkData_1, networkData_2) == 0);
end

function Test.utils_compareNodeData_on_bad_data()
  local networkData_1 = {
    revision = 1,
    heartbeat = nil,
    state = constants.nodeState.UP
  };
  local networkData_2 = {
    revision = 1,
    heartbeat = 600,
    state = constants.nodeState.UP
  };
  assert(utils.compareNodeData(networkData_1, networkData_2) == 1);
  assert(utils.compareNodeData(networkData_2, networkData_1) == -1);
  networkData_2.state = nil;
  assert(utils.compareNodeData(networkData_1, networkData_2) == 0);
end

function Test.utils_getMinus()
  local data1 = {};
  local data2 = {};

  data1[Ip_1] = {
    revision = 1,
    heartbeat = 500,
    state = constants.nodeState.UP
  };
  data1[Ip_2] = {
    revision = 1,
    heartbeat = 400,
    state = constants.nodeState.UP
  };
  data2[Ip_1] = {
    revision = 1,
    heartbeat = 400,
    state = constants.nodeState.UP
  };
  data2[Ip_2] = {
    revision = 1,
    heartbeat = 400,
    state = constants.nodeState.SUSPECT;
  };

  --local diff1 = utils.getMinus(data1, data2);
  local diff2 = utils.getMinus(data2, data1);

  --assert(diff1[Ip_1] ~= nil and diff1[Ip_2] == nil);
  assert(diff2[Ip_1] == nil and diff2[Ip_2] ~= nil);

end

-- state

function Test.state_setRev()
  gossip.ip = Ip_1;
  gossip.networkState[Ip_1] = {};
  gossip.networkState[Ip_1].revision = -1;
  assert(state.setRev() == 0, 'Revision not initialized to 0.');
end

function Test.state_tickNodeState()
  local ip_1 = Ip_1;
  local ip_2 = Ip_2;
  gossip.networkState[ip_1] = {};
  gossip.networkState[ip_2] = {};
  gossip.networkState[ip_1].state = constants.nodeState.UP;
  gossip.networkState[ip_2].state = constants.nodeState.DOWN;
  state.tickNodeState(ip_1);
  state.tickNodeState(ip_2);
  assert(gossip.networkState[ip_1].state == constants.nodeState.UP +
             constants.nodeState.TICK);
  assert(gossip.networkState[ip_2].state == constants.nodeState.REMOVE);
  state.tickNodeState(ip_1);
  assert(gossip.networkState[ip_1].state == constants.nodeState.SUSPECT);
  gossip.networkState = {};
end

-- network

function Test.network_updateNetworkState_no_callback()
  local updateData = {}
  updateData[Ip_1] = {
    revision = 1,
    heartbeat = 400,
    state = constants.nodeState.UP
  };
  updateData[Ip_2] = {
    revision = 1,
    heartbeat = 700,
    state = constants.nodeState.UP
  };
  network.updateNetworkState(updateData);
  -- send duplicate data
  network.updateNetworkState(updateData);
  assert(#gossip.config.seedList == 2);
  assert(gossip.config.seedList[1] == Ip_1);
  assert(gossip.config.seedList[2] == Ip_2);
  assert(gossip.networkState[Ip_1] ~= nil and gossip.networkState[Ip_2] ~= nil);
  gossip.networkState = {};
  gossip.config = constants.defaultConfig;
end

function Test.network_updateNetworkState_with_callback()
  local callbackTriggered = false;
  local function updateCallback() callbackTriggered = true; end
  gossip.updateCallback = updateCallback;
  Test.network_updateNetworkState_no_callback();
  assert(callbackTriggered);
  gossip.updateCallback = nil;
end

function Test.network_receiveData_when_receive_syn()
  local originalReceiveSyn = network.receiveSyn;
  local receiveSynCalled = false;
  network.receiveSyn = function() receiveSynCalled = true; end
  network.receiveData('socket', {type = constants.updateType.SYN});
  network.receiveSyn = originalReceiveSyn;
  assert(receiveSynCalled);
end

function Test.network_receiveData_when_receive_ack()
  local originalReceiveAck = network.receiveAck;
  local receiveAckCalled = false;
  network.receiveAck = function() receiveAckCalled = true; end
  network.receiveData('socket', {type = constants.updateType.ACK});
  network.receiveAck = originalReceiveAck;
  assert(receiveAckCalled);
end

-- run tests

RunTests();

-- Gossip protocol implementation tests
-- https://github.com/alexandruantochi/
local gossipSubmodules = require('gossip');

local gossip = gossipSubmodules._gossip;
local constants = gossipSubmodules._constants;
local utils = gossipSubmodules._utils;
local network = gossipSubmodules._network;
local state = gossipSubmodules._state;

-- test constants and mocks

--luacheck: push allow defined
tmr = {};
tmr.time = function() return 200; end
sjson = {};
sjson.decode = function(data)
  return data;
end
file = io;
file.exists = io.type;
file.putcontents = io.type;
--luacheck: pop

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
    print('Failed tests:');
    print('\n');
    for k  in pairs(failures) do print(failures[k]); end
  end
end

-- utils

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

function Test.utils_getNetworkStateDiff()
  local synData = {}
  gossip.networkState[Ip_1] = {
    revision = 1,
    heartbeat = 500,
    state = constants.nodeState.UP
  };
  gossip.networkState[Ip_2] = {
    revision = 1,
    heartbeat = 500,
    state = constants.nodeState.UP
  };
  synData[Ip_1] = {
    revision = 1,
    heartbeat = 400,
    state = constants.nodeState.UP
  };
  synData[Ip_2] = {
    revision = 1,
    heartbeat = 700,
    state = constants.nodeState.UP
  };

  local update, diff = utils.getUpdateDiffDelta(synData);
  gossip.networkState = {};

  assert(update[Ip_2] ~= nil and update[Ip_1] == nil);
  assert(diff[Ip_1] ~= nil and diff[Ip_2] == nil);

end

-- state

function Test.state_setRev()
    gossip.currentState.revision = -1;
    state.setRev();
    assert(gossip.currentState.revision == 0, 'Revision not initialized to 0.');
    gossip.setRevManually(5);
    assert(gossip.currentState.revision == 5, 'Revision not manually set.');
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
  assert(#gossip.config.seedList == 2);
  assert(gossip.config.seedList[1] == Ip_1);
  assert(gossip.config.seedList[2] == Ip_2);
  assert(gossip.networkState[Ip_1] ~= nil and gossip.networkState[Ip_2] ~= nil);
  gossip.networkState = {};
  gossip.config.seedList = {};
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

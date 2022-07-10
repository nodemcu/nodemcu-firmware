# ESPGossip

| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2020-01-20 | [alexandruantochi](https://github.com/alexandruantochi) | [alexandruantochi](https://github.com/alexandruantochi) | [gossip.lua](../../lua_modules/gossip/gossip.lua) |


This module is based on the gossip protocol and it can be used to disseminate information through the network to other nodes. The time it takes for the information to reach all nodes is logN. For every round number n, 2^n nodes will receive the information. 

### Require
```lua
gossip = require('gossip')
```

### Release
```lua
gossip.inboundSocket:close()
gossip = nil
```

## Usage
```lua
config = {
    seedList = { '192.168.0.1', '192.168.0.15' },
    debug = true,
    debugOutput = print
}
gossip = require ("gossip")
gossip.setConfig(config)
gossip.start()
```

## Strategy

Each controller will randomly pick an IP from it's seed list. It will send a `SYN` request to that IP and set receiving node's `state` to an intermediary state between `Up` and `Suspect`. The node that receives the `SYN` request will compute a diff on the received networkState vs own networkState. It will then send that diff as an `ACK` request. If there is no data to send, it will only send an `ACK`. When the `ACK` is received, the sender's state will revert to `Up` and the receiving node will update it's own networkState using the diff (based on the `ACK` reply).

Gossip will establish if the information received from another node has fresher data by first comparing the `revision`, then the `heartbeat` and lastly the `state`. States that are closer to `DOWN` have priority as an offline node does not update it's heartbeat.

Any other parameter can be sent along with the mandatory `revision`, `heartbeat` and `state` thus allowing the user to spread information around the network. Every time a node receives 'fresh' data, the `gossip.updateCallback` will be called with that data as the first parameter.

Currently there is no implemented deletion for nodes that are down except for the fact that their status is signaled as `REMOVE`.

## Example use-case

There are multiple modules on the network that measure temperature. We want to know the maximum and minimum temperature at a given time and have every node display it.

The brute force solution would be to query each node from a single point and save the `min` and `max` values, then go back to each node and present them with the computed `min` and `max`. This requires n*2 rounds, where n is the number of nodes. It also opens the algorithm to a single point of failure (the node that is in charge of gathering the data).

Using gossip, one can have the node send it's latest value through `SYN` or `pushGossip()` and use the `callbackUpdate` function to compare the values from other nodes to it's own. Based on that, the node will display the values it knows about by gossiping with others. The data will be transmitted in ~log(n) rounds, where n is the number of nodes.

## Terms

`revision` : generation of the node; if a node restarts, the revision will be increased by one. The revision data is stored as a file to provide persistency

`heartBeat` : the node uptime in seconds (`tmr.time()`). This is used to help the other nodes figure out if the information about that particular node is newer. 

`networkState` : the list with the state of the network composed of the `ip` as a key and `revision`, `heartBeat` and `state` as values packed in a table.

`state` : all nodes start with a state set to `UP` and when a node sends a `SYN` request, it will mark the destination node in an intermediary state until it receives an `ACK` or a `SYN` from it. If a node receives any message, it will mark that senders IP as `UP` as this provides proof that the node is online. 


## setConfig()

#### Syntax
```lua
gossip.setConfig(config)
```

Sets the configuration for gossip. The available options are:

`seedList` : the list of seeds gossip will start with; this will be updated as new nodes are discovered. Note that it's enough for all nodes to start with the same IP in the seedList, as once they have one seed in common, the data will propagate. If the seedList is empty a broadcast is sent, so this can be used for automatic discovery of nodes.

`roundInterval`: interval in milliseconds at which gossip will pick a random node from the seed list and send a `SYN` request

`comPort` : port for the listening UDP socket

`debug` : flag that will provide debugging messages

`debugOutput` : if debug is set to `true`, then this method will be used as a callback with the debug message as the first parameter

```lua
config = {
    seedList = {'192.168.0.54','192.168.0.55'},
    roundInterval = 10000,
    comPort = 5000,
    debug = true,
    debugOutput = function(message) print('Gossip says: '..message); end
}
```

If any of them is not provided, the values will default:

`seedList` : nil

`roundInterval`: 10000 (10 seconds)

`comPort` : 5000

`debug` : false

`debugOutput` : print

## start()

#### Syntax
```lua
gossip.start()
```

Starts gossip, sets the `started` flag to true and initiates the `revision`. The revision (generation) main purpose is like a persistent heartbeat, as the heartbeat (measured by uptime in seconds) will obviously revert to 0. 

## callbackFunction

#### Syntax
```lua
gossip.callbackFunction = function(data)
  processData(data)
end

-- stop the callback
gossip.callbackFunction = nil
```

If declared, this function will get called every time there is a `SYN` with new data.

## pushGossip()

#### Syntax

```lua
gossip.pushGossip(data, [ip])

-- remove data
gossip.pushGossip(nil, [ip])
```

Send a `SYN` request outside of the normal gossip round. The IP is optional and if none given, it will pick a random node.

```
!!! note
. By calling `pushGossip(nil)` you effectively remove the `data` table from the node's network state and notify other nodes of this.
```
## setRevManually()

#### Syntax

```lua
gossip.setRevFileValue(number)
```

The only scenario when rev should be set manually is when a new node is added to the network and has the same IP. Having a smaller revision than the previous node with the same IP would make gossip think the data it received is old, thus ignoring it.

```
!!! note

The revision file value will only be read when gossip starts and it will be incremented by one.
```

## getNetworkState()

#### Syntax

```lua
networkState = gossip.getNetworkState()
print(networkState)
```

The network state can be directly accessed as a Lua table : `gossip.networkState` or it can be received as a JSON with this method.

#### Returns

JSON formatted string regarding the network state.

Example:

```JSON
{
  "192.168.0.53": {
    "state": 3,
    "revision": 25,
    "heartbeat": 2500,
    "extra" : "this is some extra info from node 53"
  },
  "192.168.0.75": {
    "state": 0,
    "revision": 4,
    "heartbeat": 6500
  }
}
```


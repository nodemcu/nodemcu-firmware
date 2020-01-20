# ESPGossip

| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2020-01-20 | [alexandruantochi](https://github.com/alexandruantochi) | [alexandruantochi](https://github.com/alexandruantochi) | [gossip.lua](../../lua_modules/gossip/gossip.lua) |


This module is based on the gossip protocol and usefull for use cases when multiple controllers are on the same network and they need to know about each other's state or when a single point of failure (such as an entity that queries each controller in particular) is to be avoided. It runs using UDP, thus providing faster communication and less network noise. 

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

Each controller will randomly pick an IP from it's seed list. It will send a `SYN` request to that IP and set recieving node's `state` to an intermediary state between `Up` and `Suspect`. The node that recieves the `SYN` request will compute a diff on the received networkState vs own networkState. It will then send that diff as an `ACK` request. If there is no data to send, it will only send an `ACK`. When the `ACK` is received, the sender's state will revert to `Up` and the receiving node will update it's own networkState using the diff (based on the `ACK` reply).

Gossip will establish if the information received from another node has fresher data by first comparing the `revision`, then the `heartbeat` and lastly the `state`. States that are closer to `DOWN` have priority as an offline node does not update it's heartbeat.

Currently there is no implemented deletion for nodes that are down except for the fact that their status is signaled as `REMOVE`.

## Terms

`revision` : generation of the node; if a node restarts, the revision will be increased by one. The revision data is stored as a file to provide persistency

`heartBeat` : the node uptime in seconds (`tmr.time()`). This is used to help the other nodes figure out if the information about that particular node is newer. 

`networkState` : the list with the state of the network composed of the `ip` as a key and `revision`, `heartBeat` and `state` as values packed in a table.

`state` : all nodes start with a state set to `UP` and when a node sends a `SYN` request, it will mark the destination node in an intermediary state until it receives an `ACK` or a `SYN` from it. Basicaly, if a node receives any message, it will mark that senders IP as `UP` as this provides proof that the node is online. 


## setConfig()
#### Syntax
```lua
gossip.setConfig(config)
```

Sets the configuration for gossip. The available options are:

`seedList` : the list of seeds gossip will start with; this will be updated as new nodes are discovered. Note that it's enough for all nodes to start with the same IP in the seedList, as once they have one seed in common, the data will propagate

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

Starts gossip, sets the `started` flag to true and initiates the `revision`. The revision (generation) main purpose is like a persisten heartbeat, as the heartbeat (measured by uptime in seconds) will obviously revert to 0. 

## setRevManually()

#### Syntax

```lua
gossip.setRevManually(number)
```

The only scenario when rev should be set manually is when a new node is added to the network and has the same ip. Having a smaller revision than the previous node with the same ip would make gossip think the data it received is old, thus ignoring it.

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
    "heartbeat": 2500
  },
  "192.168.0.75": {
    "state": 0,
    "revision": 4,
    "heartbeat": 6500
  }
}
```


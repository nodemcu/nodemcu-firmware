# net Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-22 | [Zeroday](https://github.com/funshine) | [PhoeniX](https://github.com/djphoenix) | [net.c](../../../app/modules/net.c)|

** TLS operations was moved to the [TLS](tls.md) module **

## Constants
Constants to be used in other functions: `net.TCP`, `net.UDP`

## net.createConnection()

Creates a client.

#### Syntax
`net.createConnection([type[, secure]])`

#### Parameters
- `type` `net.TCP` (default) or `net.UDP`
- `secure` 1 for encrypted, 0 for plain (default)

!!! attention
    This will change in upcoming releases so that `net.createConnection` will always create an unencrypted TCP connection.

    There's no such thing as a UDP _connection_ because UDP is connection*less*. Thus no connection `type` parameter should be required. For UDP use [net.createUDPSocket()](#netcreateudpsocket) instead. To create *secure* connections use [tls.createConnection()](tls.md#tlscreateconnection) instead.

#### Returns

- for `net.TCP` - net.socket sub module
- for `net.UDP` - net.udpsocket sub module
- for `net.TCP` with `secure` - tls.socket sub module

#### Example

```lua
net.createConnection(net.TCP, 0)
```

#### See also
[`net.createServer()`](#netcreateserver), [`net.createUDPSocket()`](#netcreateudpsocket), [`tls.createConnection()`](tls.md#tlscreateconnection)

## net.createServer()

Creates a server.

#### Syntax
`net.createServer([type[, timeout]])`

#### Parameters
- `type` `net.TCP` (default) or `net.UDP`
- `timeout` for a TCP server timeout is 1~28'800 seconds, 30 sec by default (for an inactive client to be disconnected)

!!! attention
    The `type` parameter will be removed in upcoming releases so that `net.createServer` will always create a TCP-based server. For UDP use [net.createUDPSocket()](#netcreateudpsocket) instead.

#### Returns

- for `net.TCP` - net.server sub module
- for `net.UDP` - net.udpsocket sub module

#### Example

```lua
net.createServer(net.TCP, 30) -- 30s timeout
```

#### See also
[`net.createConnection()`](#netcreateconnection), [`net.createUDPSocket()`](#netcreateudpsocket)

## net.createUDPSocket()

Creates an UDP socket.

#### Syntax
`net.createUDPSocket()`

#### Parameters
none

#### Returns
[net.udpsocket sub module](#netudpsocket-module)

#### See also
[`net.createConnection()`](#netcreateconnection)

## net.multicastJoin()

Join multicast group.

#### Syntax
`net.multicastJoin(if_ip, multicast_ip)`

#### Parameters
- `if_ip` string containing the interface ip to join the multicast group. "any" or "" affects all interfaces.
- `multicast_ip` of the group to join

#### Returns
`nil`

## net.multicastLeave()

Leave multicast group.

#### Syntax
`net.multicastLeave(if_ip, multicast_ip)`

#### Parameters
- `if_ip` string containing the interface ip to leave the multicast group. "any" or "" affects all interfaces.
- `multicast_ip` of the group to leave

#### Returns
`nil`

# net.server Module

## net.server:close()

Closes the server.

#### Syntax
`net.server.close()`

#### Parameters
none

#### Returns
`nil`

#### Example
```lua
-- creates a server
sv = net.createServer(net.TCP, 30)
-- closes the server
sv:close()
```

#### See also
[`net.createServer()`](#netcreateserver)

## net.server:listen()

Listen on port from IP address.

#### Syntax
`net.server.listen([port],[ip],function(net.socket))`

#### Parameters
- `port` port number, can be omitted (random port will be chosen)
- `ip` IP address string, can be omitted
- `function(net.socket)` callback function, pass to caller function as param if a connection is created successfully

#### Returns
`nil`

#### Example
```lua
-- server listens on 80, if data received, print data to console and send "hello world" back to caller
-- 30s time out for a inactive client
sv = net.createServer(net.TCP, 30)

function receiver(sck, data)
  print(data)
  sck:close()
end

if sv then
  sv:listen(80, function(conn)
    conn:on("receive", receiver)
    conn:send("hello world")
  end)
end
```

#### See also
[`net.createServer()`](#netcreateserver)

## net.server:getaddr()

Returns server local address/port.

#### Syntax
`net.server.getaddr()`

#### Parameters
none

#### Returns
`port`, `ip` (or `nil, nil` if not listening)

#### See also
[`net.server:listen()`](#netserverlisten)


# net.socket Module
## net.socket:close()

Closes socket.

#### Syntax
`close()`

#### Parameters
none

#### Returns
`nil`

#### See also
[`net.createServer()`](#netcreateserver)

## net.socket:connect()

Connect to a remote server.

#### Syntax
`connect(port, ip|domain)`

#### Parameters
- `port` port number
- `ip` IP address or domain name string

#### Returns
`nil`

#### See also
[`net.socket:on()`](#netsocketon)

## net.socket:dns()

Provides DNS resolution for a hostname.

#### Syntax
`dns(domain, function(net.socket, ip))`

#### Parameters
- `domain` domain name
- `function(net.socket, ip)` callback function. The first parameter is the socket, the second parameter is the IP address as a string.

#### Returns
`nil`

#### Example
```lua
sk = net.createConnection(net.TCP, 0)
sk:dns("www.nodemcu.com", function(conn, ip) print(ip) end)
sk = nil
```

#### See also
[`net.createServer()`](#netcreateserver)

## net.socket:getpeer()

Retrieve port and ip of remote peer.

#### Syntax
`getpeer()`

#### Parameters
none

#### Returns
`port`, `ip` (or `nil, nil` if not connected)

## net.socket:getaddr()

Retrieve local port and ip of socket.

#### Syntax
`getaddr()`

#### Parameters
none

#### Returns
`port`, `ip` (or `nil, nil` if not connected)

## net.socket:hold()

Throttle data reception by placing a request to block the TCP receive function. This request is not effective immediately, Espressif recommends to call it while reserving 5*1460 bytes of memory.

#### Syntax
`hold()`

#### Parameters
none

#### Returns
`nil`

#### See also
[`net.socket:unhold()`](#netsocketunhold)

## net.socket:on()

Register callback functions for specific events.

#### Syntax
`on(event, function())`

#### Parameters
- `event` string, which can be "connection", "reconnection", "disconnection", "receive" or "sent"
- `function(net.socket[, string])` callback function. Can be `nil` to remove callback.

The first parameter of callback is the socket.

- If event is "receive", the second parameter is the received data as string.
- If event is "disconnection" or "reconnection", the second parameter is error code.

If reconnection event is specified, disconnection receives only "normal close" events.

Otherwise, all connection errors (with normal close) passed to disconnection event.

#### Returns
`nil`

#### Example
```lua
srv = net.createConnection(net.TCP, 0)
srv:on("receive", function(sck, c) print(c) end)
-- Wait for connection before sending.
srv:on("connection", function(sck, c)
  -- 'Connection: close' rather than 'Connection: keep-alive' to have server 
  -- initiate a close of the connection after final response (frees memory 
  -- earlier here), https://tools.ietf.org/html/rfc7230#section-6.6 
  sck:send("GET /get HTTP/1.1\r\nHost: httpbin.org\r\nConnection: close\r\nAccept: */*\r\n\r\n")
end)
srv:connect(80,"httpbin.org")
```
!!! note
    The `receive` event is fired for every network frame! Hence, if the data sent to the device exceeds 1460 bytes (derived from [Ethernet frame size](https://en.wikipedia.org/wiki/Ethernet_frame)) it will fire more than once. There may be other situations where incoming data is split across multiple frames (e.g. HTTP POST with `multipart/form-data`). You need to manually buffer the data and find means to determine if all data was received.
    
```lua
local buffer = nil

srv:on("receive", function(sck, c)
  if buffer == nil then
    buffer = c
  else
    buffer = buffer .. c
  end
end)
-- throttling could be implemented using socket:hold()
-- example: https://github.com/nodemcu/nodemcu-firmware/blob/master/lua_examples/pcm/play_network.lua#L83
```    

#### See also
- [`net.createServer()`](#netcreateserver)
- [`net.socket:hold()`](#netsockethold)

## net.socket:send()

Sends data to remote peer.

#### Syntax
`send(string[, function(sent)])`

`sck:send(data, fnA)` is functionally equivalent to `sck:send(data) sck:on("sent", fnA)`.

#### Parameters
- `string` data in string which will be sent to server
- `function(sent)` callback function for sending string

#### Returns
`nil`

#### Note

Multiple consecutive `send()` calls aren't guaranteed to work (and often don't) as network requests are treated as separate tasks by the SDK. Instead, subscribe to the "sent" event on the socket and send additional data (or close) in that callback. See [#730](https://github.com/nodemcu/nodemcu-firmware/issues/730#issuecomment-154241161) for details.

#### Example
```lua
srv = net.createServer(net.TCP)

function receiver(sck, data)
  local response = {}

  -- if you're sending back HTML over HTTP you'll want something like this instead
  -- local response = {"HTTP/1.0 200 OK\r\nServer: NodeMCU on ESP8266\r\nContent-Type: text/html\r\n\r\n"}

  response[#response + 1] = "lots of data"
  response[#response + 1] = "even more data"
  response[#response + 1] = "e.g. content read from a file"

  -- sends and removes the first element from the 'response' table
  local function send(localSocket)
    if #response > 0 then
      localSocket:send(table.remove(response, 1))
    else
      localSocket:close()
      response = nil
    end
  end

  -- triggers the send() function again once the first chunk of data was sent
  sck:on("sent", send)

  send(sck)
end

srv:listen(80, function(conn)
  conn:on("receive", receiver)
end)
```
If you do not or can not keep all the data you send back in memory at one time (remember that `response` is an aggregation) you may use explicit callbacks instead of building up a table like so:

```lua
sck:send(header, function() 
  local data1 = "some large chunk of dynamically loaded data"
  sck:send(data1, function()
    local data2 = "even more dynamically loaded data"
    sck:send(data2, function(sk) 
      sk:close()
    end)
  end)
end)
```

#### See also
[`net.socket:on()`](#netsocketon)

## net.socket:ttl()

Changes or retrieves Time-To-Live value on socket.

#### Syntax
`ttl([ttl])`

#### Parameters
- `ttl` (optional) new time-to-live value

#### Returns
current / new ttl value

#### Example
```lua
sk = net.createConnection(net.TCP, 0)
sk:connect(80, '192.168.1.1')
sk:ttl(1) -- restrict frames to single subnet
```

#### See also
[`net.createConnection()`](#netcreateconnection)

## net.socket:unhold()

Unblock TCP receiving data by revocation of a preceding `hold()`.

#### Syntax
`unhold()`

#### Parameters
none

#### Returns
`nil`

#### See also
[`net.socket:hold()`](#netsockethold)

# net.udpsocket Module

Remember that in contrast to TCP [UDP](https://en.wikipedia.org/wiki/User_Datagram_Protocol) is connectionless. Therefore, there is a minor but natural mismatch as for TCP/UDP functions in this module. While you would call [net.createConnection()](#netcreateconnection) for TCP it is [net.createUDPSocket()](#netcreateudpsocket) for UDP.

Other points worth noting:

- UDP sockets do not have a connection callback for the [`listen`](#netudpsocketlisten) function.
- UDP sockets do not have a `connect` function. Remote IP and port thus need to be defined in [`send()`](#netudpsocketsend).
- UDP socket's `receive` callback receives port/ip after the `data` argument.

## net.udpsocket:close()

Closes UDP socket.

The syntax and functional identical to [`net.socket:close()`](#netsocketclose).

## net.udpsocket:listen()

Listen on port from IP address.

The syntax and functional similar to [`net.server:listen()`](#netserverlisten), but callback parameter is not provided.

## net.udpsocket:on()

Register callback functions for specific events.

The syntax and functional similar to [`net.socket:on()`](#netsocketon). However, only "receive", "sent" and "dns" are supported events.

!!! note
	The `receive` callback receives `port` and `ip` *after* the `data` argument.

## net.udpsocket:send()

Sends data to specific remote peer.

#### Syntax
`send(port, ip, data)`

#### Parameters
- `port` remote socket port
- `ip` remote socket IP
- `data` the payload to send

#### Returns
`nil`

#### Example
```lua
udpSocket = net.createUDPSocket()
udpSocket:listen(5000)
udpSocket:on("receive", function(s, data, port, ip)
    print(string.format("received '%s' from %s:%d", data, ip, port))
    s:send(port, ip, "echo: " .. data)
end)
port, ip = udpSocket:getaddr()
print(string.format("local UDP socket address / port: %s:%d", ip, port))
```
On *nix systems that can then be tested by issuing

```
echo -n "foo" | nc -w1 -u <device-IP-address> 5000
```


## net.udpsocket:dns()

Provides DNS resolution for a hostname.

The syntax and functional identical to [`net.socket:dns()`](#netsocketdns).

## net.udpsocket:getaddr()

Retrieve local port and ip of socket.

The syntax and functional identical to [`net.socket:getaddr()`](#netsocketgetaddr).

## net.udpsocket:ttl()

Changes or retrieves Time-To-Live value on socket.

The syntax and functional identical to [`net.socket:ttl()`](#netsocketttl).

# net.dns Module

## net.dns.getdnsserver()

Gets the IP address of the DNS server used to resolve hostnames.

#### Syntax
`net.dns.getdnsserver(dns_index)`

#### Parameters
dns_index which DNS server to get (range 0~1)

#### Returns
IP address (string) of DNS server

#### Example
```lua
print(net.dns.getdnsserver(0)) -- 208.67.222.222
print(net.dns.getdnsserver(1)) -- nil

net.dns.setdnsserver("8.8.8.8", 0)
net.dns.setdnsserver("192.168.1.252", 1)

print(net.dns.getdnsserver(0)) -- 8.8.8.8
print(net.dns.getdnsserver(1)) -- 192.168.1.252
```
#### See also
[`net.dns:setdnsserver()`](#netdnssetdnsserver)

## net.dns.resolve()

Resolve a hostname to an IP address. Doesn't require a socket like [`net.socket.dns()`](#netsocketdns).

#### Syntax
`net.dns.resolve(host, function(sk, ip))`

#### Parameters
- `host` hostname to resolve
- `function(sk, ip)` callback called when the name was resolved. `sk` is always `nil`

#### Returns
`nil`

#### Example
```lua
net.dns.resolve("www.google.com", function(sk, ip)
    if (ip == nil) then print("DNS fail!") else print(ip) end
end)
```
#### See also
[`net.socket:dns()`](#netsocketdns)

## net.dns.setdnsserver()

Sets the IP of the DNS server used to resolve hostnames. Default: resolver1.opendns.com (208.67.222.222). You can specify up to 2 DNS servers.

#### Syntax
`net.dns.setdnsserver(dns_ip_addr, dns_index)`

#### Parameters
- `dns_ip_addr` IP address of a DNS server
- `dns_index` which DNS server to set (range 0~1). Hence, it supports max. 2 servers. 

#### Returns
`nil`

#### See also
[`net.dns:getdnsserver()`](#netdnsgetdnsserver)

# net.cert Module

This part gone to the [TLS](tls.md) module, link kept for backward compatibility.

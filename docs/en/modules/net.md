# net Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-22 | [Zeroday](https://github.com/funshine) | [Zeroday](https://github.com/funshine) | [net.c](../../../app/modules/net.c)|

**SSL/TLS support**

Secure connections use **TLS 1.1** with the following cipher suites:

- `TLS_RSA_WITH_AES_128_CBC_SHA`
- `TLS_RSA_WITH_AES_256_CBC_SHA`
- `TLS_RSA_WITH_RC4_128_SHA`
- `TLS_RSA_WITH_RC4_128_MD5`

This specification is imposed by the [axTLS library](http://axtls.sourceforge.net/specifications.htm) used by the SDK. 


## Constants
Constants to be used in other functions: `net.TCP`, `net.UDP`

## net.createConnection()

Creates a client.

#### Syntax
`net.createConnection(type, secure)`

#### Parameters
- `type` `net.TCP` or `net.UDP`
- `secure` 1 for encrypted, 0 for plain

#### Returns
net.socket sub module

#### Example

```lua
net.createConnection(net.UDP, 0)
```

#### See also
[`net.createServer()`](#netcreateserver)

## net.createServer()

Creates a server.

#### Syntax
`net.createServer(type, timeout)`

#### Parameters
- `type` `net.TCP` or `net.UDP`
- `timeout` for a TCP server timeout is 1~28'800 seconds (for an inactive client to be disconnected)

#### Returns
net.server sub module

#### Example

```lua
net.createServer(net.TCP, 30) -- 30s timeout
```

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
`net.server.listen(port,[ip],function(net.socket))`

#### Parameters
- `port` port number
- `ip` IP address string, can be omitted
- `function(net.socket)` callback function, pass to caller function as param if a connection is created successfully

#### Returns
`nil`

#### Example
```lua
 -- 30s time out for a inactive client
sv = net.createServer(net.TCP, 30)
-- server listens on 80, if data received, print data to console and send "hello world" back to caller
sv:listen(80, function(c)
  c:on("receive", function(c, pl) 
    print(pl)
  end)
  c:send("hello world")
end)
```

#### See also
[`net.createServer()`](#netcreateserver)

## net.server:on()

UDP server only: Register callback functions for specific events.

#### See also
[`net.socket:on()`](#netsocketon)

## net.server:send()

UDP server only: Sends data to remote peer.

#### See also
[`net.socket:send()`](#netsocketsend)

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

Retrieve port and ip of peer.

#### Syntax
`getpeer()`

#### Parameters
none

#### Returns
- `ip` of peer
- `port` of peer

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
- `function(net.socket[, string])` callback function. The first parameter is the socket. If event is "receive", the second parameter is the received data as string.

#### Returns
`nil`

#### Example
```lua
srv = net.createConnection(net.TCP, 0)
srv:on("receive", function(sck, c) print(c) end)
srv:connect(80,"192.168.0.66")
srv:on("connection", function(sck, c)
  -- Wait for connection before sending.
  sck:send("GET / HTTP/1.1\r\nHost: 192.168.0.66\r\nConnection: keep-alive\r\nAccept: */*\r\n\r\n")
end)
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
srv:listen(80, function(conn)
  conn:on("receive", function(sck, req)
    local response = {}

    -- if you're sending back HTML over HTTP you'll want something like this instead
    -- local response = {"HTTP/1.0 200 OK\r\nServer: NodeMCU on ESP8266\r\nContent-Type: text/html\r\n\r\n"}

    response[#response + 1] = "lots of data"
    response[#response + 1] = "even more data"
    response[#response + 1] = "e.g. content read from a file"
    
	 -- sends and removes the first element from the 'response' table
    local function send(sk)
      if #response > 0
        then sk:send(table.remove(response, 1))
      else
        sk:close()
        response = nil
      end
    end

    -- triggers the send() function again once the first chunk of data was sent
    sck:on("sent", send)
    
    send(sck)
  end)
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
`net.dns.resolve(host, function(ip))`

#### Parameters
- `host` hostname to resolve
- `function(sk, ip)` callback called when the name was resolved. Don't use `sk`, it's a socket used internally to resolve the hostname.

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

This controls certificate verification when SSL is in use. 

## net.cert.verify()

Controls the vertificate verification process when the Nodemcu makes a secure connection.

#### Syntax
`net.cert.verify(enable)`

`net.cert.verify(pemdata)`

#### Parameters
- `enable` A boolean which indicates whether verification should be enabled or not. The default at boot is `false`.
- `pemdata` A string containing the CA certificate to use for verification.

#### Returns
`true` if it worked. 

Can throw a number of errors if invalid data is supplied.

#### Example
Make a secure https connection and verify that the certificate chain is valid.
```
net.cert.verify(true)
http.get("https://example.com/info", nil, function (code, resp) print(code, resp) end)
```

Load a certificate into the flash chip and make a request. This is the [startssl](https://startssl.com) root certificate. They provide free
certificates.

```
net.cert.verify([[
-----BEGIN CERTIFICATE-----
MIIHyTCCBbGgAwIBAgIBATANBgkqhkiG9w0BAQUFADB9MQswCQYDVQQGEwJJTDEW
MBQGA1UEChMNU3RhcnRDb20gTHRkLjErMCkGA1UECxMiU2VjdXJlIERpZ2l0YWwg
Q2VydGlmaWNhdGUgU2lnbmluZzEpMCcGA1UEAxMgU3RhcnRDb20gQ2VydGlmaWNh
dGlvbiBBdXRob3JpdHkwHhcNMDYwOTE3MTk0NjM2WhcNMzYwOTE3MTk0NjM2WjB9
MQswCQYDVQQGEwJJTDEWMBQGA1UEChMNU3RhcnRDb20gTHRkLjErMCkGA1UECxMi
U2VjdXJlIERpZ2l0YWwgQ2VydGlmaWNhdGUgU2lnbmluZzEpMCcGA1UEAxMgU3Rh
cnRDb20gQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwggIiMA0GCSqGSIb3DQEBAQUA
A4ICDwAwggIKAoICAQDBiNsJvGxGfHiflXu1M5DycmLWwTYgIiRezul38kMKogZk
pMyONvg45iPwbm2xPN1yo4UcodM9tDMr0y+v/uqwQVlntsQGfQqedIXWeUyAN3rf
OQVSWff0G0ZDpNKFhdLDcfN1YjS6LIp/Ho/u7TTQEceWzVI9ujPW3U3eCztKS5/C
Ji/6tRYccjV3yjxd5srhJosaNnZcAdt0FCX+7bWgiA/deMotHweXMAEtcnn6RtYT
Kqi5pquDSR3l8u/d5AGOGAqPY1MWhWKpDhk6zLVmpsJrdAfkK+F2PrRt2PZE4XNi
HzvEvqBTViVsUQn3qqvKv3b9bZvzndu/PWa8DFaqr5hIlTpL36dYUNk4dalb6kMM
Av+Z6+hsTXBbKWWc3apdzK8BMewM69KN6Oqce+Zu9ydmDBpI125C4z/eIT574Q1w
+2OqqGwaVLRcJXrJosmLFqa7LH4XXgVNWG4SHQHuEhANxjJ/GP/89PrNbpHoNkm+
Gkhpi8KWTRoSsmkXwQqQ1vp5Iki/untp+HDH+no32NgN0nZPV/+Qt+OR0t3vwmC3
Zzrd/qqc8NSLf3Iizsafl7b4r4qgEKjZ+xjGtrVcUjyJthkqcwEKDwOzEmDyei+B
26Nu/yYwl/WL3YlXtq09s68rxbd2AvCl1iuahhQqcvbjM4xdCUsT37uMdBNSSwID
AQABo4ICUjCCAk4wDAYDVR0TBAUwAwEB/zALBgNVHQ8EBAMCAa4wHQYDVR0OBBYE
FE4L7xqkQFulF2mHMMo0aEPQQa7yMGQGA1UdHwRdMFswLKAqoCiGJmh0dHA6Ly9j
ZXJ0LnN0YXJ0Y29tLm9yZy9zZnNjYS1jcmwuY3JsMCugKaAnhiVodHRwOi8vY3Js
LnN0YXJ0Y29tLm9yZy9zZnNjYS1jcmwuY3JsMIIBXQYDVR0gBIIBVDCCAVAwggFM
BgsrBgEEAYG1NwEBATCCATswLwYIKwYBBQUHAgEWI2h0dHA6Ly9jZXJ0LnN0YXJ0
Y29tLm9yZy9wb2xpY3kucGRmMDUGCCsGAQUFBwIBFilodHRwOi8vY2VydC5zdGFy
dGNvbS5vcmcvaW50ZXJtZWRpYXRlLnBkZjCB0AYIKwYBBQUHAgIwgcMwJxYgU3Rh
cnQgQ29tbWVyY2lhbCAoU3RhcnRDb20pIEx0ZC4wAwIBARqBl0xpbWl0ZWQgTGlh
YmlsaXR5LCByZWFkIHRoZSBzZWN0aW9uICpMZWdhbCBMaW1pdGF0aW9ucyogb2Yg
dGhlIFN0YXJ0Q29tIENlcnRpZmljYXRpb24gQXV0aG9yaXR5IFBvbGljeSBhdmFp
bGFibGUgYXQgaHR0cDovL2NlcnQuc3RhcnRjb20ub3JnL3BvbGljeS5wZGYwEQYJ
YIZIAYb4QgEBBAQDAgAHMDgGCWCGSAGG+EIBDQQrFilTdGFydENvbSBGcmVlIFNT
TCBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0eTANBgkqhkiG9w0BAQUFAAOCAgEAFmyZ
9GYMNPXQhV59CuzaEE44HF7fpiUFS5Eyweg78T3dRAlbB0mKKctmArexmvclmAk8
jhvh3TaHK0u7aNM5Zj2gJsfyOZEdUauCe37Vzlrk4gNXcGmXCPleWKYK34wGmkUW
FjgKXlf2Ysd6AgXmvB618p70qSmD+LIU424oh0TDkBreOKk8rENNZEXO3SipXPJz
ewT4F+irsfMuXGRuczE6Eri8sxHkfY+BUZo7jYn0TZNmezwD7dOaHZrzZVD1oNB1
ny+v8OqCQ5j4aZyJecRDjkZy42Q2Eq/3JR44iZB3fsNrarnDy0RLrHiQi+fHLB5L
EUTINFInzQpdn4XBidUaePKVEFMy3YCEZnXZtWgo+2EuvoSoOMCZEoalHmdkrQYu
L6lwhceWD3yJZfWOQ1QOq92lgDmUYMA0yZZwLKMS9R9Ie70cfmu3nZD0Ijuu+Pwq
yvqCUqDvr0tVk+vBtfAii6w0TiYiBKGHLHVKt+V9E9e4DGTANtLJL4YSjCMJwRuC
O3NJo2pXh5Tl1njFmUNj403gdy3hZZlyaQQaRwnmDwFWJPsfvw55qVguucQJAX6V
um0ABj6y6koQOdjQK/W/7HW/lwLFCRsI3FU34oH7N4RDYiDK51ZLZer+bMEkkySh
NOsF/5oirpt9P/FlUQqmMGqz9IgcgA38corog14=
-----END CERTIFICATE-----
]])

http.get("https://pskreporter.info/gen404", nil, function (code, resp) print(code, resp) end)
```


#### Notes
The certificate needed for verification is stored in the flash chip. The `net.cert.verify` call with `true`
enables verification against the value stored in the flash. 

The certificate can be loaded into the flash chip in two ways -- one at firmware build time, and the other at initial boot
of the firmware. In order to load the certificate at build time, just place a file containing the CA certificate (in PEM format) 
at `server-ca.crt` in the root of the nodemcu-firmware build tree. The build scripts will incorporate this into the resulting
firmware image.

The alternative approach is easier for development, and that is to supply the PEM data as a string value to `net.cert.verify`. This
will store the certificate into the flash chip and turn on verification for that certificate. Subsequent boots of the nodemcu can then
use `net.cert.verify(true)` and use the stored certificate.


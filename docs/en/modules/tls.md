# TLS Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-12-15 | [PhoeniX](https://github.com/djphoenix) | [PhoeniX](https://github.com/djphoenix) | [tls.c](../../../app/modules/tls.c)|

**SSL/TLS support**

!!! attention
    The TLS module depends on the [net](net.md) module, it is a required dependency.

NodeMCU includes the open-source version of [mbed TLS library](https://tls.mbed.org/). With the NodeMCU default configuration it supports **TLS** 1.0 / 1.1 / 1.2 and the most common cipher suites, including DH/ECDH. ECDSA-based cipher suites are disabled by default.

!!! tip
	The complete configuration is stored in [user_mbedtls.h](../../../app/include/user_mbedtls.h). This is the file to edit if you build your own firmware and want to change mbed TLS behavior.

For a list of features have a look at the [mbed TLS features page](https://tls.mbed.org/core-features).

This module handles certificate verification when SSL/TLS is in use.

## tls.createConnection()

Creates TLS connection.

#### Syntax
`tls.createConnection()`

#### Parameters
none

#### Returns
tls.socket sub module

#### Example

```lua
tls.createConnection()
```

# tls.socket Module

## tls.socket:close()

Closes socket.

#### Syntax
`close()`

#### Parameters
none

#### Returns
`nil`

#### See also
[`tls.createConnection()`](#tlscreateconnection)

## tls.socket:connect()

Connect to a remote server.

#### Syntax
`connect(port, ip|domain)`

#### Parameters
- `port` port number
- `ip` IP address or domain name string

#### Returns
`nil`

#### See also
[`tls.socket:on()`](#tlssocketon)

## tls.socket:dns()

Provides DNS resolution for a hostname.

#### Syntax
`dns(domain, function(tls.socket, ip))`

#### Parameters
- `domain` domain name
- `function(tls.socket, ip)` callback function. The first parameter is the socket, the second parameter is the IP address as a string.

#### Returns
`nil`

#### Example
```lua
sk = tls.createConnection()
sk:dns("google.com", function(conn, ip) print(ip) end)
sk = nil
```

## tls.socket:getpeer()

Retrieve port and ip of peer.

#### Syntax
`getpeer()`

#### Parameters
none

#### Returns
- `ip` of peer
- `port` of peer

## tls.socket:hold()

Throttle data reception by placing a request to block the TCP receive function. This request is not effective immediately, Espressif recommends to call it while reserving 5*1460 bytes of memory.

#### Syntax
`hold()`

#### Parameters
none

#### Returns
`nil`

#### See also
[`tls.socket:unhold()`](#tlssocketunhold)

## tls.socket:on()

Register callback functions for specific events.

#### Syntax
`on(event, function())`

#### Parameters
- `event` string, which can be "connection", "reconnection", "disconnection", "receive" or "sent"
- `function(tls.socket[, string])` callback function. The first parameter is the socket.
If event is "receive", the second parameter is the received data as string.
If event is "reconnection", the second parameter is the reason of connection error (string).

#### Returns
`nil`

#### Example
```lua
srv = tls.createConnection()
srv:on("receive", function(sck, c) print(c) end)
srv:on("connection", function(sck, c)
  -- Wait for connection before sending.
  sck:send("GET / HTTP/1.1\r\nHost: google.com\r\nConnection: keep-alive\r\nAccept: */*\r\n\r\n")
end)
srv:connect(443,"google.com")
```
!!! note
    The `receive` event is fired for every network frame! See details at [net.socket:on()](net.md#netsocketon).

#### See also
- [`tls.createConnection()`](#tlscreateconnection)
- [`tls.socket:hold()`](#tlssockethold)

## tls.socket:send()

Sends data to remote peer.

#### Syntax
`send(string)`

#### Parameters
- `string` data in string which will be sent to server

#### Returns
`nil`

#### Note

Multiple consecutive `send()` calls aren't guaranteed to work (and often don't) as network requests are treated as separate tasks by the SDK. Instead, subscribe to the "sent" event on the socket and send additional data (or close) in that callback. See [#730](https://github.com/nodemcu/nodemcu-firmware/issues/730#issuecomment-154241161) for details.

#### See also
[`tls.socket:on()`](#tlssocketon)

## tls.socket:unhold()

Unblock TCP receiving data by revocation of a preceding `hold()`.

#### Syntax
`unhold()`

#### Parameters
none

#### Returns
`nil`

#### See also
[`tls.socket:hold()`](#tlssockethold)








# tls.cert Module

## tls.cert.verify()

Controls the vertificate verification process when the Nodemcu makes a secure connection.

#### Syntax
`tls.cert.verify(enable)`

`tls.cert.verify(pemdata)`

#### Parameters
- `enable` A boolean which indicates whether verification should be enabled or not. The default at boot is `false`.
- `pemdata` A string containing the CA certificate to use for verification.

#### Returns
`true` if it worked. 

Can throw a number of errors if invalid data is supplied.

#### Example
Make a secure https connection and verify that the certificate chain is valid.
```
tls.cert.verify(true)
http.get("https://example.com/info", nil, function (code, resp) print(code, resp) end)
```

Load a certificate into the flash chip and make a request. This is the [startssl](https://startssl.com) root certificate. They provide free
certificates.

```
tls.cert.verify([[
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
The certificate needed for verification is stored in the flash chip. The `tls.cert.verify` call with `true`
enables verification against the value stored in the flash. 

The certificate can be loaded into the flash chip in two ways -- one at firmware build time, and the other at initial boot
of the firmware. In order to load the certificate at build time, just place a file containing the CA certificate (in PEM format) 
at `server-ca.crt` in the root of the nodemcu-firmware build tree. The build scripts will incorporate this into the resulting
firmware image.

The alternative approach is easier for development, and that is to supply the PEM data as a string value to `tls.cert.verify`. This
will store the certificate into the flash chip and turn on verification for that certificate. Subsequent boots of the nodemcu can then
use `tls.cert.verify(true)` and use the stored certificate.

# tls.setDebug function

mbedTLS can be compiled with debug support.  If so, the tls.setDebug
function is mapped to the `mbedtls_debug_set_threshold` function and
can be used to enable or disable debugging spew to the console.
See mbedTLS's documentation for more details.

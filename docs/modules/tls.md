# TLS Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-12-15 | [PhoeniX](https://github.com/djphoenix) | [PhoeniX](https://github.com/djphoenix) | [tls.c](../../app/modules/tls.c)|

**SSL/TLS support**

!!! attention
    The TLS module depends on the [net](net.md) module, it is a required dependency.

NodeMCU includes the open-source version of [mbed TLS library](https://tls.mbed.org/).

With the NodeMCU default configuration it supports **TLS** 1.1 and 1.2 with
most common features supported.  Specifically, it provides:

- ciphers: AES, Camellia
- chaining modes: CBC, CFB, CTR, GCM
- digest algorithms: RIPEMD-160, SHA1, SHA2
- signature algorithms: RSA, deterministic ECDSA
- key exchange algorithms: DHE and ECDHE
- elliptic curves: secp{256,384}r1, secp256k1, bp{256,384}.

!!! tip
	If possible, you will likely be much better served by using the ECDSA
	signature and key exchange algorithms than by using RSA.  An
	increasingly large fraction of the Internet understands ECDSA, and most
	server software can speak it as well.  The much smaller key size (at
	equivalent security!) is beneficial for NodeMCU's limited RAM.

	https://wiki.openssl.org/index.php/Command_Line_Elliptic_Curve_Operations
	details how to create ECDSA keys and certificates.

!!! tip
	The complete configuration is stored in [user_mbedtls.h](../../app/include/user_mbedtls.h). This is the file to edit if you build your own firmware and want to change mbed TLS behavior.

!!! warning
	The TLS glue provided by Espressif provides no interface to TLS SNI.
	As such, NodeMCU TLS should not be expected to function with endpoints
	requiring the use of SNI, which is a growing fraction of the Internet
	and includes, for example, Cloudflare sites using their "universal SSL"
	service and other, similar "virtual" TLS servers.  TLS servers to which
	you wish NodeMCU to connect should have their own, dedicated IP/port
	pair.

!!! warning
	The TLS handshake is very heap intensive, requiring between 25 and 30
	**kilobytes** of heap.  Some, but not all, of that is made available
	again once the handshake has completed and the connection is open.
	Because of this, we have disabled mbedTLS's support for connection
	renegotiation.  You may find it necessary to restructure your
	application so that connections happen early in boot when heap is
	relatively plentiful, with connection failures inducing reboots.

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
- `event` string, which can be "dns", "connection", "reconnection", "disconnection", "receive" or "sent"
- `function(tls.socket[, string])` callback function. The first parameter is the socket.
If event is "receive", the second parameter is the received data as string.
If event is "reconnection", the second parameter is the reason of connection error (string).
If event is "dns", the second parameter will be either `nil` or a string rendering of the resolved address.

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

Load a certificate into the flash chip and make a request. This is the
[IdenTrust](https://www.identrust.co.uk/) `DST Root CA X3` certificate; it is
used, for example, by [letsencrypt](https://letsencrypt.org), for their
intermediate X3 authority; letsencrypt is one option for obtaining your own SSL
certificates free of cost.

```
tls.cert.verify([[
-----BEGIN CERTIFICATE-----
MIIDSjCCAjKgAwIBAgIQRK+wgNajJ7qJMDmGLvhAazANBgkqhkiG9w0BAQUFADA/
MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT
DkRTVCBSb290IENBIFgzMB4XDTAwMDkzMDIxMTIxOVoXDTIxMDkzMDE0MDExNVow
PzEkMCIGA1UEChMbRGlnaXRhbCBTaWduYXR1cmUgVHJ1c3QgQ28uMRcwFQYDVQQD
Ew5EU1QgUm9vdCBDQSBYMzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB
AN+v6ZdQCINXtMxiZfaQguzH0yxrMMpb7NnDfcdAwRgUi+DoM3ZJKuM/IUmTrE4O
rz5Iy2Xu/NMhD2XSKtkyj4zl93ewEnu1lcCJo6m67XMuegwGMoOifooUMM0RoOEq
OLl5CjH9UL2AZd+3UWODyOKIYepLYYHsUmu5ouJLGiifSKOeDNoJjj4XLh7dIN9b
xiqKqy69cK3FCxolkHRyxXtqqzTWMIn/5WgTe1QLyNau7Fqckh49ZLOMxt+/yUFw
7BZy1SbsOFU5Q9D8/RhcQPGX69Wam40dutolucbY38EVAjqr2m7xPi71XAicPNaD
aeQQmxkqtilX4+U9m5/wAl0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNV
HQ8BAf8EBAMCAQYwHQYDVR0OBBYEFMSnsaR7LHH62+FLkHX/xBVghYkQMA0GCSqG
SIb3DQEBBQUAA4IBAQCjGiybFwBcqR7uKGY3Or+Dxz9LwwmglSBd49lZRNI+DT69
ikugdB/OEIKcdBodfpga3csTS7MgROSR6cz8faXbauX+5v3gTt23ADq1cEmv8uXr
AvHRAosZy5Q6XkjEGB5YGV8eAlrwDPGxrancWYaLbumR9YbK+rlmM6pZW87ipxZz
R8srzJmwN0jP41ZL9c8PDHIyh8bwRLtTcm1D9SZImlJnt1ir/md2cXjbDaJWFBM5
JDGFoqgCWjBH4d1QB7wCCZAA62RjYJsWvIjJEubSfZGL+T0yjWW06XyxV3bqxbYo
Ob8VZRzI9neWagqNdwvYkQsEjgfbKbYK7p2CNTUQ
-----END CERTIFICATE-----
]])

http.get("https://letsencrypt.org/", nil, function (code, resp) print(code, resp) end)
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

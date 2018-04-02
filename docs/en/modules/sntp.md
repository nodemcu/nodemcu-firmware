# SNTP Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-06-30 | [DiUS](https://github.com/DiUS), [Johny Mattsson](https://github.com/jmattsson) | [Johny Mattsson](https://github.com/jmattsson) | [sntp.c](../../../app/modules/sntp.c)|

The SNTP module implements a [Simple Network Time Procotol](https://en.wikipedia.org/wiki/Network_Time_Protocol#SNTP) client. This includes support for the "anycast" [NTP](https://en.wikipedia.org/wiki/Network_Time_Protocol) mode where, if supported by the NTP server(s) in your network, it is not necessary to even know the IP address of the NTP server.
By default, this will use the servers 0.nodemcu.pool.ntp.org through 3.nodemcu.pool.ntp.org. These servers will be adequate for nearly all usages.

When compiled together with the [rtctime](rtctime.md) module it also offers seamless integration with it, potentially reducing the process of obtaining NTP synchronization to a simple `sntp.sync()` call without any arguments.


## sntp.sync()

Attempts to obtain time synchronization. 

For best results you may want to to call this periodically in order to compensate for internal clock drift. As stated in the [rtctime](rtctime.md) module documentation it's advisable to sync time after deep sleep and it's necessary to sync after module reset (add it to [`init.lua`](../upload.md#initlua) after WiFi initialization).
Note that either a single server can be provided as an argument (name or address), or a list (table) of servers can be provided. 

If *all* of the supplied host names/addresses are invalid, then the error callback will be called with argument type 1. Otherwise, if
there is at least one valid name/address, then then sync will be performed.

If any sync operation fails (maybe the device is disconnected from the internet), then all the names will be looked up again. 

#### Syntax
`sntp.sync([server_ip], [callback], [errcallback], [autorepeat])`
`sntp.sync({ server1, server2, .. }, [callback], [errcallback], [autorepeat])`

#### Parameters
- `server_ip` if non-`nil`, that server is used. If `nil`, then the last contacted server is used. If there is no previous server, then the pool ntp servers are used. If the anycast server was used, then the first responding server will be saved. 
- `server1`, `server2` these are either the ip address or dns name of one or more servers to try.
- `callback` if provided it will be invoked on a successful synchronization, with four parameters: seconds, microseconds, server and info. Note that when the [rtctime](rtctime.md) module is available, there is no need to explicitly call [`rtctime.set()`](rtctime.md#rtctimeset) - this module takes care of doing so internally automatically, for best accuracy. The info parameter is a table of (semi) interesting values. These are described below.
- `errcallback` failure callback with two parameters. The first is an integer describing the type of error. The module automatically performs a number of retries before giving up and reporting the error. The second is a string containing supplementary information (if any). Error codes:
  - 1: DNS lookup failed (the second parameter is the failing DNS name)
  - 2: Memory allocation failure
  - 3: UDP send failed
  - 4: Timeout, no NTP response received
- `autorepeat` if this is non-nil, then the synchronization will happen every 1000 seconds and try and condition the clock if possible. The callbacks will be called after each sync operation.

#### Returns
`nil`

#### Info table
This is passed to the success callback and contains useful information about the time synch that just completed. The keys in this table are:

- `offset_s` This is an optional field and contains the number of seconds that the clock was adjusted. This is only present for large (many second) adjustments. Typically, this is only present on the initial sync call.
- `offset_us` This is an optional field (but one of `offset_s` and `offset_us` will always be present). This contains the number of microseconds that the clock was adjusted. 
- `delay_us` This is the round trip delay to the server in microseconds. Thie setting uncertainty is somewhat less than this value.
- `stratum` This is the stratum of the server. 
- `leap` This contains the leap bits from the NTP protocol. 0 means that no leap second is pending, 1 is a pending extra leap second at the end of the UTC month, and 2 is a pending leap second removal at the end of the UTC month.

#### Example
```lua
-- Use the nodemcu specific pool servers and keep the time synced forever (this has the autorepeat flag set).
sntp.sync(nil, nil, nil, 1)
```
```lua
-- Single shot sync time with a server on the local network.
sntp.sync("224.0.1.1",
  function(sec, usec, server, info)
    print('sync', sec, usec, server)
  end,
  function()
   print('failed!')
  end
)
```
#### See also
[`rtctime.set()`](rtctime.md#rtctimeset)

## sntp.setoffset

Sets the offset between the rtc clock and the NTP time. Note that NTP time has leap seconds in it and hence it runs slow when a leap second is 
inserted. The `setoffset` call enables explicit leap second tracking and causes the rtc clock to tick more evenly -- but it gets out of step
with wall clock time. The number of seconds is the offset.

#### Syntax
`sntp.setoffset([offset])`

#### Parameters
- `offset` The offset between NTP time and the rtc time. This can be omitted, and defaults to zero. This call enables the offset tracking.

#### Returns
nil

## sntp.getoffset

Gets the offset between the rtc clock and the NTP time. This value should be subtracted from the rtc time to get the NTP time -- which
corresponds to wall clock time. If the offset returned has changed from the pervious call, then there has been a leap second inbetween.

#### Syntax
`offset = sntp.getoffset()`

#### Returns
The current offset.

# SNTP Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-06-30 | [DiUS](https://github.com/DiUS), [Johny Mattsson](https://github.com/jmattsson) | [Johny Mattsson](https://github.com/jmattsson) | [sntp.c](../../../app/modules/sntp.c)|

The SNTP module implements a [Simple Network Time Procotol](https://en.wikipedia.org/wiki/Network_Time_Protocol#SNTP) client. This includes support for the "anycast" [NTP](https://en.wikipedia.org/wiki/Network_Time_Protocol) mode where, if supported by the NTP server(s) in your network, it is not necessary to even know the IP address of the NTP server.

When compiled together with the [rtctime](rtctime.md) module it also offers seamless integration with it, potentially reducing the process of obtaining NTP synchronization to a simple `sntp.sync()` call without any arguments.


## sntp.sync()

Attempts to obtain time synchronization. 

For best results you may want to to call this periodically in order to compensate for internal clock drift. As stated in the [rtctime](rtctime.md) module documentation it's advisable to sync time after deep sleep and it's necessary to sync after module reset (add it to [`init.lua`](upload.md#initlua) after WiFi initialization).

#### Syntax
`sntp.sync([server_ip], [callback], [errcallback])`

#### Parameters
- `server_ip` if non-`nil`, that server is used. If `nil`, then the last contacted server is used. This ties in with the NTP anycast mode, where the first responding server is remembered for future synchronization requests. The easiest way to use anycast is to always pass nil for the server argument.
- `callback` if provided it will be invoked on a successful synchronization, with three parameters: seconds, microseconds, and server. Note that when the [rtctime](rtctime.md) module is available, there is no need to explicitly call [`rtctime.set()`](rtctime.md#rtctimeset) - this module takes care of doing so internally automatically, for best accuracy.
- `errcallback` failure callback with a single integer parameter describing the type of error. The module automatically performs a number of retries before giving up and reporting the error. Error codes:
  - 1: DNS lookup failed
  - 2: Memory allocation failure
  - 3: UDP send failed
  - 4: Timeout, no NTP response received

#### Returns
`nil`

#### Example
```lua
-- Best effort, use the last known NTP server (or the NTP "anycast" address 224.0.1.1 initially)
sntp.sync()
```
```lua
-- Sync time with 192.168.0.1 and print the result, or that it failed
sntp.sync('192.168.0.1',
  function(sec,usec,server)
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
nothing.

## sntp.getoffset

Gets the offset between the rtc clock and the NTP time. This value should be subtracted from the rtc time to get the NTP time -- which
corresponds to wall clock time. If the offset returned has changed from the pervious call, then there has been a leap second inbetween.

#### Syntax
`offset = sntp.getoffset()`

#### Returns
The current offset.

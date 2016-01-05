# sntp Module

The SNTP module implements a Simple Network Time Procotol client. This includes support for the "anycast" NTP mode where, if supported by the NTP server(s) in your network, it is not necessary to even know the IP address of the NTP server.

When compiled together with the rtctime module it also offers seamless integration with it, potentially reducing the process of obtaining NTP synchronization to a simple `sntp.sync()` call without any arguments.

####See also
  - rtctime module

## sntp.sync()

Attempts to obtain time synchronization.

####Syntax
`sntp.sync([server_ip], [callback], [errcallback])`

####Parameters
  - `server_ip`: If the `server_ip` argument is non-nil, that server is used. If nil, then the last contacted server is used. This ties in with the NTP anycast mode, where the first responding server is remembered for future synchronization requests. The easiest way to use anycast is to always pass nil for the server argument.
  - `callback`: If the `callback` argument is provided it will be invoked on a successful synchronization, with three parameters: seconds, microseconds, and server. Note that when the rtctime module is available, there is no need to explicitly call `rtctime.set()` - this module takes care of doing so internally automatically, for best accuracy.
  - `errcallback`: On failure, the `errcallback` will be invoked, if provided. The sntp module automatically performs a number of retries before giving up and reporting the error. This callback receives no parameters.

####Returns
`nil`

####Example
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
####See also
  - `rtctime.set()`
___

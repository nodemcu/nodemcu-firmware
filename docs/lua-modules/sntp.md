# SNTP Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2019-07-01 | [nwf](https://github.com/nwf) | [nwf](https://github.com/nwf) | [sntp.lua](../../lua_modules/sntp/sntp.lua) |

This is a user-friendly, Lua wrapper around the `sntppkt` module to facilitate
the use of SNTP.

!!! note

       This `sntp` module is expected to live in LFS, and so all the documentation
       uses `node.flashindex()` to find the module.  If the module is instead
       loaded as a file, use, for example, `(require "sntp").go()` instead, but
       note that this may consume a large amount of heap.

## Default Constructor Wrapper

The simplest use case is to use the `go` function with no arguments like this:

```lua
node.flashindex("sntp")().go()
```

This will...

  * use `rtctime` to interface with the local clock;

  * use a default list of servers, including the server provided by the local
    DHCP server, if any;

  * re-synchronize the clock every half-hour in the steady state;

  * attempt to discipline the local oscillator to improve timing accuracy (but
    note that this may take a while to converge; advanced users may wish to
    manually persist drift rate across reboots using the `rtctime` interface);
    and

  * rapidly re-discipline the local oscillator when a large deviance is
    observed (e.g., at cold boot)

The sntp object encapsulating the state machine is returned, but this may be
safely ignored in simple use cases.  (This object is augmented with `tmr`, the
timer used to manage resychronization, should you wish to dynamically start,
stop, or delay synchronization.)

You can provide your own list of servers, too, overriding the default:

```lua
node.flashindex("sntp")().go({ "ntp-server-1", "ntp-server-2" })
```

Or, to just use the server provided by DHCP and no others, when one is certain
that the DHCP server will provide one,

```lua
do
  local ifi = net.ifinfo(0)
  local srv = ifi and ifi.dhcp and ifi.dhcp.ntp_server
  if srv then node.flashindex("sntp")().go({srv}) end
end
```

Similarly, the frequency of synchronization can be changed:

```lua
-- use default servers and synchronize every ten minutes
node.flashindex("sntp")().go(nil, 600000)
```

Success and failure callbacks can be given as well, for advanced use or
increased reporting:

```lua
node.flashindex("sntp")().go(nil, 1200000,
  function(res, serv, self)
    print("SNTP OK", serv, res.theta_s, res.theta_us, rtctime.get())
  end,
  function(err, srv, rply)
    if     err == "all"    then print("SNTP FAIL", srv)
    elseif err == "kod"    then print("SNTP server kiss of death", srv, rply)
    elseif err == "goaway" then print("SNTP server rejected us", srv, rply)
    else                        print("SNTP server unreachable", srv, err)
    end
  end)
```

Details of the parameters to the callbacks are given below.  The remainder of
this document details increasingly internal details, and is likely of
decreasing interest to general audiences.

#### Syntax
`node.flashindex("sntp")().go([servers, [frequency, [success_cb, [failure_cb]]]])`

### Notes

#### Interaction with the Garbage Collector

As our network stack does not capture the time of received packets (nor does
it know how to timestamp egressing NTP packets as dedicated hardware does),
there is a fair bit of local processing delay, as we have to come into Lua
to get the local timestamps.  For higher-precision time keeping, if
possible, it may help to move the device to a (E)GC mode which has more slop
than the default, which prioritizes prompt reclaim of memory.  Consider, for
example, something like `node.egc.setmode(node.egc.ON_MEM_LIMIT, -4096)` to
permit the SNTP logic to run (more often) without interference of the GC.
(The `go` function above defaults to collecting garbage before triggering
SNTP synchronization.)

## SNTP Object Constructor
```lua
sntp = node.flashindex("sntp")().new(servers, success_cb, [failure_cb], [clock])
```

where

* `servers` specifies the name(s) of the (S)NTP server(s) to use; it may be...

    * a string, either a DNS name or an IPv4 address in dotted quad form,
    * an array of the above
    * `nil` to use any DHCP-provided NTP server and some default
      `*.nodemcu.pool.ntp.org` servers.

* `success_cb` is called back at the end of a synchronization when at least one
  server replied to us.  It will be given three arguments:

    * the preferred SNTP result converted to a a table; see `sntppkt.res.totable` below.
    * the name of the server whence that result came
    * the `sntp` object itself

* `failure_cb` may be `nil` but, otherwise, is called back in two circumstances:

    * at the end of a pass during which no server could be reached.  In this case,
      the first argument will be the string "all", the second will be the
      number of servers tried, and the third will be the `sntp` object itself.

    * an individual server has failed in some way.  In this case, the first
      argument will be one of:

        * "dns" (if name resolution failed),
        * "timeout" (if the server failed to reply in time),
        * "goaway" (if the server refused to answer), or
        * "kod" ("kiss of death", if the server told us to stop contacting it entirely).

      In all cases, the name of the server is the second argument and the `sntp`
      object itself is the third; in the "goaway" case, the fourth argument will
      contain the refusal string (e.g., "RATE" for rate-limiting or "DENY" for
      kiss-of-death warnings.

      In the case of kiss-of-death packets, the server will be removed from all
      subsequent syncs.  This may result in there eventually being no servers to
      contact.  Paranoid applications should therefore monitor failures!

* `clock`, if given, should return two values describing the local clock in
  seconds and microseconds (between 0 and 1000000).  If not given, the module
  will fall back on `rtctime.get`; if `rtctime` is not available, a clock must
  be provided.  Using `function() return 0, 0 end` will result in the "clock
  offset" (`theta`) reported by the success callback being a direct estimate of
  the true time.

## Other module methods

The module contains some other utility functions beyond the SNTP object
constructor and the `go` utility function detailed above.

### update_rtc()
Given a result from a SNTP `sync` pass, update the local RTC through `rtctime`.
Attempting to use this function without `rtctime` support will raise an error.

`sntpobj` is used to track state between syncs and should be the "sntp object"
given in the success and failure callbacks.  (In principle, any Lua table will
do, but that is the most convenient one.  All data is passed using fields whose
keys are strings with prefix `rtc_`.)

#### Syntax
`node.flashindex("sntp")().update_rtc(res, sntpobj)`

## SNTP object methods

### sync()

Run a pass through the specified servers and call back as described above.

#### Syntax
`sntpobj:sync()`

### stop()

Abort any pass in progress; no more callbacks will be called.  The current
preferred response and server name (i.e., the arguments to the success
callback, should the pass end now) are returned.

#### Syntax
`sntpobj:stop()`

### servers
The table of NTP servers currently being used.  Please treat this as
read-only.  This may be investigated to see if kiss-of-death processing
has removed any servers, but one is probably better off listening for
the failure callback notifications.

## Internal Details: sntppkt response object methods

### sntppkt.resp.totable()

Expose a `sntppkt.resp` result as a Lua table with the following fields:

* `theta_s`: Local clock offset, seconds component
* `theta_us`: Local clock offset, microseconds component

* `delta`: An estimate of the error, in 65536ths of a second (i.e., approx 15.3 microseconds)
* `delta_r`: The server's estimate of its error, in 65536ths of a second
* `epsilon_r`: The server's estimate of its dispersion, in 65536ths of a second

* `leapind`: The leap-second indicator
* `stratum`: The server's stratum

* `rx_s`: Packet reception timestamp, seconds component
* `rx_us`: Packet reception timestamp, microseconds component

* `raw`: The `sntppkt.resp` itself, so that we can pass this table around to
  user Lua code and still retain access to the raw internals in, for example,
  `drift_compensate`, below.  See the use in `update_rtc`.

Note that negative offsets will be represented with a negative `theta_s` and
a *positive* `theta_us`.  For example, -200 microseconds would be -1 seconds
and 999800 microseconds.

#### Syntax
`res:totable()`

### sntppkt.resp.pick()

Used internally to select among multiple responses; see source for usage.

### sntppkt.resp.drift_compensate()

Encapsulates a Proportional-Integral (PI) controller update step for use in
disciplining the local oscillator.  Used internally to `update_rtc`; see source
for usage.

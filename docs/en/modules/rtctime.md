# RTC Time Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-06-25 | [DiUS](https://github.com/DiUS), [Johny Mattsson](https://github.com/jmattsson), Bernd Meyer <bmeyer@dius.com.au> | [Johny Mattsson](https://github.com/jmattsson) | [rtctime.c](../../../app/modules/rtctime.c)|

The rtctime module provides advanced timekeeping support for NodeMCU, including keeping time across deep sleep cycles (provided [`rtctime.dsleep()`](#rtctimedsleep) is used instead of [`node.dsleep()`](node.md#nodedsleep)). This can be used to significantly extend battery life on battery powered sensor nodes, as it is no longer necessary to fire up the RF module each wake-up in order to obtain an accurate timestamp.

This module is intended for use together with [NTP](https://en.wikipedia.org/wiki/Network_Time_Protocol) (Network Time Protocol) for keeping highly accurate real time at all times. Timestamps are available with microsecond precision, based on the Unix Epoch (1970/01/01 00:00:00). However, the accuracy is (in practice) no better then 1ms, and often worse than that. 

Time keeping on the ESP8266 is technically quite challenging. Despite being named [RTC](https://en.wikipedia.org/wiki/Real-time_clock), the RTC is not really a Real Time Clock in the normal sense of the word. While it does keep a counter ticking while the module is sleeping, the accuracy with which it does so is *highly* dependent on the temperature of the chip. Said temperature changes significantly between when the chip is running and when it is sleeping, meaning that any calibration performed while the chip is active becomes useless mere moments after the chip has gone to sleep. As such, calibration values need to be deduced across sleep cycles in order to enable accurate time keeping. This is one of the things this module does.

Further complicating the matter of time keeping is that the ESP8266 operates on three different clock frequencies - 52MHz right at boot, 80MHz during regular operation, and 160MHz if boosted. This module goes to considerable length to take all of this into account to properly keep the time.

To enable this module, it needs to be given a reference time at least once (via [`rtctime.set()`](#rtctimeset)). For best accuracy it is recommended to provide reference
times at regular intervals. The [`sntp.sync()`](sntp.md#sntpsync) function has an easy way to do this. It is important that a reference time is provided at least twice, with the second time being after a deep sleep.

Note that while the rtctime module can keep time across deep sleeps, it *will* lose the time if the module is unexpectedly reset.

This module can compensate for the underlying clock not running at exactly the required rate. The adjustment is in steps of 1 part in 2^32 (i.e. around 0.25 ppb). This adjustment
is done automatically if the [`sntp.sync()`](sntp.md#sntpsync) is called with the `autorepeat` flag set. The rate is settable using the [`set()`](#rtctimeset) function below. When the platform
is booted, it defaults to 0 (i.e. nominal). A sample of modules shows that the actual clock rate is temperature dependant, but is normally within 5ppm of the nominal rate. This translates to around 15 seconds per month. 

In the automatic update mode it can take a couple of hours for the clock rate to settle down to the correct value. After that, how well it tracks will depend on the amount
of variation in timestamps from the NTP servers. If they are close, then the time will track to within a millisecond or so. If they are further away (say 100ms round trip), then
time tracking is somewhat worse, but normally within 10ms. 

!!! important

	This module uses RTC memory slots 0-9, inclusive. As soon as [`rtctime.set()`](#rtctimeset) (or [`sntp.sync()`](sntp.md#sntpsync)) has been called these RTC memory slots will be used.

This is a companion module to the [rtcmem](rtcmem.md) and [SNTP](sntp.md) modules.

## rtctime.dsleep()

Puts the ESP8266 into deep sleep mode, like [`node.dsleep()`](node.md#nodedsleep). It differs from [`node.dsleep()`](node.md#nodedsleep) in the following ways:

- Time is kept across the deep sleep. I.e. [`rtctime.get()`](#rtctimeget) will keep working (provided time was available before the sleep).
- This call never returns. The module is put to sleep immediately. This is both to support accurate time keeping and to reduce power consumption.
- The time slept will generally be considerably more accurate than with [`node.dsleep()`](node.md#nodedsleep).
- A sleep time of zero does not mean indefinite sleep, it is interpreted as a zero length sleep instead.

When the sleep timer expires, the platform is rebooted and the Lua code is started with the `init.lua` file. The clock is set reasonably accurately.

#### Syntax
`rtctime.dsleep(microseconds [, option])`

#### Parameters
- `microseconds` number of microseconds to sleep for. Maxmium value is 4294967295us, or ~71 minutes.
- `option` sleep option, see [`node.dsleep()`](node.md#nodedsleep) for specifics.

#### Returns
This function does not return.

#### Example
```lua
-- sleep for a minute
rtctime.dsleep(60*1000000)
```
```lua
-- sleep for 5 seconds, do not start RF on wakeup
rtctime.dsleep(5000000, 4)
```

## rtctime.dsleep_aligned()

For applications where it is necessary to take samples with high regularity, this function is useful. It provides an easy way to implement a "wake up on the next 5-minute boundary" scheme, without having to explicitly take into account how long the module has been active for etc before going back to sleep.

#### Syntax
`rtctime.dsleep_aligned(aligned_us, minsleep_us [, option])`

#### Parameters
- `aligned_us` boundary interval in microseconds
- `minsleep_us` minimum time that will be slept, if necessary skipping an interval. This is intended for sensors where a sample reading is started before putting the ESP8266 to sleep, and then fetched upon wake-up. Here `minsleep_us` should be the minimum time required for the sensor to take the sample.
- `option` as with `dsleep()`, the `option` sets the sleep option, if specified.

#### Example
```lua
-- sleep at least 3 seconds, then wake up on the next 5-second boundary
rtctime.dsleep_aligned(5*1000000, 3*1000000)
```

## rtctime.epoch2cal()

Converts a Unix timestamp to calendar format. Neither timezone nor DST correction is performed - the result is UTC time.

#### Syntax
`rtctime.epoch2cal(timestamp)`

#### Parameters
`timestamp` seconds since Unix epoch

#### Returns
A table containing the fields:

- `year` 1970 ~ 2038
- `mon` month 1 ~ 12 in current year
- `day` day 1 ~ 31 in current month
- `hour`
- `min`
- `sec`
- `yday` day 1 ~ 366 in current year
- `wday` day 1 ~ 7 in current weak (Sunday is 1)

#### Example
```lua
tm = rtctime.epoch2cal(rtctime.get())
print(string.format("%04d/%02d/%02d %02d:%02d:%02d", tm["year"], tm["mon"], tm["day"], tm["hour"], tm["min"], tm["sec"]))
```

## rtctime.get()

Returns the current time. If current time is not available, zero is returned.

#### Syntax
`rtctime.get()`

#### Parameters
none

#### Returns
A three-value timestamp containing:

- `sec` seconds since the Unix epoch
- `usec` the microseconds part
- `rate` the current clock rate offset. This is an offset of `rate / 2^32` (where the nominal rate is 1). For example, a value of 4295 corresponds to 1 part per million.

#### Example
```lua
sec, usec, rate = rtctime.get()
```
#### See also
[`rtctime.set()`](#rtctimeset)

## rtctime.set()

Sets the rtctime to a given timestamp in the Unix epoch (i.e. seconds from midnight 1970/01/01). If the module is not already keeping time, it starts now. If the module was already keeping time, it uses this time to help adjust its internal calibration values. Care is taken that timestamps returned from [`rtctime.get()`](#rtctimeget) *never go backwards*. If necessary, time is slewed and gradually allowed to catch up.

It is highly recommended that the timestamp is obtained via NTP (see [SNTP module](sntp.md)), GPS, or other highly accurate time source.

Values very close to the epoch are not supported. This is a side effect of keeping the memory requirements as low as possible. Considering that it's no longer 1970, this is not considered a problem.

#### Syntax
`rtctime.set(seconds, microseconds, [rate])`

#### Parameters
- `seconds` the seconds part, counted from the Unix epoch
- `microseconds` the microseconds part
- `rate` the rate in the same units as for `rtctime.get()`. The stored rate is not modified if not specified.

#### Returns
`nil`

#### Example
```lua
-- Set time to 2015 July 9, 18:29:49
rtctime.set(1436430589, 0)
```
#### See also
[`sntp.sync()`](sntp.md#sntpsync)

## rtctime.adjust_delta()

This takes a time interval in 'system clock microseconds' based on the timestamps returned by `tmr.now` and returns 
an adjusted time interval in 'wall clock microseconds'. The size of the adjustment is typically pretty small as it (roughly) the error in the
crystal clock rate. This function is useful in some precision timing applications.

#### Syntax
`rtctime.adjust_delta(microseconds)`

#### Parameters
- `microseconds` a time interval measured in system clock microseconds.

#### Returns
The same interval but measured in wall clock microseconds

#### Example
```lua
local start = tmr.now()
-- do something
local end = tmr.now()
print ('Duration', rtctime.adjust_delta(end - start))

-- You can also go in the other direction (roughly)
local one_second = 1000000
local ticks_in_one_second = one_second - (rtctime.adjust_delta(one_second) - one_second)
```

# Timer Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-12 | [Zeroday](https://github.com/funshine) |              [dnc40085](https://github.com/dnc40085) | [tmr.c](../../../app/modules/tmr.c)|

The tmr module allows access to simple timers, the system counter and uptime.

It is aimed at setting up regularly occurring tasks, timing out operations, and provide low-resolution deltas.

What the tmr module is *not* however, is a time keeping module. While most timeouts are expressed in milliseconds or even microseconds, the accuracy is limited and compounding errors would lead to rather inaccurate time keeping. Consider using the [rtctime](rtctime.md) module for "wall clock" time.

NodeMCU provides 7 timers, numbered 0-6. It is currently up to the user to keep track of which timers are used for what.

## tmr.alarm()

This is a convenience function combining [`tmr.register()`](#tmrregister) and [`tmr.start()`](#tmrstart) into a single call.

To free up the resources with this timer when done using it, call [`tmr.unregister()`](#tmrunregister) on it. For one-shot timers this is not necessary, unless they were stopped before they expired.

#### Parameters
- `id` timer id (0-6)
- `interval_ms` timer interval in milliseconds. Maximum value is 6870947 (1:54:30.947).
- `mode` timer mode:
	- `tmr.ALARM_SINGLE` a one-shot alarm (and no need to call [`tmr.unregister()`](#tmrunregister))
	- `tmr.ALARM_SEMI` manually repeating alarm (call [`tmr.start()`](#tmrstart) to restart)
	- `tmr.ALARM_AUTO` automatically repeating alarm

#### Returns
`true` if the timer was started, `false` on error

#### Example
```lua
if not tmr.alarm(0, 5000, tmr.ALARM_SINGLE, function() print("hey there") end) then print("whoopsie") end
```
#### See also
- [`tmr.register()`](#tmrregister)
- [`tmr.start()`](#tmrstart)
- [`tmr.unregister()`](#tmrunregister)

## tmr.delay()

Busyloops the processor for a specified number of microseconds.

This is in general a **bad** idea, because nothing else gets to run, and the networking stack (and other things) can fall over as a result. The only time `tmr.delay()` may be appropriate to use is if dealing with a peripheral device which needs a (very) brief delay between commands, or similar. *Use with caution!*

Also note that the actual amount of time delayed for may be noticeably greater, both as a result of timing inaccuracies as well as interrupts which may run during this time.

#### Syntax
`tmr.delay(us)`

#### Parameters
`us` microseconds to busyloop for

#### Returns
`nil`

#### Example
```lua
tmr.delay(100)
```

## tmr.interval()

Changes a registered timer's expiry interval.

#### Syntax
`tmr.interval(id, interval_ms)`

#### Parameters
- `id` timer id (0-6)
- `interval_ms` new timer interval in milliseconds. Maximum value is 6870947 (1:54:30.947).

#### Returns
`nil`

#### Example
```lua
tmr.register(0, 5000, tmr.ALARM_SINGLE, function() print("hey there") end)
tmr.interval(0, 3000) -- actually, 3 seconds is better!
```

## tmr.now()

Returns the system counter, which counts in microseconds. Limited to 31 bits, after that it wraps around back to zero. That is essential if you use this function to [debounce or throttle GPIO input](https://github.com/hackhitchin/esp8266-co-uk/issues/2).

#### Syntax
`tmr.now()`

#### Parameters
none

#### Returns
the current value of the system counter

#### Example
```lua
print(tmr.now())
print(tmr.now())
```

## tmr.register()

Configures a timer and registers the callback function to call on expiry.

To free up the resources with this timer when done using it, call [`tmr.unregister()`](#tmrunregister) on it. For one-shot timers this is not necessary, unless they were stopped before they expired.

#### Syntax
`tmr.register(id, interval_ms, mode, func)`

#### Parameters
- `id` timer id (0-6)
- `interval_ms` timer interval in milliseconds. Maximum value is 6870947 (1:54:30.947).
- `mode` timer mode:
	- `tmr.ALARM_SINGLE` a one-shot alarm (and no need to call [`tmr.unregister()`](#tmrunregister))
	- `tmr.ALARM_SEMI` manually repeating alarm (call [`tmr.start()`](#tmrunregister) to restart)
	- `tmr.ALARM_AUTO` automatically repeating alarm

Note that registering does *not* start the alarm.

#### Returns
`nil`

#### Example
```lua
tmr.register(0, 5000, tmr.ALARM_SINGLE, function() print("hey there") end)
tmr.start(0)
```
#### See also
[`tmr.alarm()`](#tmralarm)

## tmr.softwd()

Provides a simple software watchdog, which needs to be re-armed or disabled before it expires, or the system will be restarted.

#### Syntax
`tmr.softwd(timeout_s)`

#### Parameters
`timeout_s` watchdog timeout, in seconds. To disable the watchdog, use -1 (or any other negative value).

#### Returns
`nil`

#### Example
```lua
function on_success_callback()
  tmr.softwd(-1)
  print("Complex task done, soft watchdog disabled!")
end

tmr.softwd(5)
-- go off and attempt to do whatever might need a restart to recover from
complex_stuff_which_might_never_call_the_callback(on_success_callback)
```

## tmr.start()

Starts or restarts a previously configured timer.

#### Syntax
`tmr.start(id)`

#### Parameters
`id` timer id (0-6)

#### Returns
`true` if the timer was started, `false` on error

#### Example
```lua
tmr.register(0, 5000, tmr.ALARM_SINGLE, function() print("hey there") end)
if not tmr.start(0) then print("uh oh") end
```
#### See also
- [`tmr.register()`](#tmrregister)
- [`tmr.stop()`](#tmrstop)
- [`tmr.unregister()`](#tmrunregister)

## tmr.state()

Checks the state of a timer.

#### Syntax
`tmr.state(id)`

#### Parameters
`id` timer id (0-6)

#### Returns
(bool, int) or `nil`

If the specified timer is registered, returns whether it is currently started and its mode. If the timer is not registered, `nil` is returned.

#### Example
```lua
running, mode = tmr.state(0)
```

## tmr.stop()

Stops a running timer, but does *not* unregister it. A stopped timer can be restarted with [`tmr.start()`](#tmrstart).

#### Syntax
`tmr.stop(id)`

#### Parameters
`id` timer id (0-6)

#### Returns
`true` if the timer was stopped, `false` on error

#### Example
```lua
if not tmr.stop(2) then print("timer 2 not stopped, not registered?") end
```
#### See also
- [`tmr.register()`](#tmrregister)
- [`tmr.stop()`](#tmrstop)
- [`tmr.unregister()`](#tmrunregister)

## tmr.time()

Returns the system uptime, in seconds. Limited to 31 bits, after that it wraps around back to zero.

#### Syntax
`tmr.time()`

#### Parameters
none

#### Returns
the system uptime, in seconds, possibly wrapped around

#### Example
```lua
print("Uptime (probably):", tmr.time())
```

## tmr.unregister()

Stops the timer (if running) and unregisters the associated callback.

This isn't necessary for one-shot timers (`tmr.ALARM_SINGLE`), as those automatically unregister themselves when fired.

#### Syntax
`tmr.unregister(id)`

#### Parameters
`id` timer id (0-6)

#### Returns
`nil`

#### Example
```lua
tmr.unregister(0)
```
#### See also
[`tmr.register()`](#tmrregister)

## tmr.wdclr()

Feed the system watchdog.

*In general, if you ever need to use this function, you are doing it wrong.*

The event-driven model of NodeMCU means that there is no need to be sitting in hard loops waiting for things to occur. Rather, simply use the callbacks to get notified when somethings happens. With this approach, there should never be a need to manually feed the system watchdog.

#### Syntax
`tmr.wdclr()`

#### Parameters
none

#### Returns
`nil`

# Timer Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-12 | [Zeroday](https://github.com/funshine) |              [dnc40085](https://github.com/dnc40085) | [tmr.c](../../../app/modules/tmr.c)|

The tmr module allows access to simple timers, the system counter and uptime.

It is aimed at setting up regularly occurring tasks, timing out operations, and provide low-resolution deltas.

What the tmr module is *not* however, is a time keeping module. While most timeouts are expressed in milliseconds or even microseconds, the accuracy is limited and compounding errors would lead to rather inaccurate time keeping. Consider using the [rtctime](rtctime.md) module for "wall clock" time.

NodeMCU provides 7 static timers, numbered 0-6, and dynamic timer creation function [`tmr.create()`](#tmrcreate).

!!! attention

    Static timers are deprecated and will be removed later. Use the OO API initiated with [`tmr.create()`](#tmrcreate).

## tmr.alarm()

This is a convenience function combining [`tmr.register()`](#tmrregister) and [`tmr.start()`](#tmrstart) into a single call.

To free up the resources with this timer when done using it, call [`tmr.unregister()`](#tmrunregister) on it. For one-shot timers this is not necessary, unless they were stopped before they expired.

#### Syntax
`tmr.alarm([id/ref], interval_ms, mode, func())`

#### Parameters
- `id`/`ref` timer id (0-6) or object, obsolete for OO API (→ [`tmr.create()`](#tmrcreate))
- `interval_ms` timer interval in milliseconds. Maximum value is 6870947 (1:54:30.947).
- `mode` timer mode:
	- `tmr.ALARM_SINGLE` a one-shot alarm (and no need to call [`tmr.unregister()`](#tmrunregister))
	- `tmr.ALARM_SEMI` manually repeating alarm (call [`tmr.start()`](#tmrstart) to restart)
	- `tmr.ALARM_AUTO` automatically repeating alarm
- `func(timer)` callback function which is invoked with the timer object as an argument

#### Returns
`true` if the timer was started, `false` on error

#### Example
```lua
if not tmr.create():alarm(5000, tmr.ALARM_SINGLE, function()
  print("hey there")
end)
then
  print("whoopsie")
end
```
#### See also
- [`tmr.create()`](#tmrcreate)
- [`tmr.register()`](#tmrregister)
- [`tmr.start()`](#tmrstart)
- [`tmr.unregister()`](#tmrunregister)

## tmr.create()

Creates a dynamic timer object.

Dynamic timer can be used instead of numeric ID in control functions. Also can be controlled in object-oriented way.

Functions supported in timer object:

- [`t:alarm()`](#tmralarm)
- [`t:interval()`](#tmrinterval)
- [`t:register()`](#tmrregister)
- [`t:resume()`](#tmrresume)
- [`t:start()`](#tmrstart)
- [`t:state()`](#tmrstate)
- [`t:stop()`](#tmrstop)
- [`t:suspend()`](#tmrsuspend)
- [`t:unregister()`](#tmrunregister)

#### Parameters
none

#### Returns
`timer` object

#### Example
```lua
local mytimer = tmr.create()

-- oo calling
mytimer:register(5000, tmr.ALARM_SINGLE, function (t) print("expired"); t:unregister() end)
mytimer:start()

-- with self parameter
tmr.register(mytimer, 5000, tmr.ALARM_SINGLE, function (t) print("expired"); tmr.unregister(t) end)
tmr.start(mytimer)
```

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
`tmr.interval([id/ref], interval_ms)`

#### Parameters
- `id`/`ref` timer id (0-6) or object, obsolete for OO API (→ [`tmr.create()`](#tmrcreate))
- `interval_ms` new timer interval in milliseconds. Maximum value is 6870947 (1:54:30.947).

#### Returns
`nil`

#### Example
```lua
mytimer = tmr.create()
mytimer:register(10000, tmr.ALARM_AUTO, function() print("hey there") end)
mytimer:interval(3000) -- actually, 3 seconds is better!
mytimer:start()
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
`tmr.register([id/ref], interval_ms, mode, func())`

#### Parameters
- `id`/`ref` timer id (0-6) or object, obsolete for OO API (→ [`tmr.create()`](#tmrcreate))
- `interval_ms` timer interval in milliseconds. Maximum value is 6870947 (1:54:30.947).
- `mode` timer mode:
	- `tmr.ALARM_SINGLE` a one-shot alarm (and no need to call [`tmr.unregister()`](#tmrunregister))
	- `tmr.ALARM_SEMI` manually repeating alarm (call [`tmr.start()`](#tmrunregister) to restart)
	- `tmr.ALARM_AUTO` automatically repeating alarm
- `func(timer)` callback function which is invoked with the timer object as an argument

Note that registering does *not* start the alarm.

#### Returns
`nil`

#### Example
```lua
mytimer = tmr.create()
mytimer:register(5000, tmr.ALARM_SINGLE, function() print("hey there") end)
mytimer:start()
```
#### See also
- [`tmr.create()`](#tmrcreate)
- [`tmr.alarm()`](#tmralarm)

## tmr.resume()

Resume an individual timer.

Resumes a timer that has previously been suspended with either `tmr.suspend` or `tmr.suspend_all` 

#### Syntax
`tmr.resume(id/ref)`

#### Parameters
`id/ref` timer id (0-6) or object, obsolete for OO API (→ [`tmr.create()`](#tmrcreate))

#### Returns
`true` if timer was resumed successfully 

#### Example
```lua
--resume timer mytimer
mytimer:resume()

--alternate metod
tmr.resume(mytimer)

```
#### See also
- [`tmr.suspend()`](#tmrsuspend)
- [`tmr.suspend_all()`](#tmrsuspendall)
- [`tmr.resume_all()`](#tmrresumeall)

## tmr.resume_all()

Resume all timers.

Resumes all timers including those previously been suspended with either `tmr.suspend` or `tmr.suspend_all` 

#### Syntax
`tmr.resume_all()`

#### Parameters
none

#### Returns
`true` if timers were resumed successfully 

#### Example
```lua
--resume all previously suspended timers
tmr.resume_all()

```
#### See also
- [`tmr.suspend()`](#tmrsuspend)
- [`tmr.suspend_all()`](#tmrsuspendall)
- [`tmr.resume()`](#tmrresume)

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
`tmr.start([id/ref])`

#### Parameters
`id`/`ref` timer id (0-6) or object, obsolete for OO API (→ [`tmr.create()`](#tmrcreate))

#### Returns
`true` if the timer was started, `false` on error

#### Example
```lua
mytimer = tmr.create()
mytimer:register(5000, tmr.ALARM_SINGLE, function() print("hey there") end)
if not mytimer:start() then print("uh oh") end
```
#### See also
- [`tmr.create()`](#tmrcreate)
- [`tmr.register()`](#tmrregister)
- [`tmr.stop()`](#tmrstop)
- [`tmr.unregister()`](#tmrunregister)

## tmr.state()

Checks the state of a timer.

#### Syntax
`tmr.state([id/ref])`

#### Parameters
`id`/`ref` timer id (0-6) or object, obsolete for OO API (→ [`tmr.create()`](#tmrcreate))

#### Returns
(bool, int) or `nil`

If the specified timer is registered, returns whether it is currently started and its mode. If the timer is not registered, `nil` is returned.

#### Example
```lua
mytimer = tmr.create()
print(mytimer:state()) -- nil
mytimer:register(5000, tmr.ALARM_SINGLE, function() print("hey there") end)
running, mode = mytimer:state()
print("running: " .. tostring(running) .. ", mode: " .. mode) -- running: false, mode: 0
```

## tmr.stop()

Stops a running timer, but does *not* unregister it. A stopped timer can be restarted with [`tmr.start()`](#tmrstart).

#### Syntax
`tmr.stop([id/ref])`

#### Parameters
`id`/`ref` timer id (0-6) or object, obsolete for OO API (→ [`tmr.create()`](#tmrcreate))

#### Returns
`true` if the timer was stopped, `false` on error

#### Example
```lua
mytimer = tmr.create()
if not mytimer:stop() then print("timer not stopped, not registered?") end
```
#### See also
- [`tmr.register()`](#tmrregister)
- [`tmr.stop()`](#tmrstop)
- [`tmr.unregister()`](#tmrunregister)

## tmr.suspend()

Suspend an armed timer. 

* Timers can be suspended at any time after they are armed.
* If a timer is rearmed with `tmr.start` or `tmr.alarm` any matching suspended timers will be discarded.

#### Syntax
`tmr.suspend(id/ref)`

#### Parameters
`id/ref` timer id (0-6) or object, obsolete for OO API (→ [`tmr.create()`](#tmrcreate))

#### Returns
`true` if timer was resumed successfully 

#### Example
```lua
--suspend timer mytimer
mytimer:suspend()

--alternate metod
tmr.suspend(mytimer)

```
#### See also
- [`tmr.suspend_all()`](#tmrsuspendall)
- [`tmr.resume()`](#tmrresume)
- [`tmr.resume_all()`](#tmrresumeall)


## tmr.suspend_all()

Suspend all currently armed timers. 

!!! Warning
	This function suspends ALL active timers, including any active timers started by the NodeMCU subsystem or other modules. this may cause parts of your program to stop functioning properly. 
	USE THIS FUNCTION AT YOUR OWN RISK!


#### Syntax
`tmr.suspend_all()`

#### Parameters
none

#### Returns
`true` if timers were suspended successfully 

#### Example
```lua
--suspend timer mytimer
tmr.suspend_all()

```
#### See also
- [`tmr.suspendl()`](#tmrsuspend)
- [`tmr.resume()`](#tmrresume)
- [`tmr.resume_all()`](#tmrresumeall)


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
`tmr.unregister([id/ref])`

#### Parameters
`id`/`ref` timer id (0-6) or object, obsolete for OO API (→ [`tmr.create()`](#tmrcreate))

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

# Timer Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-12 | [Zeroday](https://github.com/funshine) |              [dnc40085](https://github.com/dnc40085) | [tmr.c](../../components/modules/tmr.c)|

The tmr module allows access to simple timers. It is aimed at setting up regularly occurring tasks and timing out operations.

What the tmr module is *not* however, is a time keeping module. While all timeouts are expressed in milliseconds, the accuracy is limited and compounding errors would lead to rather inaccurate time keeping. A module for "wall clock" time is not yet available.

!!! note

    The resolution of the timers is determined by FreeRTOS' tick rate. The default rate of 100&nbsp;kHz (resulting in 10&nbsp;ms resolution) can be changed with `make menuconfig` at item `Component config ---> FreeRTOS ---> Tick rate (Hz)`.

## tmr.create()

Creates a dynamic timer object.

#### Syntax
`tmr.create()`

#### Parameters
none

#### Returns
`timer` object

#### Example
```lua
local mytimer = tmr.create()

-- oo calling
mytimer:register(5000, tmr.ALARM_SINGLE, function (t) print("expired") end)
mytimer:start()
mytimer = nil
```

# tmr Object
## tmr.obj:alarm()

This is a convenience function combining [`tmr.obj:register()`](#tmrobjregister) and [`tmr.obj:start()`](#tmrobjstart) into a single call.

To free up the resources with this timer when done using it, call [`tmr.obj:unregister()`](#tmrobjunregister) on it. For one-shot timers this is not necessary, unless they were stopped before they expired.

#### Syntax
`mytmr:alarm(interval_ms, mode, func())`

#### Parameters
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
- [`tmr.obj:register()`](#tmrobjregister)
- [`tmr.obj:start()`](#tmrobjstart)
- [`tmr.obj:unregister()`](#tmrobjunregister)

## tmr.obj:interval()

Changes a registered timer's expiry interval.

#### Syntax
`mytmr:interval(interval_ms)`

#### Parameters
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

## tmr.obj:register()

Configures a timer and registers the callback function to call on expiry.

To free up the resources with this timer when done using it, call [`tmr.obj:unregister()`](#tmrobjunregister) on it. For one-shot timers this is not necessary, unless they were stopped before they expired.

#### Syntax
`mytmr:register(interval_ms, mode, func())`

#### Parameters
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
- [`tmr.obj:alarm()`](#tmrobjalarm)

## tmr.obj:start()

Starts or restarts a previously configured timer.

#### Syntax
`mytmr:start()`

#### Parameters
none

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
- [`tmr.obj:register()`](#tmrobjregister)
- [`tmr.obj:stop()`](#tmrobjstop)
- [`tmr.obj:unregister()`](#tmrobjunregister)

## tmr.obj:state()

Checks the state of a timer.

#### Syntax
`mytmr:state()`

#### Parameters
none

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

## tmr.obj:stop()

Stops a running timer, but does *not* unregister it. A stopped timer can be restarted with [`tmr.obj:start()`](#tmrobjstart).

#### Syntax
`mytmr:stop()`

#### Parameters
none

#### Returns
`true` if the timer was stopped, `false` on error

#### Example
```lua
mytimer = tmr.create()
if not mytimer:stop() then print("timer not stopped, not registered?") end
```
#### See also
- [`tmr.obj:register()`](#tmrobjregister)
- [`tmr.obj:stop()`](#tmrobjstop)
- [`tmr.obj:unregister()`](#tmrobjunregister)

## tmr.obj:unregister()

Stops the timer (if running) and unregisters the associated callback.

This isn't necessary for one-shot timers (`tmr.ALARM_SINGLE`), as those automatically unregister themselves when fired.

#### Syntax
`mytmr:unregister()`

#### Parameters
none

#### Returns
`nil`

#### See also
[`tmr.obj:register()`](#tmrobjregister)


## tmr.wdclr()

Resets the watchdog timer to prevent a reboot due to a perceived hung task.

Use with caution, as this could prevent a reboot to recover from a
genuinely hung task.

On the ESP32, the `tmr.wdclr()` function is implemented as a task yield
to let the system "IDLE" task do the necessary watchdog maintenance.
Overuse of this function is likely to result in degraded performance.

#### Syntax
`tmr.wdclr()`

#### Parameters
none

#### Returns
`nil`

#### Example
```lua
function long_running_function()
  while 1
  do
    if some_condition then break end
    -- do some heavy calculation here, for example
    tmr.wdclr()
  end
end
```


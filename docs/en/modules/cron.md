# Cron Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-12-18 | [PhoeniX](https://github.com/djphoenix) | [PhoeniX](https://github.com/djphoenix) | [cron.c](../../../app/modules/cron.c)|

Cron-like scheduler module.

!!! important
    This module needs RTC time to operate correctly. Do not forget to enable [`rtctime`](rtctime.md) module

## cron.schedule()

Create new schedule entry.

#### Syntax
`cron.schedule(mask, callback)`

#### Parameters
- `mask` - [crontab](https://en.wikipedia.org/wiki/Cron#Overview)-like string mask for schedule
- `callback` - callback `function(entry)` that being performed at schedule time

#### Returns
`cron.entry` sub module

#### Example

```lua
cron.schedule("* * * * *", function(e)
  print("Every minute")
end)

cron.schedule("*/5 * * * *", function(e)
  print("Every 5 minutes")
end)

cron.schedule("0 */2 * * *", function(e)
  print("Every 2 hours")
end)
```

## cron.reset()

Remove all schedule entries.

#### Syntax
`cron.reset()`

#### Parameters
none

#### Returns
none

#### Example

```lua
cron.reset()
```

# cron.entry Module

## cron.entry:handler()

Set new handler for entry.

#### Syntax
`handler(callback)`

#### Parameters
- `callback` - callback `function(entry)` that being performed at schedule time

#### Returns
none

#### Example

```lua
ent = cron.schedule("* * * * *", function(e)
  print("Every minute")
end)

ent:handler(function(e)
  print("New handler: Every minute")
end)
```

## cron.entry:schedule()

Set new schedule mask.

#### Syntax
`schedule(mask)`

#### Parameters
- `mask` - [crontab](https://en.wikipedia.org/wiki/Cron#Overview)-like string mask for schedule

#### Returns
none

#### Example

```lua
ent = cron.schedule("* * * * *", function(e)
  print("Tick")
end)

-- Every 5 minutes is really better!
ent:schedule("*/5 * * * *")
```

## cron.entry:unschedule()

Disable schedule.

Disabled schedule may be enabled again by calling [`:schedule(mask)`](#cronentryschedule).

#### Syntax
`unschedule()`

#### Parameters
none

#### Returns
none

#### Example

```lua
ent = cron.schedule("* * * * *", function(e)
  print("Tick")
end)

-- We don't need this anymore
ent:unschedule()
```

# tz module

This is a simple module that parses timezone files as found on unix systems. It is oriented around converting the current time. It can convert other times, but it is
rather less efficient as it maintains only a single cached entry in memory.

On my linux system, these files can be found in `/usr/share/zoneinfo`. 


## tz.setzone()

This sets the timezone to be used in subsequent conversions

#### Syntax
`tz.setzone(timezone)`

#### Parameters
- `timezone` this is the timezone string. It must correspond to a file in the file system which is named timezone.zone.

#### Returns
true if the zone exists in the file system.

## tz.getoffset()

This gets the offset (in seconds) of the time passed as the argument.

#### Syntax
`tz.getoffset(time)`

#### Parameters
- `time` the number of seconds since the epoch. This is the same value as used by the `sntp` module.

#### Returns

- The number of seconds of offset. West of Greenwich is negative.
- The start time (in epoch seconds) of this offset.
- The end time (in epoch seconds) of this offset.

#### Example
```
tz = require('tz')
tz.setzone('eastern')
sntp.sync(nil, function(now)
  local tm = rtctime.epoch2cal(now + tz.getoffset(now))
  print(string.format("%04d/%02d/%02d %02d:%02d:%02d", tm["year"], tm["mon"], tm["day"], tm["hour"], tm["min"], tm["sec"]))
end)
```

## tz.getzones()

This returns a list of the available timezones in the file system.

#### Syntax
`tz.getzones()`

#### Returns
A list of timezones.

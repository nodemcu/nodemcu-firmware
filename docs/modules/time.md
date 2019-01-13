# Timer Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2018-11-25 | [Skirmantas Lauzikas](https://github.com/x-logLT) | [Skirmantas Lauzikas](https://github.com/x-logLT) | [time.c](../../components/modules/time.c)|

This module offers facilities for coverting beteen unix time and calendar, setting/getting system time, locale and controll of NTP client.

## time.set()
Sets system time to a given timestamp in the Unix epoch (i.e. seconds from midnight 1970/01/01).

#### Syntax
`time.set(time)`

#### Parameters
- `time` number of seconds since the Epoch

#### Returns
`nil'

#### Example
```lua
--set time to 2018-11-20 01:40:50
time.set(1542678050)
```

#### See also
[`time.cal2epoc()`](#timecal2epoch)

## time.get()
Returns current system time in the Unix epoch (i.e. seconds from midnight 1970/01/01).

#### Syntax
time.get()

#### Parameters
none

#### Returns
A two-value timestamp consisting of:

- `sec` seconds since the Unix epoch
- `usec` the microseconds part

#### Example
```lua
sec, usec = time.get()
```

#### See also
[`time.epch2cal()`](#timeepoch2cal)

## time.getlocal()
Returns current system time adjusted for the locale in calendar format.

#### Syntax
time.getlocal()

#### Parameters
none

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
- `dst` day time adjustment:
    - 1 (DST in effect, i.e. daylight time)
	- 0 (DST not in effect, i.e. standard time) 
	- -1 (Unknown DST status)

#### Example
```lua
localTime = time.getlocal()
print(string.format("%04d-%02d-%02d %02d:%02d:%02d DST:%d", localTime["year"], localTime["mon"], localTime["day"], localTime["hour"], localTime["min"], localTime["sec"], localTime["dst"]))
```

## time.settimezone()
Sets correct format for Time Zone locale

#### Syntax
`time.settimezone(timezone)`

#### Parameters
- `timezone` a string representing timezone, can also include DST adjustment. For full syntax see [TZ variable documentation](http://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html).

#### Returns
`nil`

#### Example
```lua
--set timezone to Eastern Standard Time
time.settimezone("EST+5")
```

## time.initntp()
Initializes and starts ntp client 

#### Syntax
`time.initntp([ntpAddr])`

#### Parameters
- `ntpAddr` address of a ntp server, defaults to "pool.ntp.org" if none is specified

#### Returns
`nil`

#### Example
```lua
time.initntp("pool.ntp.org")
```

## time.ntpenabled()
Checks if ntp client is enabled.

#### Syntax
`time.ntpenabled()`

#### Parameters
none

#### Returns
`true' if ntp client is enabled.

## time.ntpstop()
Stops ntp client.

#### Syntax
`time.ntpstop()`

#### Parameters
none

#### Returns
`nil`

## time.epoch2cal()
Converts timestamp in Unix epoch to calendar format

#### Syntax
`time.epoch2cal(time)

#### Parameters
- `time` number of seconds since the Epoch

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
- `dst` day time adjustment:
    - 1 (DST in effect, i.e. daylight time)
	- 0 (DST not in effect, i.e. standard time) 
	- -1 (Unknown DST status)

#### Example
```lua
--Gets current time calendar format, no locale adjustment
time = time.epoch2cal(time.get())
print(string.format("%04d-%02d-%02d %02d:%02d:%02d DST:%d", time["year"], time["mon"], time["day"], time["hour"], time["min"], time["sec"], time["dst"]))
```

## time.cal2epoch()
Converts calendar table to a timestamp in Unix epoch

#### Syntax
`time.cal2epoch(calendar)`

#### Parameters
- `calendar` Table containing calendar info.
    - `year` 1970 ~ 2038
    - `mon` month 1 ~ 12 in current year
    - `day` day 1 ~ 31 in current month
    - `hour`
    - `min`
    - `sec`

#### Returns
number of seconds since the Epoch

#### Example

```lua
calendar={}
calendar.year = 2018-11-20
calendar.mon = 11
calendar.day = 20
calendar.hour = 1
calendar.min = 40
calendar.sec = 50

timestamp = time.cal2epoch(calendar)
time.set(timestamp)
```


# WPS Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-01-01 | [Frank Exoo](https://github.com/FrankX0) | [Frank Exoo](https://github.com/FrankX0) | [wps.c](../../../app/modules/wps.c)|

[WPS](https://en.wikipedia.org/wiki/WiFi_Protected_Setup) allows devices to be added to an existing network without entering the network credentials.

## wps.disable()
Disable WiFi WPS function.

#### Syntax
`wps.disable()`

#### Parameters
none

#### Returns
`nil`

## wps.enable()
Enable WiFi WPS function.

#### Syntax
`wps.enable()`

#### Parameters
none

#### Returns
`nil`

## wps.start()
Start WiFi WPS function. WPS must be enabled prior calling this function.

#### Syntax
`wps.start([function(status)])`

#### Parameters
- `function(status)` callback function for when the WPS function ends.

#### Returns
`nil`

#### Example
```lua
wps.enable()
wps.start(function(status)
  if status == wps.SUCCESS then
    print("SUCCESS!")
  elseif status == wps.FAILED then
    print("Failed")
  elseif status == wps.TIMEOUT then
    print("Timeout")
  elseif status == wps.WEP then
    print("WEP not supported")
  elseif status == wps.SCAN_ERR then
    print("WPS AP not found")
  else
    print(status)
  end
  wps.disable()
end)
```

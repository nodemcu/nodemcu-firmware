# WPS Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2017-01-01 | [Frank Exoo](https://github.com/FrankX0) | [Frank Exoo](https://github.com/FrankX0) | [wps.c](../../../app/modules/wps.c)|

[WPS](https://en.wikipedia.org/wiki/Wi-Fi_Protected_Setup) allows devices to be added to an existing network without entering the network credentials.

!!! danger

    Use this with caution. There are serious security concerns about using WPS.

    WPA/WPA2 networks that have the WPS feature enabled are [very easy to crack](http://www.howtogeek.com/176124/wi-fi-protected-setup-wps-is-insecure-heres-why-you-should-disable-it/). Once the WPS pin has been stolen [the router gives out the password](https://scotthelme.co.uk/wifi-insecurity-wps/) even if it has been changed.

    You should use WPA/WPA2 with the WPS feature disabled.

## wps.disable()
Disable WiFi WPS function.

#### Parameters
none

#### Returns
`nil`

## wps.enable()
Enable WiFi WPS function.

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

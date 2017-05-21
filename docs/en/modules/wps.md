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

!!! note
	This function only configures the station with the AP's info, it does not connect to AP automatically.

#### Syntax
`wps.start([function(status)])`

#### Parameters
- `function(status)` callback function for when the WPS function ends.

#### Returns
`nil`

#### Example
```lua
  --Basic example
  wifi.setmode(wifi.STATION)
  wps.enable()
  wps.start(function(status)
    if status == wps.SUCCESS then
      wps.disable()
      print("WPS: Success, connecting to AP...")
      wifi.sta.connect()
      return
    elseif status == wps.FAILED then
      print("WPS: Failed")
    elseif status == wps.TIMEOUT then
      print("WPS: Timeout")
    elseif status == wps.WEP then
      print("WPS: WEP not supported")
    elseif status == wps.SCAN_ERR then
      print("WPS: AP not found")
    else
      print(status)
    end
    wps.disable()
  end)
  
  --Full example
  do
    -- Register wifi station event callbacks
    wifi.eventmon.register(wifi.eventmon.STA_CONNECTED, function(T)
      print("\n\tSTA - CONNECTED".."\n\tSSID: "..T.SSID.."\n\tBSSID: "..
      T.BSSID.."\n\tChannel: "..T.channel)
    end)
    wifi.eventmon.register(wifi.eventmon.STA_GOT_IP, function(T)
      print("\n\tSTA - GOT IP".."\n\tStation IP: "..T.IP.."\n\tSubnet mask: "..
      T.netmask.."\n\tGateway IP: "..T.gateway)
    end)

    wifi.setmode(wifi.STATION)
  
    wps_retry_func = function() 
      if wps_retry_count == nil then wps_retry_count = 0 end
      if wps_retry_count < 3 then 
        wps.disable()
        wps.enable()
        wps_retry_count = wps_retry_count + 1
        wps_retry_timer = tmr.create()
        wps_retry_timer:alarm(3000, tmr.ALARM_SINGLE, function() wps.start(wps_cb) end)
        print("retry #"..wps_retry_count)
      else
        wps_retry_count = nil
        wps_retry_timer = nil
        wps_retry_func = nil
        wps_cb = nil
      end
    end
  
    wps_cb = function(status)
      if status == wps.SUCCESS then
        wps.disable()
        print("WPS: success, connecting to AP...")
        wifi.sta.connect()
        wps_retry_count = nil
        wps_retry_timer = nil
        wps_retry_func = nil
        wps_cb = nil
        return
      elseif status == wps.FAILED then
        print("WPS: Failed")
        wps_retry_func()
        return
      elseif status == wps.TIMEOUT then
        print("WPS: Timeout")
        wps_retry_func()
        return
      elseif status == wps.WEP then
        print("WPS: WEP not supported")
      elseif status == wps.SCAN_ERR then
        print("WPS: AP not found")
        wps_retry_func()
        return
      else
        print(status)
      end
      wps.disable()
      wps_retry_count = nil
      wps_retry_timer = nil
      wps_retry_func = nil
      wps_cb = nil
    end
    wps.enable()
    wps.start(wps_cb)
  end
  
```

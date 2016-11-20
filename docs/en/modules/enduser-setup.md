# enduser setup Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-09-02 | [Robert Foss](https://github.com/robertfoss) | [Robert Foss](https://github.com/robertfoss) | [enduser_setup.c](../../../app/modules/enduser_setup.c)|

This module provides a simple way of configuring ESP8266 chips without using a serial interface or pre-programming WiFi credentials onto the chip.

![enduser setup config dialog](../../img/enduser-setup.jpg "enduser setup config dialog")

After running [`enduser_setup.start()`](#enduser_setupstart), a wireless network named "SetupGadget_XXXXXX" will start (this prefix can be overridden in `user_config.h` by defining 
`ENDUSER_SETUP_AP_SSID`). Connect to that SSID and then navigate to the root
of any website (e.g., `http://example.com/` will work, but do not use `.local` domains because it will fail on iOS). A web page similar to the picture above will load, allowing the 
end user to provide their Wi-Fi information.

After an IP address has been successfully obtained, then this module will stop as if [`enduser_setup.stop()`](#enduser_setupstop) had been called. There is a 10-second delay before
teardown to allow connected clients to obtain a last status message while the SoftAP is still active.

Alternative HTML can be served by placing a file called `enduser_setup.html` on the filesystem. Everything needed by the web page must be included in this one file. This file will be kept 
in RAM, so keep it as small as possible. The file can be gzip'd ahead of time to reduce the size (i.e., using `gzip -n` or `zopfli`), and when served, the End User Setup module will add 
the appropriate `Content-Encoding` header to the response. 

*Note: If gzipped, the file can also be named `enduser_setup.html.gz` for semantic purposes. Gzip encoding is determined by the file's contents, not the filename.*

The following HTTP endpoints exist:

|Endpoint|Description|
|--------|-----------|
|/|Returns HTML for the web page. Will return the contents of `enduser_setup.html` if it exists on the filesystem, otherwise will return a page embedded into the firmware image.|
|/aplist|Forces the ESP8266 to perform a site survey across all channels, reporting access points that it can find. Return payload is a JSON array: `[{"ssid":"foobar","rssi":-36,"chan":3}]`|
|/generate_204|Returns a HTTP 204 status (expected by certain Android clients during Wi-Fi connectivity checks)|
|/status|Returns plaintext status description, used by the web page|
|/status.json|Returns a JSON payload containing the ESP8266's chip id in hexadecimal format and the status code: 0=Idle, 1=Connecting, 2=Wrong Password, 3=Network not Found, 4=Failed, 5=Success|
|/setwifi|Endpoint intended for services to use for setting the wifi credentials. Identical to `/update` except returns the same payload as `/status.json` instead of redirecting to `/`.|
|/update|Form submission target. Example: `http://example.com/update?wifi_ssid=foobar&wifi_password=CorrectHorseBatteryStaple`. Must be a GET request. Will redirect to `/` when complete. |


## enduser_setup.manual()

Controls whether manual AP configuration is used.

By default the `enduser_setup` module automatically configures an open access point when starting, and stops it when the device has been successfully joined to a WiFi network. If manual mode has been enabled, neither of this is done. The device must be manually configured for `wifi.SOFTAP` mode prior to calling `enduser_setup.start()`. Additionally, the portal is not stopped after the device has successfully joined to a WiFi network.


#### Syntax
`enduser_setup.manual([on_off])`

#### Parameters
  - `on_off` a boolean value indicating whether to use manual mode; if not given, the function only returns the current setting.

#### Returns
The current setting, true if manual mode is enabled, false if it is not.

#### Example
```lua
wifi.setmode(wifi.STATIONAP)
wifi.ap.config({ssid="MyPersonalSSID", auth=wifi.OPEN})
enduser_setup.manual(true)
enduser_setup.start(
  function()
    print("Connected to wifi as:" .. wifi.sta.getip())
  end,
  function(err, str)
    print("enduser_setup: Err #" .. err .. ": " .. str)
  end
);
```

## enduser_setup.start()

Starts the captive portal. 

*Note: Calling start() while EUS is already running is an error, and will result in stop() to be invoked to shut down EUS.*

#### Syntax
`enduser_setup.start([onConnected()], [onError(err_num, string)], [onDebug(string)])`

#### Parameters
 - `onConnected()` callback will be fired when an IP-address has been obtained, just before the enduser_setup module will terminate itself
 - `onError()` callback will be fired if an error is encountered. `err_num` is a number describing the error, and `string` contains a description of the error.
 - `onDebug()` callback is disabled by default (controlled by `#define ENDUSER_SETUP_DEBUG_ENABLE` in `enduser_setup.c`). It is intended to be used to find internal issues in the module. `string` contains a description of what is going on.

#### Returns
`nil`

#### Example
```lua
enduser_setup.start(
  function()
    print("Connected to wifi as:" .. wifi.sta.getip())
  end,
  function(err, str)
    print("enduser_setup: Err #" .. err .. ": " .. str)
  end,
  print -- Lua print function can serve as the debug callback
);
```

## enduser_setup.stop()

Stops the captive portal.

#### Syntax
`enduser_setup.stop()`

#### Parameters
none

#### Returns
`nil`

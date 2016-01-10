# enduser setup Module
This module provides a simple way of configuring ESP8266 chips without using a serial interface or pre-programming WiFi credentials onto the chip.

![](https://github.com/robertfoss/esp8266_nodemcu_wifi_setup/blob/images/screenshot.png?raw=true)

After running [`enduser_setup.start()`](#enduser_setupstart) a portal like the above can be accessed through a wireless network called SetupGadget_XXXXXX. The portal is used to submit the credentials for the WiFi of the enduser.
After an IP address has been successfully obtained this module will stop as if [`enduser_setup.stop()`](#enduser_setupstop) had been called.

## enduser_setup.start()

Starts the captive portal.

#### Syntax
`enduser_setup.start([onConfigured()], [onError(err_num, string)], [onDebug(string)])`

#### Parameters
 - `onConfigured()` callback will be fired when an IP-address has been obtained, just before the enduser_setup module will terminate itself
 - `onError()` callback will be fired if an error is encountered. `err_num` is a number describing the error, and `string` contains a description of the error.
 - `onDebug()` callback is disabled by default. It is intended to be used to find internal issues in the module. `string` contains a description of what is going on.

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
  end
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
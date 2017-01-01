# WiFi Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-05-12 | [Zeroday](https://github.com/funshine) | [dnc40085](https://github.com/dnc40085) | [wifi.c](../../../app/modules/wifi.c)|

The NodeMCU WiFi control is spread across several tables:

- `wifi` for overall WiFi configuration
- [`wifi.sta`](#wifista-module) for station mode functions
- [`wifi.ap`](#wifiap-module) for wireless access point (WAP or simply AP) functions
- [`wifi.ap.dhcp`](#wifiapdhcp-module) for DHCP server control
- [`wifi.eventmon`](#wifieventmon-module) for wifi event monitor

## wifi.getchannel()

Gets the current WiFi channel.

#### Syntax
`wifi.getchannel()`

#### Parameters
`nil`

#### Returns
current WiFi channel

## wifi.getdefaultmode()

Gets default WiFi operation mode.

#### Syntax
`wifi.getdefaultmode()`

#### Parameters
`nil`

#### Returns
The WiFi mode, as one of the `wifi.STATION`, `wifi.SOFTAP`, `wifi.STATIONAP` or `wifi.NULLMODE` constants.

#### See also
[`wifi.getmode()`](#wifigetmode)
[`wifi.setmode()`](#wifisetmode)

## wifi.getmode()

Gets WiFi operation mode.

#### Syntax
`wifi.getmode()`

#### Parameters
`nil`

#### Returns
The WiFi mode, as one of the `wifi.STATION`, `wifi.SOFTAP`, `wifi.STATIONAP` or `wifi.NULLMODE` constants.

#### See also
[`wifi.getdefaultmode()`](#wifigetdefaultmode)
[`wifi.setmode()`](#wifisetmode)

## wifi.getphymode()

Gets WiFi physical mode.

#### Syntax
`wifi.getphymode()`

#### Parameters
none

#### Returns
The current physical mode as one of `wifi.PHYMODE_B`, `wifi.PHYMODE_G` or `wifi.PHYMODE_N`.

#### See also
[`wifi.setphymode()`](#wifisetphymode)

## wifi.setmode()

Configures the WiFi mode to use. NodeMCU can run in one of four WiFi modes:

- Station mode, where the NodeMCU device joins an existing network
- Access point (AP) mode, where it creates its own network that others can join
- Station + AP mode, where it both creates its own network while at the same time being joined to another existing network
- WiFi off

When using the combined Station + AP mode, the same channel will be used for both networks as the radio can only listen on a single channel.

NOTE: WiFi Mode configuration will be retained until changed even if device is turned off.

#### Syntax
`wifi.setmode(mode[, save])`

#### Parameters
- `mode` value should be one of
	- `wifi.STATION` for when the device is connected to a WiFi router. This is often done to give the device access to the Internet.
	- `wifi.SOFTAP` for when the device is acting *only* as an access point. This will allow you to see the device in the list of WiFi networks (unless you hide the SSID, of course). In this mode your computer can connect to the device, creating a local area network. Unless you change the value, the NodeMCU device will be given a local IP address of 192.168.4.1 and assign your computer the next available IP address, such as 192.168.4.2.
	- `wifi.STATIONAP` is the combination of `wifi.STATION` and `wifi.SOFTAP`. It allows you to create a local WiFi connection *and* connect to another WiFi router.
	- `wifi.NULLMODE` changing WiFi mode to NULL_MODE will put wifi into a low power state similar to MODEM_SLEEP, provided `wifi.nullmodesleep(false)` has not been called.
- `save` choose whether or not to save wifi mode to flash
	- `true` WiFi mode configuration **will** be retained through power cycle. (Default)
	- `false` WiFi mode configuration **will not** be retained through power cycle.

#### Returns
current mode after setup

#### Example
```lua
wifi.setmode(wifi.STATION)
```

#### See also
[`wifi.getmode()`](#wifigetmode)
[`wifi.getdefaultmode()`](#wifigetdefaultmode)

## wifi.setphymode()

Sets WiFi physical mode.

- `wifi.PHYMODE_B`
    802.11b, more range, low Transfer rate, more current draw
- `wifi.PHYMODE_G`
    802.11g, medium range, medium transfer rate, medium current draw
- `wifi.PHYMODE_N`
    802.11n, least range, fast transfer rate, least current draw (STATION ONLY)
Information from the Espressif datasheet v4.3

|           Parameters                        |Typical Power Usage|
|---------------------------------------------|-------------------|
|Tx 802.11b, CCK 11Mbps, P OUT=+17dBm         |     170 mA        |
|Tx 802.11g, OFDM 54Mbps, P OUT =+15dBm       |     140 mA        |
|Tx 802.11n, MCS7 65Mbps, P OUT =+13dBm       |     120 mA        |
|Rx 802.11b, 1024 bytes packet length, -80dBm |      50 mA        |
|Rx 802.11g, 1024 bytes packet length, -70dBm |      56 mA        |
|Rx 802.11n, 1024 bytes packet length, -65dBm |      56 mA        |

#### Syntax
`wifi.setphymode(mode)`

#### Parameters
`mode` one of the following

- `wifi.PHYMODE_B`
- `wifi.PHYMODE_G`
- `wifi.PHYMODE_N`

#### Returns
physical mode after setup

#### See also
[`wifi.getphymode()`](#wifigetphymode)

## wifi.nullmodesleep()

Configures whether or not WiFi automatically goes to sleep in NULL_MODE. Enabled by default.

!!! note
	This function **does not** store it's setting in flash, if auto sleep in NULL_MODE is not desired, `wifi.nullmodesleep(false)` must be called after power-up, restart, or wake from deep sleep.

#### Syntax
`wifi.nullmodesleep([enable])`

#### Parameters
- `enable`
  - `true` Enable WiFi auto sleep in NULL_MODE. (Default setting)
  - `false` Disable WiFi auto sleep in NULL_MODE.

#### Returns
- `sleep_enabled` Current/New NULL_MODE sleep setting
	- If `wifi.nullmodesleep()` is called with no arguments, current setting is returned.
	- If `wifi.nullmodesleep()` is called with `enable` argument, confirmation of new setting is returned.

## wifi.startsmart()

Starts to auto configuration, if success set up SSID and password automatically.

Intended for use with SmartConfig apps, such as Espressif's [Android & iOS app](https://github.com/espressifapp).

Only usable in `wifi.STATION` mode.

!!! important

    SmartConfig is disabled by default and can be enabled by setting `WIFI_SMART_ENABLE` in [`user_config.h`](https://github.com/nodemcu/nodemcu-firmware/blob/dev/app/include/user_config.h#L96) before you build the firmware.

#### Syntax
`wifi.startsmart(type, callback)`

#### Parameters
- `type` 0 for ESP\_TOUCH, or 1 for AIR\_KISS.
- `callback` a callback function of the form `function(ssid, password) end` which gets called after configuration.

#### Returns
`nil`

#### Example

```lua
wifi.setmode(wifi.STATION)
wifi.startsmart(0,
	function(ssid, password)
		print(string.format("Success. SSID:%s ; PASSWORD:%s", ssid, password))
	end
)
```

#### See also
[`wifi.stopsmart()`](#wifistopsmart)

## wifi.stopsmart()

Stops the smart configuring process.

#### Syntax
`wifi.stopsmart()`

#### Parameters
none

#### Returns
`nil`

#### See also
[`wifi.startsmart()`](#wifistartsmart)

# wifi.sta Module

## wifi.sta.autoconnect()

Auto connects to AP in station mode.

#### Syntax
`wifi.sta.autoconnect(auto)`

#### Parameters
`auto` 0 to disable auto connecting, 1 to enable auto connecting

#### Returns
`nil`

#### Example
```lua
wifi.sta.autoconnect(1)
```

#### See also
- [`wifi.sta.config()`](#wifistaconfig)
- [`wifi.sta.connect()`](#wifistaconnect)
- [`wifi.sta.disconnect()`](#wifistadisconnect)

## wifi.sta.changeap()

Select Access Point from list returned by `wifi.sta.getapinfo()`

#### Syntax
`wifi.sta.changeap(ap_index)`

#### Parameters
`ap_index` Index of Access Point you would like to change to. (Range:1-5)
 - Corresponds to index used by [`wifi.sta.getapinfo()`](#wifistagetapinfo) and [`wifi.sta.getapindex()`](#wifistagetapindex)

#### Returns
- `true`  Success
- `false` Failure

#### Example
```lua
wifi.sta.changeap(4)
```

#### See also
- [`wifi.sta.getapinfo()`](#wifistagetapinfo)
- [`wifi.sta.getapindex()`](#wifistagetapindex)

## wifi.sta.config()

Sets the WiFi station configuration.

#### Syntax
`wifi.sta.config(station_config)`

#### Parameters
- `station_config` table containing configuration data for station
	- `ssid` string which is less than 32 bytes.
	- `pwd` string which is 8-64 or 0 bytes. Empty string indicates an open WiFi access point.
	- `auto` defaults to true
		- `true` to enable auto connect and connect to access point, hence with `auto=true` there's no need to call [`wifi.sta.connect()`](#wifistaconnect)
		- `false` to disable auto connect and remain disconnected from access point
	- `bssid` string that contains the MAC address of the access point (optional)
		- You can set BSSID if you have multiple access points with the same SSID.
		- If you set BSSID for a specific SSID and would like to configure station to connect to the same SSID only without the BSSID requirement, you MUST first configure to station to a different SSID first, then connect to the desired SSID
		- The following formats are valid:
			- "DE:C1:A5:51:F1:ED"
			- "AC-1D-1C-B1-0B-22"
			- "DE AD BE EF 7A C0"
	- `save` Save station configuration to flash. 
		- `true` configuration **will** be retained through power cycle. 
		- `false` configuration **will not** be retained through power cycle. (Default)

#### Returns
- `true`  Success
- `false` Failure

#### Example

```lua
--connect to Access Point (DO NOT save config to flash)
station_cfg={}
station_cfg.ssid="NODE-AABBCC"
station_cfg.pwd="password"
wifi.sta.config(station_cfg)

--connect to Access Point (DO save config to flash)
station_cfg={}
station_cfg.ssid="NODE-AABBCC"
station_cfg.pwd="password"
station_cfg.save=true
wifi.sta.config(station_cfg)

--connect to Access Point with specific MAC address  
station_cfg={}
station_cfg.ssid="NODE-AABBCC"
station_cfg.pwd="password"
station_cfg.bssid="AA:BB:CC:DD:EE:FF"
wifi.sta.config(station_cfg)

--configure station but don't connect to Access point
station_cfg={}
station_cfg.ssid="NODE-AABBCC"
station_cfg.pwd="password"
station_cfg.auto=false
wifi.sta.config(station_cfg)

```

#### See also
- [`wifi.sta.connect()`](#wifistaconnect)
- [`wifi.sta.disconnect()`](#wifistadisconnect)
- [`wifi.sta.apinfo()`](#wifistaapinfo)

## wifi.sta.connect()

Connects to the configured AP in station mode. You only ever need to call this if auto-connect was disabled in [`wifi.sta.config()`](#wifistaconfig).

#### Syntax
`wifi.sta.connect()`

#### Parameters
none

#### Returns
`nil`

#### See also
- [`wifi.sta.disconnect()`](#wifistadisconnect)
- [`wifi.sta.config()`](#wifistaconfig)

## wifi.sta.disconnect()

Disconnects from AP in station mode.

!!! note
	Please note that disconnecting from Access Point does not reduce power consumption. If power saving is your goal, please refer to the description for `wifi.NULLMODE` in the function [`wifi.setmode()`](#wifisetmode) for more details.

#### Syntax
`wifi.sta.disconnect()`

#### Parameters
none

#### Returns
`nil`

#### See also
- [`wifi.sta.config()`](#wifistaconfig)
- [`wifi.sta.connect()`](#wifistaconnect)

## wifi.sta.eventMonReg()

Registers callbacks for WiFi station status events.

####  Syntax
- `wifi.sta.eventMonReg(wifi_status[, function([previous_state])])`

####  Parameters
- `wifi_status` WiFi status you would like to set a callback for:
    - `wifi.STA_IDLE`
    - `wifi.STA_CONNECTING`
    - `wifi.STA_WRONGPWD`
    - `wifi.STA_APNOTFOUND`
    - `wifi.STA_FAIL`
    - `wifi.STA_GOTIP`
- `function` callback function to perform when event occurs
	- Note: leaving field blank unregisters callback.
- `previous_state` previous wifi_state(0 - 5)

####  Returns
`nil`

####  Example
```lua
--register callback
wifi.sta.eventMonReg(wifi.STA_IDLE, function() print("STATION_IDLE") end)
wifi.sta.eventMonReg(wifi.STA_CONNECTING, function() print("STATION_CONNECTING") end)
wifi.sta.eventMonReg(wifi.STA_WRONGPWD, function() print("STATION_WRONG_PASSWORD") end)
wifi.sta.eventMonReg(wifi.STA_APNOTFOUND, function() print("STATION_NO_AP_FOUND") end)
wifi.sta.eventMonReg(wifi.STA_FAIL, function() print("STATION_CONNECT_FAIL") end)
wifi.sta.eventMonReg(wifi.STA_GOTIP, function() print("STATION_GOT_IP") end)

--register callback: use previous state
wifi.sta.eventMonReg(wifi.STA_CONNECTING, function(previous_State)
	if(previous_State==wifi.STA_GOTIP) then
		print("Station lost connection with access point\n\tAttempting to reconnect...")
	else
		print("STATION_CONNECTING")
	end
end)

--unregister callback
wifi.sta.eventMonReg(wifi.STA_IDLE)
```
#### See also
- [`wifi.sta.eventMonStart()`](#wifistaeventmonstart)
- [`wifi.sta.eventMonStop()`](#wifistaeventmonstop)
- [`wifi.eventmon.register()`](#wifieventmonregister)
- [`wifi.eventmon.unregister()`](#wifieventmonunregister)


## wifi.sta.eventMonStart()

Starts WiFi station event monitor.

####  Syntax
`wifi.sta.eventMonStart([ms])`

### Parameters
- `ms` interval between checks in milliseconds, defaults to 150ms if not provided.

####  Returns
`nil`

####  Example
```lua
--start WiFi event monitor with default interval
wifi.sta.eventMonStart()

--start WiFi event monitor with 100ms interval
wifi.sta.eventMonStart(100)
```

#### See also
- [`wifi.sta.eventMonReg()`](#wifistaeventmonreg)
- [`wifi.sta.eventMonStop()`](#wifistaeventmonstop)
- [`wifi.eventmon.register()`](#wifieventmonregister)
- [`wifi.eventmon.unregister()`](#wifieventmonunregister)

## wifi.sta.eventMonStop()

Stops WiFi station event monitor.
####  Syntax
`wifi.sta.eventMonStop([unregister_all])`

####  Parameters
- `unregister_all` enter 1 to unregister all previously registered functions.
	- Note: leave blank to leave callbacks registered

####  Returns
`nil`

####  Example
```lua
--stop WiFi event monitor
wifi.sta.eventMonStop()

--stop WiFi event monitor and unregister all callbacks
wifi.sta.eventMonStop(1)
```

#### See also
- [`wifi.sta.eventMonReg()`](#wifistaeventmonreg)
- [`wifi.sta.eventMonStart()`](#wifistaeventmonstart)
- [`wifi.eventmon.register()`](#wifieventmonregister)
- [`wifi.eventmon.unregister()`](#wifieventmonunregister)

## wifi.sta.getap()

Scans AP list as a Lua table into callback function.

#### Syntax
`wifi.sta.getap([[cfg], format,] callback(table))`

#### Parameters
- `cfg` table that contains scan configuration
	- `ssid` SSID == nil, don't filter SSID
	- `bssid` BSSID == nil, don't filter BSSID
	- `channel` channel == 0, scan all channels, otherwise scan set channel (default is 0)
	- `show_hidden` show_hidden == 1, get info for router with hidden SSID (default is 0)
- `format` select output table format, defaults to 0
	- 0: old format (SSID : Authmode, RSSI, BSSID, Channel), any duplicate SSIDs will be discarded
	- 1: new format (BSSID : SSID, RSSI, auth mode, Channel)
- `callback(table)` a callback function to receive the AP table when the scan is done. This function receives a table, the key is the BSSID, the value is other info in format: SSID, RSSID, auth mode, channel.

#### Returns
`nil`

#### Example

```lua
-- print AP list in old format (format not defined)
function listap(t)
	for k,v in pairs(t) do
		print(k.." : "..v)
	end
end
wifi.sta.getap(listap)

-- Print AP list that is easier to read
function listap(t) -- (SSID : Authmode, RSSI, BSSID, Channel)
	print("\n"..string.format("%32s","SSID").."\tBSSID\t\t\t\t  RSSI\t\tAUTHMODE\tCHANNEL")
	for ssid,v in pairs(t) do
		local authmode, rssi, bssid, channel = string.match(v, "([^,]+),([^,]+),([^,]+),([^,]+)")
		print(string.format("%32s",ssid).."\t"..bssid.."\t  "..rssi.."\t\t"..authmode.."\t\t\t"..channel)
	end
end
wifi.sta.getap(listap)

-- print AP list in new format
function listap(t)
	for k,v in pairs(t) do
		print(k.." : "..v)
	end
end
wifi.sta.getap(1, listap)

-- Print AP list that is easier to read
function listap(t) -- (SSID : Authmode, RSSI, BSSID, Channel)
 	print("\n\t\t\tSSID\t\t\t\t\tBSSID\t\t\t  RSSI\t\tAUTHMODE\t\tCHANNEL")
  	for bssid,v in pairs(t) do
   		local ssid, rssi, authmode, channel = string.match(v, "([^,]+),([^,]+),([^,]+),([^,]*)")
    	print(string.format("%32s",ssid).."\t"..bssid.."\t  "..rssi.."\t\t"..authmode.."\t\t\t"..channel)
	end
end
wifi.sta.getap(1, listap)

--check for specific AP
function listap(t)
 	print("\n\t\t\tSSID\t\t\t\t\tBSSID\t\t\t  RSSI\t\tAUTHMODE\t\tCHANNEL")
	for bssid,v in pairs(t) do
		local ssid, rssi, authmode, channel = string.match(v, "([^,]+),([^,]+),([^,]+),([^,]*)")
		print(string.format("%32s",ssid).."\t"..bssid.."\t  "..rssi.."\t\t"..authmode.."\t\t\t"..channel)
	end
end
scan_cfg = {}
scan_cfg.ssid = "myssid"
scan_cfg.bssid = "AA:AA:AA:AA:AA:AA"
scan_cfg.channel = 0
scan_cfg.show_hidden = 1
wifi.sta.getap(scan_cfg, 1, listap)

--get RSSI for currently configured AP
function listap(t)
	for bssid,v in pairs(t) do
		local ssid, rssi, authmode, channel = string.match(v, "([^,]+),([^,]+),([^,]+),([^,]*)")
		print("CURRENT RSSI IS: "..rssi)
	end
end
ssid, tmp, bssid_set, bssid=wifi.sta.getconfig()

scan_cfg = {}
scan_cfg.ssid = ssid
if bssid_set == 1 then scan_cfg.bssid = bssid else scan_cfg.bssid = nil end
scan_cfg.channel = wifi.getchannel()
scan_cfg.show_hidden = 0
ssid, tmp, bssid_set, bssid=nil, nil, nil, nil
wifi.sta.getap(scan_cfg, 1, listap)

```

#### See also
[`wifi.sta.getip()`](#wifistagetip)

## wifi.sta.getapindex()

Get index of current Access Point stored in AP cache.

#### Syntax
`wifi.sta.getapindex()`

#### Parameters
none

#### Returns
`current_index` index of currently selected Access Point. (Range:1-5)

#### Example
```lua
print("the index of the currently selected AP is: "..wifi.sta.getapindex())
```

#### See also
- [`wifi.sta.getapindex()`](#wifistagetapindex)
- [`wifi.sta.apinfo()`](#wifistaapinfo)
- [`wifi.sta.apchange()`](#wifistaapchange)

## wifi.sta.getapinfo()

Get information of APs cached by ESP8266 station.

!!! Note
		Any Access Points configured with save disabled `wifi.sta.config({save=false})` will populate this list (appearing to overwrite APs stored in flash) until restart.

#### Syntax
`wifi.sta.getapinfo()`

#### Parameters
`nil`

#### Returns
- `ap_info`
	- `qty` quantity of APs returned
	- `1-5` index of AP. (the index corresponds to index used by [`wifi.sta.changeap()`](#wifistachangeap) and [`wifi.sta.getapindex()`](#wifistagetapindex))
	- `ssid`  ssid of Access Point
	- `pwd` password for Access Point, `nil` if no password was configured 
	- `bssid` MAC address of Access Point, `nil` if no MAC address was configured

#### Example
```lua
--print stored access point info
do
  for k,v in pairs(wifi.sta.getapinfo()) do
    if (type(v)=="table") then
      print(" "..k.." : "..type(v))
      for k,v in pairs(v) do
        print("\t\t"..k.." : "..v)
      end
    else
      print(" "..k.." : "..v)
    end
  end
end

--print stored access point info(formatted)
do
  local x=wifi.sta.getapinfo()
  local y=wifi.sta.getapindex()
  print("\n Number of APs stored in flash:", x.qty)
  print(string.format("  %-6s %-32s %-64s %-18s", "index:", "SSID:", "Password:", "BSSID:")) 
  for i=1, (x.qty), 1 do
    print(string.format(" %s%-6d %-32s %-64s %-18s",(i==y and ">" or " "), i, x[i].ssid, x[i].pwd and x[i].pwd or type(nil), x[i].bssid and x[i].bssid or type(nil)))
  end
end
```

#### See also
- [`wifi.sta.getapindex()`](#wifistagetapindex)
- [`wifi.sta.setaplimit()`](#wifistasetaplimit)
- [`wifi.sta.changeap()`](#wifistachangeap)
- [`wifi.sta.config()`](#wifistaconfig)

## wifi.sta.getbroadcast()

Gets the broadcast address in station mode.

#### Syntax
`wifi.sta.getbroadcast()`

#### Parameters
`nil`

#### Returns
broadcast address as string, for example "192.168.0.255",
returns `nil` if IP address = "0.0.0.0".

#### See also
[`wifi.sta.getip()`](#wifistagetip)

## wifi.sta.getconfig()

Gets the WiFi station configuration.

#### Syntax
`wifi.sta.getconfig()`

#### Parameters
- `return_table`
	- `true` returns data in a table
	- `false` returns data in the old format (default)

#### Returns
If `return_table` is `true`:

- `config_table`
	- `ssid` ssid of Access Point.
	- `pwd` password to Access Point, `nil` if no password was configured 
	- `bssid` MAC address of Access Point, `nil` if no MAC address was configured 

If `return_table` is `false`:

- ssid, password, bssid_set, bssid, if `bssid_set` is equal to `0` then `bssid` is irrelevant

#### Example

```lua
--Get current Station configuration (NEW FORMAT)
do
local def_sta_config=wifi.sta.getconfig(true)
print(string.format("\tDefault station config\n\tssid:\"%s\"\tpassword:\"%s\"%s", def_sta_config.ssid, def_sta_config.pwd, (type(def_sta_config.bssid)=="string" and "\tbssid:\""..def_sta_config.bssid.."\"" or "")))
end

--Get current Station configuration (OLD FORMAT)
ssid, password, bssid_set, bssid=wifi.sta.getconfig()
print("\nCurrent Station configuration:\nSSID : "..ssid
.."\nPassword  : "..password
.."\nBSSID_set  : "..bssid_set
.."\nBSSID: "..bssid.."\n")
ssid, password, bssid_set, bssid=nil, nil, nil, nil
```

#### See also
- [`wifi.sta.getdefaultconfig()`](#wifistagetdefaultconfig)
- [`wifi.sta.connect()`](#wifistaconnect)
- [`wifi.sta.disconnect()`](#wifistadisconnect)

## wifi.sta.getdefaultconfig()

Gets the default WiFi station configuration stored in flash.

#### Syntax
`wifi.sta.getdefaultconfig(return_table)`

#### Parameters
- `return_table`
	- `true` returns data in a table
	- `false` returns data in the old format (default)

#### Returns
If `return_table` is `true`:

- `config_table`
	- `ssid` ssid of Access Point.
	- `pwd` password to Access Point, `nil` if no password was configured
	- `bssid` MAC address of Access Point, `nil` if no MAC address was configured

If `return_table` is `false`:

- ssid, password, bssid_set, bssid, if `bssid_set` is equal to `0` then `bssid` is irrelevant

#### Example

```lua
--Get default Station configuration (NEW FORMAT)
do
  local def_sta_config=wifi.sta.getdefaultconfig(true)
  print(string.format("\tDefault station config\n\tssid:\"%s\"\tpassword:\"%s\"%s", def_sta_config.ssid, def_sta_config.pwd, (type(def_sta_config.bssid)=="string" and "\tbssid:\""..def_sta_config.bssid.."\"" or "")))
end

--Get default Station configuration (OLD FORMAT)
ssid, password, bssid_set, bssid=wifi.sta.getdefaultconfig()
print("\nCurrent Station configuration:\nSSID : "..ssid
.."\nPassword  : "..password
.."\nBSSID_set  : "..bssid_set
.."\nBSSID: "..bssid.."\n")
ssid, password, bssid_set, bssid=nil, nil, nil, nil
```

#### See also
- [`wifi.sta.getconfig()`](#wifistagetconfig)
- [`wifi.sta.connect()`](#wifistaconnect)
- [`wifi.sta.disconnect()`](#wifistadisconnect)

## wifi.sta.gethostname()

Gets current station hostname.

#### Syntax
`wifi.sta.gethostname()`

#### Parameters
none

#### Returns
currently configured hostname

#### Example
```lua
print("Current hostname is: \""..wifi.sta.gethostname().."\"")
```

## wifi.sta.getip()

Gets IP address, netmask, and gateway address in station mode.

#### Syntax
`wifi.sta.getip()`

#### Parameters
none

#### Returns
IP address, netmask, gateway address as string, for example "192.168.0.111". Returns `nil` if IP = "0.0.0.0".

#### Example
```lua
-- print current IP address, netmask, gateway
print(wifi.sta.getip())
-- 192.168.0.111  255.255.255.0  192.168.0.1
ip = wifi.sta.getip()
print(ip)
-- 192.168.0.111
ip, nm = wifi.sta.getip()
print(nm)
-- 255.255.255.0
```

#### See also
[`wifi.sta.getmac()`](#wifistagetmac)

## wifi.sta.getmac()

Gets MAC address in station mode.

#### Syntax
`wifi.sta.getmac()`

#### Parameters
none

#### Returns
MAC address as string e.g. "18:fe:34:a2:d7:34"

#### See also
[`wifi.sta.getip()`](#wifistagetip)


## wifi.sta.getrssi()

Get RSSI(Received Signal Strength Indicator) of the Access Point which ESP8266 station connected to.

#### Syntax
`wifi.sta.getrssi()`

#### Parameters
none

#### Returns
- If station is connected to an access point, `rssi` is returned.
- If station is not connected to an access point, `nil` is returned.  

#### Example
```lua
RSSI=wifi.sta.getrssi()
print("RSSI is", RSSI)
```

## wifi.sta.setaplimit()

Set Maximum number of Access Points to store in flash.
 - This value is written to flash

!!! Attention
		If 5 Access Points are stored and AP limit is set to 4, the AP at index 5 will remain until [`node.restore()`](node.md#noderestore) is called or AP limit is set to 5 and AP is overwritten.  

#### Syntax
`wifi.sta.setaplimit(qty)`

#### Parameters
`qty` Quantity of Access Points to store in flash. Range: 1-5 (Default: 5)

#### Returns
- `true`  Success
- `false` Failure

#### Example
```lua
wifi.sta.setaplimit(true)
```

#### See also
- [`wifi.sta.getapinfo()`](#wifistagetapinfo)

## wifi.sta.sethostname()

Sets station hostname.

#### Syntax
`wifi.sta.sethostname(hostname)`

#### Parameters
`hostname` must only contain letters, numbers and hyphens('-') and be 32 characters or less with first and last character being alphanumeric

#### Returns
`nil`

#### Example
```lua
if (wifi.sta.sethostname("NodeMCU") == true) then
	print("hostname was successfully changed")
else
	print("hostname was not changed")
end
```

## wifi.sta.setip()
Sets IP address, netmask, gateway address in station mode.

#### Syntax
`wifi.sta.setip(cfg)`

#### Parameters
`cfg` table contain IP address, netmask, and gateway
```lua
{
  ip = "192.168.0.111",
  netmask = "255.255.255.0",
  gateway = "192.168.0.1"
}
```

#### Returns
true if success, false otherwise

#### See also
[`wifi.sta.setmac()`](#wifistasetmac)

## wifi.sta.setmac()

Sets MAC address in station mode.

#### Syntax
`wifi.sta.setmac(mac)`

#### Parameters
MAC address in string e.g. "DE:AD:BE:EF:7A:C0"

#### Returns
true if success, false otherwise

#### Example
```lua
print(wifi.sta.setmac("DE:AD:BE:EF:7A:C0"))
```

#### See also
[`wifi.sta.setip()`](#wifistasetip)

## wifi.sta.sleeptype()

Configures the WiFi modem sleep type to be used while station is connected to an Access Point.

!!! note
	Does not apply to `wifi.SOFTAP`, `wifi.STATIONAP` or `wifi.NULLMODE`.

#### Syntax
`wifi.sta.sleeptype(type_wanted)`

#### Parameters
`type_wanted` one of the following:

- `wifi.NONE_SLEEP` to keep the modem on at all times
- `wifi.LIGHT_SLEEP` to allow the CPU to power down under some circumstances
- `wifi.MODEM_SLEEP` to power down the modem as much as possible

#### Returns
The actual sleep mode set, as one of `wifi.NONE_SLEEP`, `wifi.LIGHT_SLEEP` or `wifi.MODEM_SLEEP`.

## wifi.sta.status()

Gets the current status in station mode.

#### Syntax
`wifi.sta.status()`

#### Parameters
`nil`

#### Returns
number： 0~5

- 0: STA_IDLE,
- 1: STA_CONNECTING,
- 2: STA_WRONGPWD,
- 3: STA_APNOTFOUND,
- 4: STA_FAIL,
- 5: STA_GOTIP.

# wifi.ap Module

## wifi.ap.config()

Sets SSID and password in AP mode. Be sure to make the password at least 8 characters long! If you don't it will default to *no* password and not set the SSID! It will still work as an access point but use a default SSID like e.g. NODE-9997C3.

#### Syntax
`wifi.ap.config(cfg)`

#### Parameters
- `cfg` table to hold configuration
	- `ssid` SSID chars 1-32
	- `pwd` password chars 8-64
	- `auth` authentication method, one of `wifi.OPEN` (default), `wifi.WPA_PSK`, `wifi.WPA2_PSK`, `wifi.WPA_WPA2_PSK`
	- `channel` channel number 1-14 default = 6
	- `hidden` false = not hidden, true = hidden, default = false
	- `max` maximum number of connections 1-4 default=4
	- `beacon` beacon interval time in range 100-60000, default = 100
	- `save` save configuration to flash.
		- `true` configuration **will** be retained through power cycle. (Default)
		- `false` configuration **will not** be retained through power cycle.
 

#### Returns
- `true`  Success
- `false` Failure

#### Example:
```lua
 cfg={}
 cfg.ssid="myssid"
 cfg.pwd="mypassword"
 wifi.ap.config(cfg)
```

## wifi.ap.deauth()

Deauths (forcibly removes) a client from the ESP access point by sending a corresponding IEEE802.11 management packet (first) and removing the client from it's data structures (afterwards).

The IEEE802.11 reason code used is 2 for "Previous authentication no longer valid"(AUTH_EXPIRE).

#### Syntax
`wifi.ap.deauth([MAC])`

#### Parameters
- `MAC` address of station to be deauthed.
	- Note: if this field is left blank, all currently connected stations will get deauthed.

#### Returns
Returns true unless called while the ESP is in the STATION opmode

#### Example
```lua
allowed_mac_list={"18:fe:34:00:00:00", "18:fe:34:00:00:01"}

wifi.eventmon.register(wifi.eventmon.AP_STACONNECTED, function(T)
  print("\n\tAP - STATION CONNECTED".."\n\tMAC: "..T.MAC.."\n\tAID: "..T.AID)
  if(allowed_mac_list~=nil) then
    for _, v in pairs(allowed_mac_list) do
      if(v == T.MAC) then return end
    end
  end
  wifi.ap.deauth(T.MAC)
  print("\tStation DeAuthed!")
end)

```

#### See also
[`wifi.eventmon.register()`](#wifieventmonregister)  
[`wifi.eventmon.reason()`](#wifieventmonreason)

## wifi.ap.getbroadcast()

Gets broadcast address in AP mode.

#### Syntax
`wifi.ap.getbroadcast()`

#### Parameters
none

#### Returns
broadcast address in string, for example "192.168.0.255",
returns `nil` if IP address = "0.0.0.0".

#### Example
```lua
bc = wifi.ap.getbroadcast()
print(bc)
-- 192.168.0.255
```

#### See also
[`wifi.ap.getip()`](#wifiapgetip)

## wifi.ap.getclient()

Gets table of clients connected to device in AP mode.

#### Syntax
`wifi.ap.getclient()`

#### Parameters
none

#### Returns
table of connected clients

#### Example
```lua
table={}
table=wifi.ap.getclient()
for mac,ip in pairs(table) do
	print(mac,ip)
end

-- or shorter
for mac,ip in pairs(wifi.ap.getclient()) do
	print(mac,ip)
end
```

## wifi.ap.getconfig()

Gets the current SoftAP configuration.

#### Syntax
`wifi.ap.getconfig(return_table)`

#### Parameters
- `return_table`
	- `true` returns data in a table
	- `false` returns data in the old format (default)

#### Returns
If `return_table` is true:

- `config_table`
	- `ssid` Network name
	- `pwd` Password, `nil` if no password was configured	- `auth` Authentication Method (`wifi.OPEN`, `wifi.WPA_PSK`, `wifi.WPA2_PSK` or `wifi.WPA_WPA2_PSK`)
	- `channel` Channel number
	- `hidden` `false` = not hidden, `true` = hidden
	- `max` Maximum number of client connections
	- `beacon` Beacon interval

If `return_table` is false:

- ssid, password, if `bssid_set` is equal to 0 then `bssid` is irrelevant

#### Example

```lua
--Get SoftAP configuration table (NEW FORMAT)
do
  print("\n  Current SoftAP configuration:")
  for k,v in pairs(wifi.ap.getconfig(true)) do
      print("   "..k.." :",v)
  end
end

--Get current SoftAP configuration (OLD FORMAT)
do
  local ssid, password=wifi.ap.getconfig()
  print("\n  Current SoftAP configuration:\n   SSID : "..ssid..
    "\n   Password  :",password)
  ssid, password=nil, nil
end
```

## wifi.ap.getdefaultconfig()

Gets the default SoftAP configuration stored in flash.

#### Syntax
`wifi.ap.getdefaultconfig(return_table)`

#### Parameters
- `return_table`
	- `true` returns data in a table
	- `false` returns data in the old format (default)

#### Returns
If `return_table` is true:

- `config_table`
	- `ssid` Network name
	- `pwd` Password, `nil` if no password was configured	- `auth` Authentication Method (`wifi.OPEN`, `wifi.WPA_PSK`, `wifi.WPA2_PSK` or `wifi.WPA_WPA2_PSK`)
	- `channel` Channel number
	- `hidden` `false` = not hidden, `true` = hidden
	- `max` Maximum number of client connections
	- `beacon` Beacon interval

If `return_table` is false:

- ssid, password, if `bssid_set` is equal to 0 then `bssid` is irrelevant

#### Example

```lua
--Get default SoftAP configuration table (NEW FORMAT)
do
  print("\n  Default SoftAP configuration:")
  for k,v in pairs(wifi.ap.getdefaultconfig(true)) do
      print("   "..k.." :",v)
  end
end

--Get default SoftAP configuration (OLD FORMAT)
do
  local ssid, password=wifi.ap.getdefaultconfig()
  print("\n  Default SoftAP configuration:\n   SSID : "..ssid..
    "\n   Password  :",password)
  ssid, password=nil, nil
end
```

## wifi.ap.getip()

Gets IP address, netmask and gateway in AP mode.

#### Syntax
`wifi.ap.getip()`

#### Parameters
none

#### Returns
IP address, netmask, gateway address as string, for example "192.168.0.111", returns `nil` if IP address = "0.0.0.0".

#### Example

```lua
-- print current ip, netmask, gateway
print(wifi.ap.getip())
-- 192.168.4.1  255.255.255.0  192.168.4.1
ip = wifi.ap.getip()
print(ip)
-- 192.168.4.1
ip, nm = wifi.ap.getip()
print(nm)
-- 255.255.255.0
ip, nm, gw = wifi.ap.getip()
print(gw)
-- 192.168.4.1
```

#### See also
- [`wifi.ap.getmac()`](#wifiapgetmac)

## wifi.ap.getmac()

Gets MAC address in AP mode.

#### Syntax
`wifi.ap.getmac()`

#### Parameters
none

#### Returns
MAC address as string, for example "1A-33-44-FE-55-BB"

#### See also
[`wifi.ap.getip()`](#wifiapgetip)

## wifi.ap.setip()

Sets IP address, netmask and gateway address in AP mode.

#### Syntax
`wifi.ap.setip(cfg)`

#### Parameters
`cfg` table contain IP address, netmask, and gateway

#### Returns
true if successful, false otherwise

#### Example

```lua
cfg =
{
	ip="192.168.1.1",
	netmask="255.255.255.0",
	gateway="192.168.1.1"
}
wifi.ap.setip(cfg)
```

#### See also
[`wifi.ap.setmac()`](#wifiapsetmac)

## wifi.ap.setmac()

Sets MAC address in AP mode.

#### Syntax
`wifi.ap.setmac(mac)`

#### Parameters
MAC address in byte string, for example "AC-1D-1C-B1-0B-22"

#### Returns
true if success, false otherwise

#### Example
```lua
print(wifi.ap.setmac("AC-1D-1C-B1-0B-22"))
```

#### See also
[`wifi.ap.setip()`](#wifiapsetip)

# wifi.ap.dhcp Module

## wifi.ap.dhcp.config()

Configure the dhcp service. Currently only supports setting the start address of the dhcp address pool.

#### Syntax
`wifi.ap.dhcp.config(dhcp_config)`

#### Parameters
`dhcp_config` table containing the start-IP of the DHCP address pool, eg. "192.168.1.100"

#### Returns
`pool_startip`, `pool_endip`

#### Example
```lua
dhcp_config ={}
dhcp_config.start = "192.168.1.100"
wifi.ap.dhcp.config(dhcp_config)
```

## wifi.ap.dhcp.start()

Starts the DHCP service.

#### Syntax
`wifi.ap.dhcp.start()`

#### Parameters
none

#### Returns
boolean indicating success

## wifi.ap.dhcp.stop()

Stops the DHCP service.

#### Syntax
`wifi.ap.dhcp.stop()`

#### Parameters
none

#### Returns
boolean indicating success

# wifi.eventmon Module
Note: The functions `wifi.sta.eventMon___()` and `wifi.eventmon.___()` are completely seperate and can be used independently of one another.

## wifi.eventmon.register()

Register/unregister callbacks for WiFi event monitor.

#### Syntax
wifi.eventmon.register(Event[, function(T)])

#### Parameters
Event: WiFi event you would like to set a callback for.  

- Valid WiFi events:  
 	- wifi.eventmon.STA_CONNECTED  
	- wifi.eventmon.STA_DISCONNECTED  
	- wifi.eventmon.STA_AUTHMODE_CHANGE  
	- wifi.eventmon.STA_GOT_IP  
	- wifi.eventmon.STA_DHCP_TIMEOUT  
	- wifi.eventmon.AP_STACONNECTED  
	- wifi.eventmon.AP_STADISCONNECTED  
	- wifi.eventmon.AP_PROBEREQRECVED  

#### Returns
Function:  
`nil`

Callback:  
T: Table returned by event.  

- `wifi.eventmon.STA_CONNECTED` Station is connected to access point.  
	- `SSID`: SSID of access point.  
	- `BSSID`: BSSID of access point.  
	- `channel`: The channel the access point is on.  
- `wifi.eventmon.STA_DISCONNECTED`: Station was disconnected from access point.  
	- `SSID`: SSID of access point.  
	- `BSSID`: BSSID of access point.  
	- `REASON`: See [wifi.eventmon.reason](#wifieventmonreason) below.  
- `wifi.eventmon.STA_AUTHMODE_CHANGE`: Access point has changed authorization mode.    
	- `old_auth_mode`: Old wifi authorization mode.  
	- `new_auth_mode`: New wifi authorization mode.  
- `wifi.eventmon.STA_GOT_IP`: Station got an IP address.  
	- `IP`: The IP address assigned to the station.  
	- `netmask`: Subnet mask.  
	- `gateway`: The IP address of the access point the station is connected to.  
- `wifi.eventmon.STA_DHCP_TIMEOUT`: Station DHCP request has timed out.  
	- Blank table is returned.  
- `wifi.eventmon.AP_STACONNECTED`: A new client has connected to the access point.  
	- `MAC`: MAC address of client that has connected.  
	- `AID`: SDK provides no details concerning this return value.  
- `wifi.eventmon.AP_STADISCONNECTED`: A client has disconnected from the access point.  
	- `MAC`: MAC address of client that has disconnected.  
	- `AID`: SDK provides no details concerning this return value.  
- `wifi.eventmon.AP_PROBEREQRECVED`: A probe request was received.  
	- `MAC`: MAC address of the client that is probing the access point.  
	- `RSSI`: Received Signal Strength Indicator of client.  

#### Example

```lua
 wifi.eventmon.register(wifi.eventmon.STA_CONNECTED, function(T)
 print("\n\tSTA - CONNECTED".."\n\tSSID: "..T.SSID.."\n\tBSSID: "..
 T.BSSID.."\n\tChannel: "..T.channel)
 end)

 wifi.eventmon.register(wifi.eventmon.STA_DISCONNECTED, function(T)
 print("\n\tSTA - DISCONNECTED".."\n\tSSID: "..T.SSID.."\n\tBSSID: "..
 T.BSSID.."\n\treason: "..T.reason)
 end)

 wifi.eventmon.register(wifi.eventmon.STA_AUTHMODE_CHANGE, Function(T)
 print("\n\tSTA - AUTHMODE CHANGE".."\n\told_auth_mode: "..
 T.old_auth_mode.."\n\tnew_auth_mode: "..T.new_auth_mode)
 end)

 wifi.eventmon.register(wifi.eventmon.STA_GOT_IP, function(T)
 print("\n\tSTA - GOT IP".."\n\tStation IP: "..T.IP.."\n\tSubnet mask: "..
 T.netmask.."\n\tGateway IP: "..T.gateway)
 end)

 wifi.eventmon.register(wifi.eventmon.STA_DHCP_TIMEOUT, function()
 print("\n\tSTA - DHCP TIMEOUT")
 end)

 wifi.eventmon.register(wifi.eventmon.AP_STACONNECTED, function(T)
 print("\n\tAP - STATION CONNECTED".."\n\tMAC: "..T.MAC.."\n\tAID: "..T.AID)
 end)

 wifi.eventmon.register(wifi.eventmon.AP_STADISCONNECTED, function(T)
 print("\n\tAP - STATION DISCONNECTED".."\n\tMAC: "..T.MAC.."\n\tAID: "..T.AID)
 end)

 wifi.eventmon.register(wifi.eventmon.AP_PROBEREQRECVED, function(T)
 print("\n\tAP - STATION DISCONNECTED".."\n\tMAC: ".. T.MAC.."\n\tRSSI: "..T.RSSI)
 end)
```
#### See also
- [`wifi.eventmon.unregister()`](#wifieventmonunregister)
- [`wifi.sta.eventMonStart()`](#wifistaeventmonstart)
- [`wifi.sta.eventMonStop()`](#wifistaeventmonstop)
- [`wifi.sta.eventMonReg()`](#wifistaeventmonreg)

## wifi.eventmon.unregister()

Unregister callbacks for WiFi event monitor.

#### Syntax
wifi.eventmon.unregister(Event)

#### Parameters
Event: WiFi event you would like to set a callback for.  

- Valid WiFi events:
	- wifi.eventmon.STA_CONNECTED  
	- wifi.eventmon.STA_DISCONNECTED  
	- wifi.eventmon.STA_AUTHMODE_CHANGE  
	- wifi.eventmon.STA_GOT_IP  
	- wifi.eventmon.STA_DHCP_TIMEOUT  
	- wifi.eventmon.AP_STACONNECTED  
	- wifi.eventmon.AP_STADISCONNECTED  
	- wifi.eventmon.AP_PROBEREQRECVED  

#### Returns
`nil`

#### Example

```lua
 wifi.eventmon.unregister(wifi.eventmon.STA_CONNECTED)
```
#### See also
- [`wifi.eventmon.register()`](#wifieventmonregister)
- [`wifi.sta.eventMonStart()`](#wifistaeventmonstart)
- [`wifi.sta.eventMonStop()`](#wifistaeventmonstop)

## wifi.eventmon.reason

Table containing disconnect reasons.

|  Disconnect reason  |  value  |
|:--------------------|:-------:|
|wifi.eventmon.reason.UNSPECIFIED   |  1  |
|wifi.eventmon.reason.AUTH_EXPIRE   |  2  |				
|wifi.eventmon.reason.AUTH_LEAVE    |  3  |
|wifi.eventmon.reason.ASSOC_EXPIRE  |  4  |
|wifi.eventmon.reason.ASSOC_TOOMANY |  5  |
|wifi.eventmon.reason.NOT_AUTHED    |  6  |
|wifi.eventmon.reason.NOT_ASSOCED   |  7  |
|wifi.eventmon.reason.ASSOC_LEAVE   |  8  |
|wifi.eventmon.reason.ASSOC_NOT_AUTHED     |  9  |
|wifi.eventmon.reason.DISASSOC_PWRCAP_BAD  |  10  |
|wifi.eventmon.reason.DISASSOC_SUPCHAN_BAD |  11  |
|wifi.eventmon.reason.IE_INVALID    |  13  |
|wifi.eventmon.reason.MIC_FAILURE   |  14  |
|wifi.eventmon.reason.4WAY_HANDSHAKE_TIMEOUT   |  15  |
|wifi.eventmon.reason.GROUP_KEY_UPDATE_TIMEOUT |  16  |
|wifi.eventmon.reason.IE_IN_4WAY_DIFFERS       |  17  |
|wifi.eventmon.reason.GROUP_CIPHER_INVALID     |  18  |
|wifi.eventmon.reason.PAIRWISE_CIPHER_INVALID  |  19  |
|wifi.eventmon.reason.AKMP_INVALID          |  20  |
|wifi.eventmon.reason.UNSUPP_RSN_IE_VERSION |  21  |
|wifi.eventmon.reason.INVALID_RSN_IE_CAP    |  22  |
|wifi.eventmon.reason.802_1X_AUTH_FAILED    |  23  |
|wifi.eventmon.reason.CIPHER_SUITE_REJECTED |  24  |
|wifi.eventmon.reason.BEACON_TIMEOUT    |  200  |
|wifi.eventmon.reason.NO_AP_FOUND       |  201  |
|wifi.eventmon.reason.AUTH_FAIL         |  202  |
|wifi.eventmon.reason.ASSOC_FAIL        |  203  |
|wifi.eventmon.reason.HANDSHAKE_TIMEOUT |  204  |

# WiFi Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-05-12 | [Zeroday](https://github.com/funshine) | [dnc40085](https://github.com/dnc40085) | [wifi.c](../../../app/modules/wifi.c)|

The NodeMCU WiFi control is spread across several tables:

- `wifi` for overall WiFi configuration
- [`wifi.sta`](#wifista-module) for station mode functions
- [`wifi.ap`](#wifiap-module) for wireless access point (WAP or simply AP) functions
- [`wifi.ap.dhcp`](#wifiapdhcp-module) for DHCP server control

## wifi.getchannel()

Gets the current WiFi channel.

#### Syntax
`wifi.getchannel()`

#### Parameters
`nil`

#### Returns
current WiFi channel

#### Example
```lua
print(wifi.getchannel())
```

## wifi.getmode()

Gets WiFi operation mode.

#### Syntax
`wifi.getmode()`

#### Parameters
`nil`

#### Returns
The WiFi mode, as one of the `wifi.STATION`, `wifi.SOFTAP`, `wifi.STATIONAP` or `wifi.NULLMODE` constants.

#### See also
[`wifi.setmode()`](#wifisetmode)

## wifi.getphymode()

Gets WiFi physical mode.

#### Syntax
`wifi.getpymode()`

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

#### Syntax
`wifi.setmode(mode)`

#### Parameters
`mode` value should be one of

- `wifi.STATION` for when the device is connected to a WiFi router. This is often done to give the device access to the Internet.
- `wifi.SOFTAP` for when the device is acting *only* as an access point. This will allow you to see the device in the list of WiFi networks (unless you hide the SSID, of course). In this mode your computer can connect to the device, creating a local area network. Unless you change the value, the NodeMCU device will be given a local IP address of 192.168.4.1 and assign your computer the next available IP address, such as 192.168.4.2.
- `wifi.STATIONAP` is the combination of `wifi.STATION` and `wifi.SOFTAP`. It allows you to create a local WiFi connection *and* connect to another WiFi router.
- `wifi.NULLMODE` to switch off WiFi

#### Returns
current mode after setup

#### Example
```lua
wifi.setmode(wifi.STATION)
```

#### See also
[`wifi.getmode()`](#wifigetmode)

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

## wifi.sleeptype()

Configures the WiFi modem sleep type.

#### Syntax
`wifi.sleeptype(type_wanted)`

#### Parameters
`type_wanted` one of the following:

- `wifi.NONE_SLEEP` to keep the modem on at all times
- `wifi.LIGHT_SLEEP` to allow the modem to power down under some circumstances
- `wifi.MODEM_SLEEP` to power down the modem as much as possible

#### Returns
The actual sleep mode set, as one of `wifi.NONE_SLEEP`, `wifi.LIGHT_SLEEP` or `wifi.MODEM_SLEEP`.

#### See also
- [`node.dsleep()`](node.md#nodedsleep)
- [`rtctime.dsleep()`](rtctime.md#rtctimedsleep)

## wifi.startsmart()

Starts to auto configuration, if success set up SSID and password automatically.

Intended for use with SmartConfig apps, such as Espressif's [Android & iOS app](https://github.com/espressifapp).

Only usable in `wifi.STATION` mode.

!!! note "Note:"

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

## wifi.sta.config()

Sets the WiFi station configuration.

#### Syntax
`wifi.sta.config(ssid, password[, auto[, bssid]])`

#### Parameters

- `ssid` string which is less than 32 bytes.
- `password` string which is 8-64 or 0 bytes. Empty string indicates an open WiFi access point.
- `auto` value of 0 or 1 (default)
	- 0, Disable auto connect and remain disconnected from access point
	- 1, Enable auto connect and connect to access point
- `bssid` string that contains the MAC address of the access point (optional)
	- You can set BSSID if you have multiple access points with the same SSID.
 	- Note: if you set BSSID for a specific SSID and would like to configure station to connect to the same SSID only without the BSSID requirement, you MUST first configure to station to a different SSID first, then connect to the desired SSID
 	- The following formats are valid:
		- "DE-C1-A5-51-F1-ED"
		- "AC-1D-1C-B1-0B-22"
		- "DE AD BE EF 7A C0"

#### Returns
`nil`

#### Example

```lua
--Connect to access point automatically when in range
wifi.sta.config("myssid", "password")

--Connect to Unsecured access point automatically when in range
wifi.sta.config("myssid", "")
  
--Connect to access point, User decides when to connect/disconnect to/from AP
wifi.sta.config("myssid", "mypassword", 0)
wifi.sta.connect()
-- ... do some WiFi stuff
wifi.sta.disconnect()
   
--Connect to specific access point automatically when in range
wifi.sta.config("myssid", "mypassword", "12:34:56:78:90:12")

--Connect to specific access point, User decides when to connect/disconnect to/from AP
wifi.sta.config("myssid", "mypassword", 0, "12:34:56:78:90:12")
wifi.sta.connect()
-- ... do some WiFi stuff
wifi.sta.disconnect()
```

#### See also
- [`wifi.sta.connect()`](#wifistaconnect)
- [`wifi.sta.disconnect()`](#wifistadisconnect)

## wifi.sta.connect()

Connects to AP in station mode.

#### Syntax
`wifi.sta.connect()`

#### Parameters
none

#### Returns
`nil`

#### Example
```lua
wifi.sta.connect()
```

#### See also
- [`wifi.sta.disconnect()`](#wifistadisconnect)
- [`wifi.sta.config()`](#wifistaconfig)

## wifi.sta.disconnect()

Disconnects from AP in station mode.

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
- `wifi.sta.eventMonReg(wifi_status, function([previous_state]))`
- `wifi.sta.eventMonReg(wifi.status, "unreg")`

####  Parameters
- `wifi_status` WiFi status you would like to set callback for, one of: 
    - `wifi.STA_IDLE`
    - `wifi.STA_CONNECTING`
    - `wifi.STA_WRONGPWD`
    - `wifi.STA_APNOTFOUND`
    - `wifi.STA_FAIL`
    - `wifi.STA_GOTIP`
- `function` function to perform when event occurs
- `"unreg"` unregister previously registered callback
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
wifi.sta.eventMonReg(wifi.STA_IDLE, "unreg")
```
#### See also
- [`wifi.sta.eventMonStart()`](#wifistaeventmonstart)
- [`wifi.sta.eventMonStop()`](#wifistaeventmonstop)

## wifi.sta.eventMonStart()

Starts WiFi station event monitor.

####  Syntax
`wifi.sta.eventMonStart([ms])`

### Parameters
`ms` interval between checks in milliseconds, defaults to 150ms if not provided

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
- 
## wifi.sta.eventMonStop()

Stops WiFi station event monitor.
####  Syntax
`wifi.sta.eventMonStop(["unreg all"])`

####  Parameters
`"unreg all"` unregister all previously registered functions

####  Returns
`nil`

####  Example
```lua
--stop WiFi event monitor
wifi.sta.eventMonStop()

--stop WiFi event monitor and unregister all callbacks
wifi.sta.eventMonStop("unreg all")
```

#### See also
- [`wifi.sta.eventMonReg()`](#wifistaeventmonreg)
- [`wifi.sta.eventMonStart()`](#wifistaeventmonstart)

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
none

#### Returns
ssid, password, bssid_set, bssid

Note: If bssid_set is equal to 0 then bssid is irrelevant 

#### Example

```lua
--Get current Station configuration
ssid, password, bssid_set, bssid=wifi.sta.getconfig()
print("\nCurrent Station configuration:\nSSID : "..ssid
.."\nPassword  : "..password
.."\nBSSID_set  : "..bssid_set
.."\nBSSID: "..bssid.."\n")
ssid, password, bssid_set, bssid=nil, nil, nil, nil
```

#### See also
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
MAC address as string e.g. "18-33-44-FE-55-BB"

#### See also
[`wifi.sta.getip()`](#wifistagetip)

## wifi.sta.sethostname()

Sets station hostname.

#### Syntax
`wifi.sta.sethostname(hostname)`

#### Parameters
`hostname` must only contain letters, numbers and hyphens('-') and be 32 characters or less with first and last character being alphanumeric

#### Returns
true if hostname was successfully set, false otherwise

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

## wifi.sta.status()

Gets the current status in station mode.

#### Syntax
`wifi.sta.status()`

#### Parameters
`nil`

#### Returns
number： 0~5

- 0: STATION_IDLE,
- 1: STATION_CONNECTING,
- 2: STATION_WRONG_PASSWORD,
- 3: STATION_NO_AP_FOUND,
- 4: STATION_CONNECT_FAIL,
- 5: STATION_GOT_IP.

# wifi.ap Module

## wifi.ap.config()

Sets SSID and password in AP mode. Be sure to make the password at least 8 characters long! If you don't it will default to *no* password and not set the SSID! It will still work as an access point but use a default SSID like e.g. ESP_9997C3.

#### Syntax
`wifi.ap.config(cfg)`

#### Parameters
- `ssdi` SSID chars 1-32
- `pwd` password chars 8-64
- `auth` authentication  one of AUTH\_OPEN, AUTH\_WPA\_PSK, AUTH\_WPA2\_PSK, AUTH\_WPA\_WPA2\_PSK, default = AUTH\_OPEN
- `channel` channel number 1-14 default = 6
- `hidden` 0 = not hidden, 1 = hidden, default 0
- `max` maximal number of connections 1-4 default=4
- `beacon` beacon interval time in range 100-60000, default = 100

#### Returns
`nil`

#### Example:
```lua
 cfg={}
 cfg.ssid="myssid"
 cfg.pwd="mypassword"
 wifi.ap.config(cfg)
```

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

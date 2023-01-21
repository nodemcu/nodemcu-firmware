# WiFi Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-10-03 | [Johny Mattsson](https://github.com/jmattsson) |[Johny Mattsson](https://github.com/jmattsson) | [wifi.c](../../components/modules/wifi.c)|


The NodeMCU WiFi control is spread across several tables:

- `wifi` for overall WiFi configuration
- [`wifi.sta`](#wifista-module) for station mode functions
- [`wifi.ap`](#wifiap-module) for access point (AP) functions

## wifi.getchannel()

Gets the current WiFi channel.

#### Syntax
`wifi.getchannel()`

#### Parameters
`nil`

#### Returns
- current WiFi channel (primary channel)
- HT20/HT40 information (secondary channel). One of the constants:
    - `wifi.HT20 `
    - `wifi.HT40_ABOVE`
    - `wifi.HT40_BELOW`

## wifi.getmode()


Gets WiFi operation mode.

#### Syntax
`wifi.getmode()`

#### Parameters
`nil`

#### Returns
The WiFi mode, as one of the `wifi.STATION`, `wifi.SOFTAP`, `wifi.STATIONAP` or `wifi.NULLMODE` constants.

#### See also
[`wifi.mode()`](#wifimode)


## wifi.mode()

Configures the WiFi mode to use. NodeMCU can run in one of four WiFi modes:

- Station mode, where the NodeMCU device joins an existing network
- Access point (AP) mode, where it creates its own network that others can join
- Station + AP mode, where it both creates its own network while at the same time being joined to another existing network
- WiFi off

When using the combined Station + AP mode, the same channel will be used for both networks as the radio can only listen on a single channel.

#### Syntax
`wifi.mode(mode[, save])`

#### Parameters
- `mode` value should be one of
    - `wifi.STATION` for when the device is connected to a WiFi router. This is often done to give the device access to the Internet.
    - `wifi.SOFTAP` for when the device is acting *only* as an access point. This will allow you to see the device in the list of WiFi networks (unless you hide the SSID, of course). In this mode your computer can connect to the device, creating a local area network. Unless you change the value, the NodeMCU device will be given a local IP address of 192.168.4.1 and assign your computer the next available IP address, such as 192.168.4.2.
    - `wifi.STATIONAP` is the combination of `wifi.STATION` and `wifi.SOFTAP`. It allows you to create a local WiFi connection *and* connect to another WiFi router.
    - `wifi.NULLMODE` disables the WiFi interface(s). Use `wifi.stop()` to fully shut down the WiFi interface.
- `save` choose whether or not to save wifi mode to flash
    - `true` WiFi mode configuration **will** be retained through power cycle. (Default)
    - `false` WiFi mode configuration **will not** be retained through power cycle.

#### Returns
current mode after setup

#### Example
```lua
wifi.mode(wifi.STATION)
```

#### See also
[`wifi.getmode()`](#wifigetmode)
[`wifi.stop()`](#wifistop)


## wifi.start()

Starts the WiFi interface(s). On system startup the WiFi interface(s) are
not running. This is to enable users to choose whether to expend the power
necessary for radio comms. A sensor device running on battery might only
want to enable WiFi every 10th boot for example.

#### Syntax
`wifi.start()`

#### Parameters
`nil`

#### Returns
`nil`

#### See also
[`wifi.stop()`](#wifistop)


## wifi.stop()

Shuts down the WiFi interface(s).

#### Syntax
`wifi.stop()`

#### Parameters
`nil`

#### Returns
`nil`

#### See also
[`wifi.start()`](#wifistart)


# wifi.sta Module

## wifi.sta.config()

Sets the WiFi station configuration.

The WiFi mode must be set to `wifi.STATION` or `wifi.STATIONAP` before this
function can be used.

Note that the earlier auto-connect feature is no longer available due to
being removed in the SDK/IDF. After start-up it is necessary to call
[`wifi.stat.connect()`](#wifistaconnect) manually.

#### Syntax
`wifi.sta.config(station_config, save)`

#### Parameters
- `station_config` table containing configuration data for station
    - `ssid` string which is less than 32 bytes.
    - `pwd` string which is 8-64 or 0 bytes. Empty string indicates an open WiFi access point.
    - `bssid` string that contains the MAC address of the access point (optional)
        - You can set BSSID if you have multiple access points with the same SSID.
        - Note: if you set BSSID for a specific SSID and would like to configure station to connect to the same SSID only without the BSSID requirement, you MUST first configure to station to a different SSID first, then connect to the desired SSID
        - The following formats are valid:
            - "DE:C1:A5:51:F1:ED"
            - "AC-1D-1C-B1-0B-22"
            - "DE AD BE EF 7A C0"
        - "AcDc0123c0DE"
    - `pmf` an optional setting to control whether Protected Management Frames
      are supported and/or required. One of:
          - `wifi.sta.PMF_AVAILABLE`
          - `wifi.sta.PMF_REQUIRED`.
      Defaults to `wifi.sta.PMF_AVAILABLE`. PMF is required when joining to
      WPA3-Personal access points. The value `wifi.sta.PMF_OFF` is no longer
      available as it is no longer supported by the wifi stack.

    - `channel` optional integer value, the channel number to start scanning for the AP from, if known.
    - `scan_method` optional string value, one of `"fast"` or `"all"` do either do a fast scan or all channel scan when looking for the AP to connect to. With fast scan, the first found matching AP is used even if it is not the best/closest one.
    - `listen_interval` optional listen interval to receive beacon if max wifi power saving mode is enabled. Units is in AP beacon intervals. Defaults to 3.
    - `sort_by` optional string value for preferential selection of AP. Must be one of `"rssi"` or `"authmode"` if present.
    - `threshold_rssi` optional integer value to limit APs to only those which have a signal stronger than this value.
    - `threshold_authmode` optional value to limit APs to those with an authentication mode of at least this settings. One of `wifi.AUTH_OPEN`, `wifi.AUTH_WEP`, `wifi.AUTH_WPA_PSK`, `wifi.AUTH_WPA2_PSK`, `wifi.AUTH_WPA_WPA2_PSK`, `wifi.AUTH_WPA2_ENTERPRISE`, `wifi.AUTH_WPA3_PSK`, `wifi.AUTH_WPA2_WPA3_PSK`, `wifi.AUTH_WAPI_PSK`.
    - `rm` optional boolean, set to `true` to enable Radio Measurements
    - `btm` optional boolean, set to `true` to enable BSS Transition Management
    - `mbo` optional boolean, set to `true` to enable Multi-Band Operation
    - `sae_pwe` optional, configures WPA3 SAE Password Element setting. One of `wifi.SAE_PWE_UNSPECIFIED`, `wifi.SAE_PWE_HUNT_AND_PECK`, `wifi.SAE_PWE_HASH_TO_ELEMENT` or `wifi.SAE_PWE_BOTH`.

- `save` Save station configuration to flash. 
    - `true` configuration **will** be retained through power cycle. 
    - `false` configuration **will not** be retained through power cycle. (Default)

#### Returns
`nil`

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
wifi.sta.config(station_cfg, true)

--connect to Access Point with specific MAC address  
station_cfg={}
station_cfg.ssid="NODE-AABBCC"
station_cfg.pwd="password"
station_cfg.bssid="AA:BB:CC:DD:EE:FF"
wifi.sta.config(station_cfg)

```

#### See also
- [`wifi.sta.connect()`](#wifistaconnect)
- [`wifi.sta.disconnect()`](#wifistadisconnect)


## wifi.sta.getconfig()

Returns the current station configuration.

#### Syntax
`wifi.sta.getconfig()`

#### Parameters
`nil`

#### Returns
A table with the configuration settings. Refer to [`wifi.sta.config()`](#wifistaconfig) for field details.


## wifi.sta.connect()

Connects to the configured AP in station mode. You will want to call this
on start-up after [`wifi.start()`](#wifistart), and quite possibly also
in response to `disconnected` events.

#### Syntax
`wifi.sta.connect()`

#### Parameters
`nil`

#### Returns
`nil`

#### See also
- [`wifi.sta.disconnect()`](#wifistadisconnect)
- [`wifi.sta.config()`](#wifistaconfig)
- [`wifi.sta.on()`](#wifistaon)

## wifi.sta.disconnect()

Disconnects from AP in station mode.

!!! note

    Please note that disconnecting from Access Point does not reduce power consumption much.
    If power saving is your goal, please use [`wifi.stop()`](#wifisetmode).

#### Syntax
`wifi.sta.disconnect()`

#### Parameters
`nil`

#### Returns
`nil`

#### See also
- [`wifi.sta.config()`](#wifistaconfig)
- [`wifi.sta.connect()`](#wifistaconnect)


## wifi.sta.settxpower

Allows adjusting the maximum TX power for the WiFi. This is (unfortunately) needed for some boards which 
have a badly matched antenna.

#### Syntax
`wifi.sta.settxpower(power)`

#### Parameters
- `power` The maximum transmit power in dBm. This must have the range 2dBm - 20dBm. This value is a float.

#### Returns
A `boolean` where `true` is OK.

#### Example

```
# Needed for the WEMOS C3 Mini
wifi.sta.settxpower(8.5)
```

## wifi.sta.powersave

Configures power-saving setting in station mode.

#### Syntax
`wifi.sta.powersave(setting)`

#### Parameters
- `setting` one of `"none"`, `"min"` or `"max"`. In `"min"` mode, the station wakes up every DTIM period to receive the beacon. In `"max"` mode, the station wakes up at the interval configured in `listen_interval` (see [`wifi.sta.config()`](#wifistaconfig). When set to `"none"` the station does not go to sleep and can receive frames immediately.

#### Returns
`nil`

#### See also
- [`wifi.sta.getpowersave()`](#wifistagetpowersave)


## wifi.sta.getpowersave

Returns the configured station power-saving mode.

#### Syntax
`wifi.sta.getpowersave()`

#### Parameters
`nil`

#### Returns

One of `"none"`, `"min"` or `"max"`. Refer to [`wifi.sta.powersave()`](#wifistapowersave) for details.

#### See also
- [`wifi.sta.powersave()`](#wifistapowersave)


## wifi.sta.on()

Registers callbacks for WiFi station status events.

####  Syntax
`wifi.sta.on(event, callback)`

####  Parameters
- `event` WiFi station event you would like to set a callback for:
    - "start"
    - "stop"
    - "connected"
    - "disconnected"
    - "authmode_changed"
    - "got_ip"
- `callback` callback `function(event, info)` to perform when event occurs,
  or `nil` to unregister the callback for the event. The `info` argument
  given to the callback is a table containing additional information about
  the event.

Event information provided for each event is as follows:

- `start`: no additional info
- `stop`: no additional info
- `connected`: information about network/AP that was connected to:
    - `ssid`: the SSID of the network
    - `bssid`: the BSSID of the AP
    - `channel`: the primary channel of the network
    - `auth` authentication method, one of `wifi.AUTH_OPEN`, `wifi.AUTH_WEP`, `wifi.AUTH_WPA_PSK`, `wifi.AUTH_WPA2_PSK`, `wifi.AUTH_WPA_WPA2_PSK`, `wifi.AUTH_WPA2_ENTERPRISE`, `wifi.AUTH_WPA3_PSK`, `wifi.AUTH_WPA2_WPA3_PSK`, `wifi.AUTH_WAPI_PSK`
- `disconnected`: information about the network/AP that was disconnected from:
    - `ssid`: the SSID of the network
    - `bssid`: the BSSID of the AP
    - `reason`: an integer code for the reason (see table below for mapping)
- `authmode_changed`: authentication mode information:
    - `old_mode`: the previous auth mode used
    - `new_mode`: the new auth mode used
- `got_ip`: IP network information:
    - `ip`: the IP address assigned
    - `netmask`: the IP netmask
    - `gw`: the gateway ("0.0.0.0" if no gateway)

Table containing disconnect reasons.

|  Disconnect reason  |  value  |
|:--------------------|:-------:|
|UNSPECIFIED   |  1  |
|AUTH_EXPIRE   |  2  |				
|AUTH_LEAVE    |  3  |
|ASSOC_EXPIRE  |  4  |
|ASSOC_TOOMANY |  5  |
|NOT_AUTHED    |  6  |
|NOT_ASSOCED   |  7  |
|ASSOC_LEAVE   |  8  |
|ASSOC_NOT_AUTHED     |  9  |
|DISASSOC_PWRCAP_BAD  |  10  |
|DISASSOC_SUPCHAN_BAD |  11  |
|IE_INVALID    |  13  |
|MIC_FAILURE   |  14  |
|4WAY_HANDSHAKE_TIMEOUT   |  15  |
|GROUP_KEY_UPDATE_TIMEOUT |  16  |
|IE_IN_4WAY_DIFFERS       |  17  |
|GROUP_CIPHER_INVALID     |  18  |
|PAIRWISE_CIPHER_INVALID  |  19  |
|AKMP_INVALID          |  20  |
|UNSUPP_RSN_IE_VERSION |  21  |
|INVALID_RSN_IE_CAP    |  22  |
|802_1X_AUTH_FAILED    |  23  |
|CIPHER_SUITE_REJECTED |  24  |
|BEACON_TIMEOUT    |  200  |
|NO_AP_FOUND       |  201  |
|AUTH_FAIL         |  202  |
|ASSOC_FAIL        |  203  |
|HANDSHAKE_TIMEOUT |  204  |

####  Returns
`nil`

####  Example
```lua
--register callback
wifi.sta.on("got_ip", function(ev, info)
  print("NodeMCU IP config:", info.ip, "netmask", info.netmask, "gw", info.gw)
end)

--unregister callback
wifi.sta.on("got_ip", nil)
```


## wifi.sta.getmac()
Gets MAC address in station mode.

#### Syntax
`wifi.sta.getmac()`

#### Parameters
None


## wifi.sta.scan()

Scan for available networks.

#### Syntax
`wifi.sta.scan(cfg, callback)`

#### Parameters
- `cfg` table that contains scan configuration:
    - `ssid` SSID == nil, don't filter SSID
    - `bssid` BSSID == nil, don't filter BSSID
    - `channel` channel == 0, scan all channels, otherwise scan set channel (default is 0)
    - `hidden` hidden == 1, get info for router with hidden SSID (default is 0)
- `callback(ap_list)` a callback function to receive the list of APs when the scan is done. Each entry in the returned array follows the format used for [`wifi.sta.config()`](#wifistaconfig), with some additional fields.

The following fields are provided for each scanned AP:

- `ssid`: the network SSID
- `bssid`: the BSSID of the AP
- `channel`: primary WiFi channel of the AP
- `rssi`: Received Signal Strength Indicator value
- `auth` authentication method, one of `wifi.AUTH_OPEN`, `wifi.AUTH_WEP`, `wifi.AUTH_WPA_PSK`, `wifi.AUTH_WPA2_PSK`, `wifi.AUTH_WPA_WPA2_PSK`, `wifi.AUTH_WPA2_ENTERPRISE`, `wifi.AUTH_WPA3_PSK`, `wifi.AUTH_WPA2_WPA3_PSK`, `wifi.AUTH_WAPI_PSK`
- `bandwidth`: one of the following constants:
    - `wifi.HT20`
    - `wifi.HT40_ABOVE`
    - `wifi.HT40_BELOW`

#### Returns
`nil`

#### Example

```lua
-- Scan and print all found APs, including hidden ones
wifi.sta.scan({ hidden = 1 }, function(err,arr)
  if err then
    print ("Scan failed:", err)
  else
    print(string.format("%-26s","SSID"),"Channel BSSID              RSSI Auth Bandwidth")
    for i,ap in ipairs(arr) do
      print(string.format("%-32s",ap.ssid),ap.channel,ap.bssid,ap.rssi,ap.auth,ap.bandwidth)
    end
    print("-- Total APs: ", #arr)
  end
end)
```

## wifi.sta.setip()

Sets IP address, netmask, gateway, dns address in station mode.

Options set by this function are not saved to flash.

#### Syntax
`wifi.sta.setip(cfg)`

#### Parameters
- `cfg` table to hold configuration:
    - `ip` device ip address.
    - `netmask` network netmask.
    - `gateway` gateway address.
    - `dns` name server address.

#### Returns 
`nil`

#### Example
```lua
  cfg={}
  cfg.ip=192.168.0.10
  cfg.netmask=255.255.255.0
  cfg.gateway=192.168.0.1
  cfg.dns=8.8.8.8
  wifi.sta.setip(cfg)
```

## wifi.sta.sethostname

Sets station hostname

Must be called before `wifi.sta.connect()`

Options set by this function are not saved to flash.

#### Syntax
`wifi.sta.sethostname(hostname)`

#### Returns 
true if success, false otherwise

#### Parameters
`hostname` must only contain letters, numbers and hyphens('-') and be 32 characters or less with first and last character being alphanumeric

#### Example
```lua
  wifi.sta.sethostname("ESP32")
```

# wifi.ap Module

## wifi.ap.config()

Configures the AP.

The WiFi mode must be set to `wifi.SOFTAP` or `wifi.STATIONAP` before this
function can be used.

#### Syntax
`wifi.ap.config(cfg, save)`

#### Parameters
- `cfg` table to hold configuration:
    - `ssid` SSID chars 1-32
    - `pwd` password chars 8-64
    - `auth` authentication method, one of `wifi.AUTH_OPEN`, `wifi.AUTH_WPA_PSK`, `wifi.AUTH_WPA2_PSK` (default), `wifi.AUTH_WPA_WPA2_PSK`
    - `channel` channel number 1-14 default = 11
    - `hidden` false = not hidden, true = hidden, default = false
    - `max` maximum number of connections 1-4 default=4
    - `beacon` beacon interval time in range 100-60000, default = 100
- `save` save configuration to flash.
    - `true` configuration **will** be retained through power cycle. (Default)
    - `false` configuration **will not** be retained through power cycle.

#### Returns
`nil`

#### Example:
```lua
 cfg={}
 cfg.ssid="myssid"
 cfg.pwd="mypassword"
 wifi.ap.config(cfg)
```


## wifi.ap.on()

Registers callbacks for WiFi AP events.

####  Syntax
`wifi.ap.on(event, callback)`

####  Parameters
- `event` WiFi AP event you would like to set a callback for:
    - "start"
    - "stop"
    - "sta_connected"
    - "sta_disconnected"
    - "probe_req"
- `callback` callback `function(event, info)` to perform when event occurs,
  or `nil` to unregister the callback for the event. The `info` argument
  given to the callback is a table containing additional information about
  the event.

Event information provided for each event is as follows:

- `start`: no additional info
- `stop`: no additional info
- `sta_connected`: information about the client that connected:
    - `mac`: the MAC address
    - `id`: assigned station id (AID)
- `disconnected`: information about disconnecting client
    - `mac`: the MAC address
    - `id`: assigned station id (AID)
- `probe_req`: information about the probing client
    - `from`: MAC address of the probing client
    - `rssi`: Received Signal Strength Indicator value

## wifi.ap.getmac()

Gets MAC address in access point mode.

#### Syntax
`wifi.ap.getmac()`

#### Parameters
None

#### Returns
MAC address as string e.g. "18:fe:34:a2:d7:34"

## wifi.ap.setip()

Sets IP address, netmask, gateway, dns address in AccessPoint mode.

Options set by this function are not saved to flash.

#### Syntax
`wifi.ap.setip(cfg)`

#### Parameters
- `cfg` table to hold configuration:
    - `ip` device ip address.
    - `netmask` network netmask.
    - `gateway` gateway address.
    - `dns` name server address, which will be provided to clients over DHCP. (Optional)

#### Returns 
`nil`

#### Example
```lua
  cfg={}
  cfg.ip=192.168.0.10
  cfg.netmask=255.255.255.0
  cfg.gateway=192.168.0.1
  cfg.dns=8.8.8.8
  wifi.ap.setip(cfg)
```

## wifi.ap.sethostname

Sets AccessPoint hostname.

Options set by this function are not saved to flash.

#### Syntax
`wifi.ap.sethostname(hostname)`

#### Returns 
true if success, false otherwise

#### Parameters
`hostname` must only contain letters, numbers and hyphens('-') and be 32 characters or less with first and last character being alphanumeric

#### Example
```lua
  wifi.ap.sethostname("ESP32")
```

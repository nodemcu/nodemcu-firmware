# WiFi Module

The NodeMCU WiFi control is spread across several tables:
  - `wifi`, for overall WiFi configuration
  - `wifi.sta` for station mode functions
  - `wifi.ap` for Access Point functions
  - `wifi.ap.dhcp` for DHCP server control


## wifi.setmode()

Configures the WiFi mode to use. NodeMCU can run in one of four WiFi modes:
  - Station mode, where the NodeMCU device joins an existing network
  - Access Point (AP) mode, where it creates its own network that others can join
  - Station + AP mode, where it both creates its own network while at the same time being joined to another existing network
  - WiFi off

When using the combined Station + AP mode, the same channel will be used for both networks as the radio can only listen on a single channel.

#### Syntax
`wifi.setmode(mode)`

#### Parameters
  - `mode` one of the following:
    - `wifi.STATION` for when the device is connected to another WiFi router. This is often done to give the device access to the internet.
    - `wifi.SOFTAP` for when the device is acting *only* as an access point. This will allow you to see the device in the list of WiFi networks (unless you hide the SSID, of course). In this mode your computer can connect to the device, creating a local area network. Unless you change the value, the NodeMCU device will be given a local IP address of 192.168.4.1 and assign your computer the next available IP address, such as 192.168.4.2.
    - `wifi.STATIONAP` is the combination of `wifi.STATION` and `wifi.SOFTAP`. It allows you to create a local WiFi connection *and* connect to another WiFi router.
   - `wifi.NULLMODE` to switch off WiFi

#### Returns
The resulting mode.

#### See also
  - [`wifi.getmode()`](#wifigetmode)


## wifi.getmode()

Returns the current WiFi mode.

#### Syntax
`wifi.getmode()`

#### Parameters
none

#### Returns
The WiFi mode, as one of the `wifi.STATION`, `wifi.SOFTAP`, `wifi.STATIONAP` or `wifi.NULLMODE` constants.

#### See also
  - [`wifi.setmode()`](#wifisetmode)


## wifi.getchannel()

Returns the current WiFi channel used.

#### Syntax
`wifi.getchannel()`

#### Parameters
none

#### Returns
The current WiFi channel number.


## wifi.setphymode()

Setup WiFi physical mode.

* `wifi.PHYMODE_B`
  802.11b, More range, Low Transfer rate, More current draw
* `wifi.PHYMODE_G`
  802.11g, Medium range, Medium transfer rate, Medium current draw
* `wifi.PHYMODE_N`
  802.11n, Least range, Fast transfer rate, Least current draw (STATION ONLY)

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
  - `mode` one of the following:
    - `wifi.PHYMODE_B`
    - `wifi.PHYMODE_G`
    - `wifi.PHYMODE_N`

#### Returns
Current physical mode after setup.

#### See also
  - [`wifi.getphymode()`](#wifigetphymode)


## wifi.getphymode()

Get the current physical mode.

#### Syntax
`wifi.getphymode()`

#### Parameters
none

#### Returns
The current physical mode as one of `wifi.PHYMODE_B`, `wifi.PHYMODE_G` or `wifi.PHYMODE_N`.


## wifi.startsmart()

Starts to auto configuration, if success set up SSID and password automatically.

Intended for use with SmartConfig apps, such as Espressif's [Android & iOS app](https://github.com/espressifapp).

Only usable in `wifi.STATION` mode.

#### Syntax
`wifi.startsmart(type, callback)`

#### Parameters
  - `type` 0 for ESP_TOUCH, or 1 for AIR_KISS.
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
  - [`wifi.stopsmart()`](#wifistopsmart)


## wifi.stopsmart()

Stops the autoconfiguration attempt.

#### Syntax
`wifi.stopsmart()`

#### Parameters
none

#### Returns
`nil`

#### See also
  - [`wifi.startsmart()`](#wifistartsmart)


## wifi.sleeptype()

Configures the WiFi modem sleep type.

#### Syntax
`wifi.sleeptype(type_wanted)`

#### Parameters
  - `type_wanted` one of the following:
    - `wifi.NONE_SLEEP` to keep the modem on at all times
    - `wifi.LIGHT_SLEEP` to allow the modem to power down under some circumstances
    - `wifi.MODEM_SLEEP` to power down the modem as much as possible

#### Returns
The actual sleep mode set, as one of `wifi.NONE_SLEEP`, `wifi.LIGHT_SLEEP` or `wifi.MODEM_SLEEP`.

#### See also
  - [`node.dsleep()`](#nodedsleep)
  - [`rtctime.dsleep()`](#rtctimedsleep)


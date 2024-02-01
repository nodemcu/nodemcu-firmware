# Eth Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2019-06-25 | [Arnim Laeuger](https://github.com/devsaurus) |[Arnim Laeuger](https://github.com/devsaurus) | [eth.c](../../components/modules/eth.c)|

The eth module provides access to the ethernet PHY chip configuration.

Your board must contain one of the PHY chips that are supported by ESP-IDF:

- IP101
- RTL8201
- LAN8720 (possibly others in the LAN87xx family)
- DP83848
- KSZ8001 / KSZ8021 / KSZ8031 / KSZ8041 / KSZ8051 / KSZ8061 / KSZ8081 / KSZ8091

## eth.get_mac()
Get MAC address.

#### Syntax
```lua
local mac = eth.get_mac()
```

#### Parameters
None

#### Returns
MAC address as string "aa:bb:cc:dd:ee:dd".


## eth.get_speed()
Get Ethernet connection speed.

#### Syntax
```lua
local speed = eth.get_speed()
```

#### Parameters
None

#### Returns
Connection speed in Mbit/s, or error if not connected.
- `10`
- `100`


## eth.init()
Initialize the internal ethernet interface by configuring the MAC and PHY
and starting the interface.

Note that while the PHY model and some GPIO pins are configured at runtime,
the clock configuration has been moved into the menuconfig and is no
longer available at runtime. Please refer to the settings available
under `Component config -> Ethernet -> Support ESP32 internal EMAC controller`.


#### Syntax
```lua
eth.init(cfg)
```

#### Parameters
- `cfg` table containing configuration data. Unless otherwise noted,
  the entries are mandatory:
    - `addr` PHY address, 0 to 31, optional (default will attempt auto-detect)
    - `mdc` MDC pin number
    - `mdio` MDIO pin number
    - `phy` PHY chip model, one of
        - `PHY_DP83848`
        - `PHY_IP101`
        - `PHY_KSZ80XX`
        - `PHY_KSZ8041` (deprecated, use `PHY_KSZ80XX` instead)
        - `PHY_KSZ8081` (deprecated, use `PHY_KSZ80XX` instead)
        - `PHY_LAN87XX`
        - `PHY_LAN8720` (deprecated, use `PHY_LAN87XX` instead)
        - `PHY_RTL8201`
    - `power` power enable pin, optional

#### Returns
`nil`

An error is thrown in case of invalid parameters, MAC/PHY setup errors, or if the ethernet interface has already been successfully configured.

#### Example
```lua
-- Initialize ESP32-GATEWAY
eth.init({phy  = eth.PHY_LAN8720,
          addr = 0,
          power = 5,
          mdc   = 23,
          mdio  = 18})

-- Initialize wESP32
eth.init({phy  = eth.PHY_LAN8720,
          addr = 0,
          mdc   = 16,
          mdio  = 17})


-- Initialise wt32-eth01
eth.init({ phy = eth.PHY_LAN8720, addr = 1, mdc = 23, mdio = 18, power = 16 })

```


## eth.on()
Register or unregister callback functions for Ethernet events.

#### Syntax
```lua
eth.on(event, callback)
```

#### Parameters
- `event` Ethernet event to register the callback for:
    - "start"
    - "stop"
    - "connected"
    - "disconnected"
    - "got_ip"
- `callback` callback `function(event, info)` to perform when event occurs, or `nil` to unregister the callback for the event. The `info` argument given to the callback is a table containing additional information about the event.

Event information provided for each event is as follows:

- `start`: no additional info
- `stop`: no additional info
- `connected`: no additional info
- `disconnected`: no additional info
- `got_ip`: IP network information:
    - `ip`: the IP address assigned
    - `netmask`: the IP netmask
    - `gw`: the gateway ("0.0.0.0" if no gateway)

#### Example
```lua
function ev(event, info)
    print("event", event)
    if event == "got_ip" then
        print("ip:"..info.ip..", nm:"..info.netmask..", gw:"..info.gw)
    elseif event == "connected" then
        print("speed:", eth.get_speed())
        print("mac:", eth.get_mac())
    end
end

eth.on("connected", ev)
eth.on("disconnected", ev)
eth.on("start", ev)
eth.on("stop", ev)
eth.on("got_ip", ev)
```


## eth.set_mac()
Set MAC address.

#### Syntax
```lua
eth.set_mac(mac)
```

#### Parameters
- `mac` MAC address as string "aa:bb:cc:dd:ee:ff"

#### Returns
`nil`

An error is thrown in case of invalid parameters or if the ethernet driver failed.

## eth.set_ip()
Configures a static IPv4 address on the ethernet interface.

Calling this function does three things:
  - disables the DHCP client for the ethernet interface
  - sets the IP address, netmask and gateway
  - set the DNS server to use

Note that these settings are not persisted to flash.

#### Syntax
`eth.set_ip(cfg_opts)`

#### Parameters
- `cfg_opts` - table with the following fields:
    - `ip` static IPv4 address to set
    - `netmask` the network netmask
    - `gateway` default gateway to use
    - `dns` DNS server

#### Returns
`nil`

An error is thrown in case of invalid parameters or if any of the options can
not be set.

#### Example

```lua
eth.set_ip({
  ip = "192.168.0.12",
  netmask = "255.255.255.0",
  gateway = "192.168.0.1",
  dns = "8.8.8.8"
})
```

## eth.set_hostname()
Configures the interface specific hostname for the ethernet interface. The ethernet interface must be initialized before the hostname can be configured.

By default the system hostname is used, as configured in the menu config.

#### Syntax
```lua
eth.set_hostname(hostname)
```

#### Parameters
- `hostname` the hostname to use on the ethernet interface

#### Returns
`nil`

An error is thrown in case the hostname cannot be set.

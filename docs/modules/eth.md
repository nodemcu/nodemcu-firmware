# Eth Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2019-06-25 | [Arnim Laeuger](https://github.com/devsaurus) |[Arnim Laeuger](https://github.com/devsaurus) | [eth.c](../../components/modules/eth.c)|

The eth module provides access to the ethernet PHY chip configuration.

Your board must contain one of the PHY chips that are supported by ESP-IDF:

- IP101
- LAN8720
- TLK110

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
Initialize the PHY chip and set up its tcpip adapter.

#### Syntax
```lua
eth.init(cfg)
```

#### Parameters
- `cfg` table containing configuration data. All entries are mandatory:
    - `addr` PHY address, 0 to 31
    - `clock_mode` external/internal clock mode selection, one of
        - `eth.CLOCK_GPIO0_IN`
        - `eth.CLOCK_GPIO0_OUT`
        - `eth.CLOCK_GPIO16_OUT`
        - `eth.CLOCK_GPIO17_OUT`
    - `mdc` MDC pin number
    - `mdio` MDIO pin number
    - `phy` PHY chip model, one of
        - `eth.PHY_IP101`
        - `eth.PHY_LAN8720`
        - `eth.PHY_TLK110`
    - `power` power enable pin, optional

#### Returns
`nil`

An error is thrown in case of invalid parameters or if the ethernet driver failed.

#### Example
```lua
-- Initialize ESP32-GATEWAY
eth.init({phy  = eth.PHY_LAN8720,
          addr = 0,
          clock_mode = eth.CLOCK_GPIO17_OUT,
          power = 5,
          mdc   = 23,
          mdio  = 18})

-- Initialize wESP32
eth.init({phy  = eth.PHY_LAN8720,
          addr = 0,
          clock_mode = eth.CLOCK_GPIO0_IN,
          mdc   = 16,
          mdio  = 17})
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

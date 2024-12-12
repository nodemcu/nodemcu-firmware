# mDNS Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2024-12-11 | [Jade Mattsson](https://github.com/jmattsson) | [Jade Mattsson](https://github.com/jmattsson) | [mdns.c](../../components/modules/mdns.c)|

This module provides access to the mDNS subsystem and allows both registering services that can be discovered and performing service discovery on the local network.

Other names for mDNS include Bonjour and Avahi.

Some aspects of the mDNS subsystem are compile-time configurable via Kconfig (`make menuconfig`). The defaults are likely sufficient for the vast majority of users.

## mdns.start()

Initialises the mDNS subsystem and registers any services for the device.

#### Syntax
`mdns.start(config)`

#### Parameters
- `config` Table containing the mDNS service configuration:
  - `hostname` (Required if any services are to be registered) The hostname to use for mDNS.
  - `instance_name` (Optional) The default service instance name. Defaults to the hostname if not set explicitly.
  - `services` (Optional) An array of service entries to register with mDNS, with each entry being a table comprising these fields:
    - `service_type` (Required) The service type to register, e.g. `"_http"`.
    - `protocol` (Required) The protocol to register, one of `"_udp"` or `"_tcp"` typically.
    - `port` (Required) The port number of the service, e.g. `80`.
    - `subtype` (Optional) The service subtype, if applicable.
    - `instance_name` (Optional) The instance name of the service. Defaults to the system-wide instance name if not set explicitly.
    - `txt` (Optional) A table of key/value value pairs to add to the service's `TXT` entry.

#### Returns
The number of services registered with the mDNS subsystem.

#### Examples
Enabling mDNS discovery of this device for both HTTP and FTP services.

```lua
mdns.start({
  hostname="esp32server",
  instance_name="My cool ESP32",
  services={
    {
      service_type="_http",
      protocol="_tcp",
      port=80,
      txt={
        path="/login.html"
      }
    },
    {
      service_type="_ftp",
      protocol="_tcp",
      port=21
    }
  }
})
```

Starting mDNS without registering any services. Only useful for doing mDNS queries.

```lua
mdns.start({})
```

## mdns.query()

Perform an mDNS query.

#### Syntax
`mdns.query(query)`

#### Parameters
- `query` Table with the query parameters. Most fields are optional depending on the query type.
  - `query_type` (Required) The type of mDNS query to issue. One of:
    - `mdns.TYPE_A` IPv4 address lookup query.
    - `mdns.TYPE_AAAA` IPv6 address lookup query.
    - `mdns.TYPE_PTR` PTR record query (find services).
    - `mdns.TYPE_TXT` TXT record query.
    - `mdns.TYPE_SRV` SRV record query (find hostname/port for service).
    - `mdns.TYPE_ANY` Query all record types.
  - `name` Name to query for.
  - `service_type` The service type to query for.
  - `protocol` The transport protocol of the service being queried for (e.g. `"_tcp"` or `"_udp"`.
  - `timeout` Timeout in milliseconds to wait for responses. Default 2000.
  - `max_results` Maximum number of responses to return. Default 10.

#### Returns
A Lua array with the results. Each result is a table. The fields in the table depend on the query type performed.

```lua
{ {
    -- PTR results
    instance_name=,
    service_type=,
    protocol=,
    -- SRV results
    hostname=,
    port=,
    -- TXT results
    txt={
      key1=,
      key2=,
      ...
    },
    -- A and AAAA results
    addresses={ ip1str, ip2str, ...  }
  },
  ...
}
```

## mdns.stop()

Unregisters any services and shuts down the mDNS subsystems.

#### Syntax
`mdns.stop()`

#### Parameters
None

#### Returns
`nil`

#### Examples
Find SMB file shares on the network:

```lua
r=mdns.query({query_type=mdns.TYPE_PTR,service_type="_smb",protocol="_tcp"})
dump(r)
{
  1={
    service_type=_smb
    protocol=_tcp
    hostname=mynas
    port=445
    instance_name=mynas
  }
  2={
    service_type=_smb
    protocol=_tcp
    hostname=desktop
    port=445
    instance_name=desktop
  }
}
```

Resolve IPv4 address:

```lua
r=mdns.query({query_type=mdns.TYPE_A,name="mynas"})
dump(r)
{
  1={
    addresses={
      1=192.168.1.8
    }
    hostname=mynas
  }
}
```

Resolve IPv6 address:

```lua
r=mdns.query({query_type=mdns.TYPE_AAAA,name="Hue-Study"})
dump(r)
{
  1={
    addresses={
      1=2001:44B8:221A:2132:217:88FF:FEB1:E364
      2=2001:44B8:8C77:EB32:217:88FF:FEB1:E364
      3=FE80::217:88FF:FEB1:E364
    }
    hostname=Hue-Study
  }
}
```

And in case someone wants the `dump` function, it's just:

```lua
function dump(x, ind, key)
  local ind = ind or 0
  local key = key or ""
  local indent = string.rep("  ", ind)
  local t = type(x)
  local prefix=indent..key.."="
  if x == nil then
    print(prefix.."nil")
  elseif (t == "table") then
    print(prefix.."{")
    for k,v in pairs(x) do
      dump(v, ind + 1, k)
    end
    print(indent.."}")
  elseif (t == "number" or t == "string" or t == "boolean") then
    print(prefix..tostring(x))
  else
    print(prefix..t)
  end
end
```

# mDNS Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2016-02-24 | [Philip Gladstone](https://github.com/pjsg) | [Philip Gladstone](https://github.com/pjsg) | [mdns.c](../../../app/modules/mdns.c)|

[Multicast DNS](https://en.wikipedia.org/wiki/Multicast_DNS) is used as part of Bonjour / Zeroconf. This allows systems to identify themselves and the services that they provide on a local area network. Clients are then able to discover these systems and connect to them. 

!!! note

	This is a mDNS *server* module. If you are looking for a mDNS *client* for NodeMCU (i.e. to query mDNS) then [udaygin/nodemcu-mdns-client](https://github.com/udaygin/nodemcu-mdns-client) may be an option.

## mdns.register()
Register a hostname and start the mDNS service. If the service is already running, then it will be restarted with the new parameters.

#### Syntax
`mdns.register(hostname [, attributes])`

#### Parameters
- `hostname` The hostname for this device. Alphanumeric characters are best.
- `attributes` A optional table of options. The keys must all be strings.

The `attributes` contains two sorts of attributes — those with specific names, and those that are service specific. [RFC 6763](https://tools.ietf.org/html/rfc6763#page-13) 
defines how extra, service specific, attributes are encoded into the DNS. One example is that if the device supports printing, then the queue name can 
be specified as an additional attribute. This module supports up to 10 such attributes.

The specific names are:

- `port` The port number for the service. Default value is 80.
- `service` The name of the service. Default value is 'http'.
- `description` A short phrase (under 63 characters) describing the service. Default is the hostname.

#### Returns
`nil`

#### Errors
Various errors can be generated during argument validation. The NodeMCU must have an IP address at the time of the call, otherwise an error is thrown.

#### Example

    mdns.register("fishtank", {hardware='NodeMCU'})

Using `dns-sd` on macOS, you can see `fishtank.local` as providing the `_http._tcp` service. You can also browse directly to `fishtank.local`. In Safari you can get all the mDNS web pages as part of your bookmarks menu.

    mdns.register("fishtank", { description="Top Fishtank", service="http", port=80, location='Living Room' })

## mdns.close()
Shut down the mDNS service. This is not normally needed.

#### Syntax
`mdns.close()`

#### Parameters
none

#### Returns
`nil`

# mdns Module

Multicast DNS is used as part of Bonjour / Zeroconf. This allows system to identify themselves and the services that they provide on a local area network. Clients are then
able to discover these systems and connect to them. 

## mdns.register()
Register a hostname and start the mDns service. If the service is already running, then it 
will be restarted with the new parameters.

#### Syntax
`mdns.register(hostname, servicename, port [, attributes])`

#### Parameters
- `hostname` The hostname for this device. Alphanumeric characters are best.
- `servicename` The servicename for this device. Alphanumeric characters are best. This will get prefixed with '_' and suffixed with ._tcp
- `port` The port number for the primary service.
- `attributes` A optional table of up to 10 attributes to be exposed. The keys must all be strings.

#### Returns
Nothing.

#### Errors
Various errors can be generated during argument validation. The nodemcu must have an IP address at the time of the call, otherwise an error is thrown.

#### Example

    mdns.register("fishtank", "http", 80, { hardware='NodeMCU'})

Using dns-sd on OSX, you can see fishtank.local as providing the _http._tcp service. You can also browse directly to fishtank.local. In safari you can get all the mdns web pages as part of your bookmarks menu.

## mdns.close()
Shut down the mDns service. This is not normally needed.

#### Syntax
`mdns.close()`

#### Returns
Nothing.

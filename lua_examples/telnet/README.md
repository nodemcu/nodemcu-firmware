# Telnet Module

| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2014-12-22 | [Zeroday](https://github.com/funshine) |  [Terry Ellison](https://github.com/TerryE) | [simple_telnet.lua](./simple_telnet.lua) |
| 2018-05-24 | [Terry Ellison](https://github.com/TerryE) |  [Terry Ellison](https://github.com/TerryE) | [telnet.lua](./telnet.lua) |


This README discusses the packet marshalling versions of telnet. The first (fifosock)
version was written for SDK 2 implementations, with all of the marshalling imlemented
in Lua; the second (pipe) version uses the latest features added to the SDK 3 version
that have been added to prepare for the `lua53` implementation.  These exploit the
stdin / stdout pipe functionality and task integration that is now build into the
NodeNMCU Lua core. 

There are two nice advantages of this core implementation:

-  Errors are now written to stdout in a spearate task execution.
-  The pipes pretty much eliminate uart and telnet overrun.

Both have the same interface if required into the variable `telnet`

## telnet:open()

Open a telnet server based on the provided parameters.

#### Syntax

`telnet:open(ssid, pwd, port)`

#### Parameters

`ssid` and `password`.  Strings.  SSID and Password for the Wifi network.  If these are
`nil` then the wifi is assumed to be configured or autoconfigured.

`port`.  Integer TCP listenting port for the Telnet service.  The default is 2323

#### Returns

Nothing returned (this is evaluted as `nil` in a scalar context).

## telnet:close()

Close a telnet server and release all resources.  Also set the variable `telnet` to nil to fully reference and GC the resources.

#### Syntax

`telnet:close()`

#### Parameters

None

#### Returns

Nothing returned (this is evaluted as `nil` in a scalar context).

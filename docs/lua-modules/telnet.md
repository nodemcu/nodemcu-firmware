# Telnet Module

| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2018-05-24 | [Terry Ellison](https://github.com/TerryE) |  [Terry Ellison](https://github.com/TerryE) | [telnet.lua](../../lua_modules/telnet/telnet.lua) |

The current version of this module exploits the stdin / stdout pipe functionality and 
task integration that is now build into the NodeNMCU Lua core. 

There are two nice advantages of this core implementation:

-  Errors are now written to stdout in a separate task execution.
-  The pipes pretty much eliminate UART and telnet overrun.

Both have the same interface if required into the variable `telnet`

## telnet:open()

Open a telnet server based on the provided parameters.

#### Syntax

`telnet:open(ssid, pwd, port)`

#### Parameters

`ssid` and `password`.  Strings.  SSID and Password for the Wifi network.  If these are
`nil` then the wifi is assumed to be configured or auto-configured.

`port`.  Integer TCP listening port for the Telnet service.  The default is 2323

#### Returns

Nothing returned (this is evaluated as `nil` in a scalar context).

## telnet:close()

Close a telnet server and release all resources.  Also set the variable `telnet` to nil to fully reference and GC the resources.

#### Syntax

`telnet:close()`

#### Parameters

None

#### Returns

Nothing returned (this is evaluated as `nil` in a scalar context).

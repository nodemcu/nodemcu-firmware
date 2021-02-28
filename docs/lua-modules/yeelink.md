# Yeelink Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-04-14 | [Martin Han](https://github.com/MarsTechHAN) | [Martin Han](https://github.com/MarsTechHAN) | [yeelink_lib.lua](../../lua_modules/yeelink/yeelink_lib.lua) |

This Lua module provides a simple implementation of an [Yeelink](http://www.yeelink.net/) client.

### Require
```lua
yeelink = require("yeelink_lib")
```

### Release
```lua
yeelink = nil
package.loaded["yeelink_lib"] = nil
```

## yeelink.init()
Initializes Yeelink client.

#### Syntax
`yeelink.init(device, sensor, apikey)`

#### Parameters
- `device`: device number
- `sensor`: sensor number
- `apikey`: Yeelink API key string

#### Returns
IP address of `api.yeelink.net`, if not obtained then `false`

## yeelink.getDNS()
Function to check DNS resolution of `api.yeelink.net` status.

#### Syntax
`yeelink.getDNS()`

#### Parameters
None

#### Returns
IP address of `api.yeelink.net` or `nil` when name resolution failed.

## yeelink.update()
Send data to Yeelink Sever.

#### Syntax
`yeelink.update(datapoint)`

#### Parameters
- `datapoint`: Data to send to Yeelink API

#### Returns
`nil`

#### Notes
Example of using this module can be found in [Example_for_Yeelink_Lib.lua](../../lua_modules/yeelink/Example_for_Yeelink_Lib.lua) file.

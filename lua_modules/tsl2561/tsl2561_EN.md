# tsl2561 Module

##Require
```lua
tsl2561 = require("tsl2561")
```
## Release
```lua
tsl2561 = nil
package.loaded["tsl2561"]=nil
```
<a id="tsl2561_init"></a>
##init()
####Description
Setting the I2C pin of tsl2561.<br />

####Syntax
init(sda, scl)

####Parameters
sda: 1~12, IO index.<br />
scl: 1~12, IO index.<br />

####Returns
nil

####Example
```lua
SDA_PIN = 6 -- sda pin, GPIO12
SCL_PIN = 5 -- scl pin, GPIO14

tsl2561 = require("tsl2561")
tsl2561.init(SDA_PIN, SCL_PIN)

-- release module
tsl2561 = nil
package.loaded["tsl2561"]=nil
```

####See also
**-**   []()

<a id="tsl2561_read"></a>
##readVisibleLux()
####Description
Get the Lux reading of visible light<br />

####Syntax
readVisibleLux()

####Parameters
nil.<br />

####Returns
nil.<br />

####Example
```lua
SDA_PIN = 6 -- sda pin, GPIO12
SCL_PIN = 5 -- scl pin, GPIO14

tsl2561 = require("tsl2561")
tsl2561.init(SDA_PIN, SCL_PIN)
lux = tsl2561.readVisibleLux()

-- release module
tsl2561 = nil
package.loaded["tsl2561"]=nil
```

####See also
**-**   []()

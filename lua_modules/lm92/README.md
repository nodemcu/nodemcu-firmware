# LM92 module
This module adds basic support for the LM92 +-0.33C 12bit+sign temperature sensor. More details in the [datasheet](http://www.ti.com/lit/ds/symlink/lm92.pdf).
Works:
- getting the temperature
- entering the chip's to shutdown mode (350uA -> 5uA power consumption)
- waking up the chip from shutdown

##Require
```lua
LM92 = require("lm92")
```
## Release
```lua
LM92 = nil
package.loaded["lm92"]=nil
```

##init()
####Description
Setting the i2c pins and address for lm92.

####Syntax
init(sda, scl, address)

####Parameters
sda: 1~12, IO index.<br />
scl: 1~12, IO index.<br />
address: 0x48~0x4b, i2c address (depends on tha A0~A1 pins)
####Returns
nil

####Example
```lua
LM92 = require("lm92")
gpio0 = 3
gpio2 = 4
sda = gpio0
scl = gpio2
addr = 0x48
LM92.init(sda, scl,addr)
```
##getTemperature()
####Description
Returns the temperature register's content.

####Syntax
getTemperature()

####Parameters
-

####Returns
Temperature in degree Celsius.

####Example
```lua
t = LM92.getTemperature()
print("Got temperature: "..t.." C")
```

##wakeup()
####Description
Makes the chip exit the low power shutdown mode.

####Syntax
wakeup()

####Parameters
-

####Returns
-

####Example
```lua
LM92.wakeup()
tmr.delay( 1 * 1000 * 1000 )
```

##shutdown()
####Description
Makes the chip enter the low power shutdown mode.

####Syntax
shutdown()

####Parameters
-

####Returns
-

####Example
```lua
LM92.shutdown()
```
#### TODO:
- add full support of the features, including interrupt and critical alert support

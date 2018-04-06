# LM92 module
This module adds basic support for the LM92 +-0.33C 12bit+sign temperature sensor. More details in the [datasheet](http://www.ti.com/lit/ds/symlink/lm92.pdf).
Works:
- 06/01/2016: Add get/set comparison registers. Thyst, Tlow, Thigh and Tcrit
- getting the temperature
- entering the chip's to shutdown mode (350uA -> 5uA power consumption)
- waking up the chip from shutdown

## Require
```lua
LM92 = require("lm92")
```
## Release
```lua
LM92 = nil
package.loaded["lm92"]=nil
```

## setup()
#### Description
Setting the address for lm92.

#### Syntax
setup(sda, scl, address)

#### Parameters
address: 0x48~0x4b, i2c address (depends on tha A0~A1 pins)
#### Returns
nil

#### Example
```lua
LM92 = require("lm92")
gpio0 = 3
gpio2 = 4
sda = gpio0
scl = gpio2
addr = 0x48
i2c.setup(0, sda, scl, i2c.SLOW)  -- call i2c.setup() only once
LM92.setup(addr)
```
## getTemperature()
#### Description
Returns the temperature register's content.

#### Syntax
getTemperature()

#### Parameters
-

#### Returns
Temperature in degree Celsius.

#### Example
```lua
t = LM92.getTemperature()
print("Got temperature: "..t.." C")
```

## wakeup()
#### Description
Makes the chip exit the low power shutdown mode.

#### Syntax
wakeup()

#### Parameters
-

#### Returns
-

#### Example
```lua
LM92.wakeup()
tmr.delay( 1 * 1000 * 1000 )
```

## shutdown()
#### Description
Makes the chip enter the low power shutdown mode.

#### Syntax
shutdown()

#### Parameters
-

#### Returns
-

#### Example
```lua
LM92.shutdown()
```

## setThyst()
#### Description
Set hysteresis Temperature.

#### Syntax
setThyst(data_wr)

#### Parameters
data_wr: 130~-55 ºC, hysteresis Temperature

#### Returns
nil

#### Example
```lua
LM92.setThyst(3)
```

## setTcrit()
#### Description
Set Critical Temperature.

#### Syntax
setTcrit(data_wr)

#### Parameters
data_wr: 130~-55 ºC, Critical Temperature

#### Returns
nil

#### Example
```lua
LM92.setTcrit(100.625)
```

## setTlow()
#### Description
Set Low Window Temperature.

#### Syntax
setTlow(data_wr)

####Parameters
data_wr: 130~-55 ºC, Low Window Temperature

#### Returns
nil

#### Example
```lua
LM92.setTlow(32.25)
```
## setThigh()
####Description
Set High Window Temperature.

#### Syntax
setThigh(data_wr)

#### Parameters
data_wr: 130~-55 ºC, High Window Temperature

#### Returns
nil

####Example
```lua
LM92.setThigh(27.5)
```

## getThyst()
#### Description
Get hysteresis Temperature.

#### Syntax
getThyst()

#### Parameters
--

#### Returns
Hysteresis Temperature in degree Celsius.

#### Example
```lua
t = LM92.getThyst()
print("Got hysteresis temperature: "..t.." C")
```

## getTcrit()
#### Description
Get Critical Temperature.

#### Syntax
getTcrit()

#### Parameters
--

#### Returns
Critical Temperature in degree Celsius.

#### Example
```lua
t = LM92.getTcrit()
print("Got Critical temperature: "..t.." C")
```

## getTlow()
#### Description
Get Low Window Temperature.

#### Syntax
getTlow()

#### Parameters
--

#### Returns
Low Window Temperature in degree Celsius.

#### Example
```lua
t = LM92.getTlow()
print("Got Low Window temperature: "..t.." C")
```

## getThigh()
#### Description
Get High Window Temperature.

#### Syntax
getThigh()

#### Parameters
--

#### Returns
High Window Temperature in degree Celsius.

#### Example
```lua
t = LM92.getThigh()
print("Got High Window temperature: "..t.." C")
```

## Full example
```lua
--node.compile("lm92.lua")
LM92 = require("lm92")
gpio0 = 3
gpio2 = 4
sda = gpio0
scl = gpio2
addr = 0x48
i2c.setup(0, sda, scl, i2c.SLOW)  -- call i2c.setup() only once
LM92.setup(addr)
 
t = LM92.getTemperature()
print("Got temperature: "..t.." C")

--Seting comparison temperatures
LM92.setThyst(3)
LM92.setTcrit(40.75)
LM92.setTlow(28.5)
LM92.setThigh(31.625)

t = LM92.getThyst()
print("Got hyster: "..t.." C")
t = LM92.getTcrit()
print("Got Crit: "..t.." C")
t = LM92.getTlow()
print("Got Low: "..t.." C")
t = LM92.getThigh()
print("Got High: "..t.." C")
```

#### TODO:
- add full support of the features, including interrupt and critical alert support

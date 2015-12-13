# node Module
## node.restart()
####Description
Restarts the chip.

####Syntax
`node.restart()`

####Parameters
`nil`

####Returns
`nil`

####Example
   
```lua
node.restart();
```
___
## node.dsleep()
####Description
Enter deep sleep mode, wake up when timed out.

####Syntax
`node.dsleep(us, option)`

**Note:** This function can only be used in the condition that esp8266 PIN32(RST) and PIN8(XPD_DCDC aka GPIO16) are connected together. Using sleep(0) will set no wake up timer, connect a GPIO to pin RST, the chip will wake up by a falling-edge on pin RST.<br />
option=0, init data byte 108 is valuable;<br />
option>0, init data byte 108 is valueless.<br />
More details as follows:<br />
0, RF_CAL or not after deep-sleep wake up, depends on init data byte 108.<br />
1, RF_CAL after deep-sleep wake up, there will belarge current.<br />
2, no RF_CAL after deep-sleep wake up, there will only be small current.<br />
4, disable RF after deep-sleep wake up, just like modem sleep, there will be the smallest current.

####Parameters
 - `us`: number(Integer) or nil, sleep time in micro second. If us = 0, it will sleep forever. If us = nil, will not set sleep time.

 - `option`: number(Integer) or nil. If option = nil, it will use last alive setting as default option.

####Returns
`nil`

####Example

```lua
--do nothing
node.dsleep()
--sleep μs
node.dsleep(1000000)
--set sleep option, then sleep μs
node.dsleep(1000000, 4)
--set sleep option only
node.dsleep(nil,4)
```
___
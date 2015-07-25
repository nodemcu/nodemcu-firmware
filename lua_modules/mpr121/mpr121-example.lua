---------------------------------------------------------------
-- MPR121 I2C module usage example for NodeMCU
-- Based on code from: http://bildr.org/2011/05/mpr121_arduino/
-- Tiago Custódio <tiagocustodio1@gmail.com>
---------------------------------------------------------------

require("mpr121")

-- default address for the mpr121
mpr121_address = 0x5A

id = 0
sda = 3
scl = 4

i2c.setup(id, sda, scl, i2c.SLOW)

-- needs only to be called once to configure the mpr121 registers
mpr121.init(mpr121_address)

while true do -- WARNING: this is a infinite loop! don't set your init to execute this file
	touch = mpr121.readTouchInputs()
	touchString = ""
	for k, v in pairs(touch) do
		-- concatenate every value in a string
		touchString = touchString .. v
	end
	print(touchString)
	tmr.wdclr() -- clear the watchdog or the esp will reset!
end

-- don't forget to release it after use
mpr121 = nil
package.loaded["mpr121"] = nil
---
-- @description Shows control of 8 GPIO pins/LEDs via I2C with the MCP23008 I/O expander.
-- Tested on odeMCU 0.9.5 build 20150213. 
-- @date March 02, 2015
-- @circuit Connect 8 LEDs withs resistors to the GPIO pins of the MCP23008.
--  Connect GPIO0 of the ESP8266-01 module to the SCL pin of the MCP23008. 
--  Connect GPIO2 of the ESP8266-01 module to the SDA pin of the MCP23008.
--  Connect two 4.7k pull-up resistors on SDA and SCL
--  Use 3.3V for VCC.
-- @author Miguel (AllAboutEE)
--      GitHub: https://github.com/AllAboutEE 
--      Working Example Video: https://www.youtube.com/watch?v=KphAJMZZed0 
--      Website: http://AllAboutEE.com
---------------------------------------------------------------------------------------------

require ("mcp23008")

-- ESP-01 GPIO Mapping as per GPIO Table in https://github.com/nodemcu/nodemcu-firmware
gpio0, gpio2 = 3, 4

-- Setup MCP23008
mcp23008.begin(0x0,gpio2,gpio0,i2c.SLOW)

mcp23008.writeIODIR(0x00) -- make all GPIO pins as outputs
mcp23008.writeGPIO(0x00) -- make all GIPO pins off/low

---
-- @name count()
-- @description Reads the value from the GPIO register, increases the read value by 1 
--  and writes it back so the LEDs will display a binary count up to 255 or 0xFF in hex.
local function count()

    local gpio = 0x00

    gpio = mcp23008.readGPIO()
    
    if(gpio<0xff) then
        mcp23008.writeGPIO(gpio+1)
    else
        mcp23008.writeGPIO(0x00)
    end

end
-- Run count() every 100ms
tmr.alarm(0,100,1,count)

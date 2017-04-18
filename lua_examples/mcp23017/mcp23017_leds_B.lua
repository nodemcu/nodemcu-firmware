---
-- @description Shows control of 8 GPIO pins/LEDs via I2C with the MCP23017 I/O expander.
-- Tested on odeMCU 0.9.5 build 20150213. 
-- modifyed by Alberto Panu from MCP2308 module on March 11, 2016
-- Video on https://www.youtube.com/watch?v=uh4x8T8cQ-E
-- @date March 02, 2015
-- @circuit Connect 8 LEDs withs resistors to the GPIO pins of the MCP23017.
--  Connect GPIO0 of the ESP8266-01 module to the SCL pin of the MCP23017. 
--  Connect GPIO2 of the ESP8266-01 module to the SDA pin of the MCP23017.
--  Connect two 4.7k pull-up resistors on SDA and SCL
--  Use 3.3V for VCC.
-- @author Miguel (AllAboutEE)
--      GitHub: https://github.com/AllAboutEE 
--      Working Example Video: https://www.youtube.com/watch?v=KphAJMZZed0 
--      Website: http://AllAboutEE.com
---------------------------------------------------------------------------------------------

require ("mcp23017")

-- ESP-01 GPIO Mapping as per GPIO Table in https://github.com/nodemcu/nodemcu-firmware
gpio0, gpio2 = 3, 4
gpio = 0
-- Setup MCP23017
--- function M.begin(address,pinSDA,pinSCL,speed)
--- Original examlpe
--- mcp23017.begin(0x0,gpio2,gpio0,i2c.SLOW)
--- My board has gpio0 gpip2 swapped pin!
mcp23017.begin(0x0,gpio0,gpio2,i2c.SLOW)

mcp23017.writeIODIRB(0x00) -- make all GPIO pins as outputs
mcp23017.writeGPIOB(0x00) -- make all GIPO pins off/low

---
-- @name count()
-- @description Reads the value from the GPIO register, increases the read value by 1 
--  and writes it back so the LEDs will display a binary count up to 255 or 0xFF in hex.
function count()
    
    if(gpio == 0) then
	    gpio = 1
    else
        gpio = 0
    end
    mcp23017.writeGPIOB(gpio)
end
-- Run count() every second
tmr.alarm(0,1000,1,count)

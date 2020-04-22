--[[

    This example demonstrates how to use the different functions of the mcp23017 lua module.

    @author Marcel P. | Plomi.net
	@github https://github.com/plomi-net

	@version 1.0.0

]]





--[[

 initialize and setup

]]

-- initialize module
local mcp23017 = require('mcp23017')

-- set the address for MCP23017
local address = 0x20

-- SCL pin = 1 = D1 / GPIO 5 (ESP8266)
local cSCL = 1

-- SDA pin = 2 = D2 / GPIO4 (ESP8266)
local cSDA = 2

-- setup mcp23017
mcp23017.setup(address, cSCL, cSDA)


--[[

 set output and input channels

]]

-- set pin 7 and 8 to output (GPA7 and GPB0) and GPB1 to input
mcp23017.setMode(mcp23017.GPA, 7, mcp23017.OUTPUT)
mcp23017.setMode(mcp23017.GPB, 0, mcp23017.OUTPUT)
mcp23017.setMode(mcp23017.GPB, 1, mcp23017.INPUT)



--[[

 set output channels to high and low

]]

-- set pin 7 to high (GPA7)
mcp23017.setPin(mcp23017.GPA, 7, mcp23017.HIGH)
-- set pin 8 to low (GPB0)
mcp23017.setPin(mcp23017.GPB, 0, mcp23017.LOW)




--[[

 toggle pin 6 channel state every second (blinking)

]]

local currentPin = 6
local currentState = false

mcp23017.setMode(mcp23017.GPA, currentPin, mcp23017.OUTPUT)

tmr.create():alarm(1000, tmr.ALARM_AUTO, function()
    if currentState == true then
        -- print("set to low")
        mcp23017.setPin(mcp23017.GPA, currentPin, mcp23017.LOW)
        currentState = false
    else
        -- print("set to high")
        mcp23017.setPin(mcp23017.GPA, currentPin, mcp23017.HIGH)
        currentState = true
    end
end)





--[[

 read input channels and display every 7 seconds

]]

-- read input register
tmr.create():alarm(7000, tmr.ALARM_AUTO, function()
    local a = mcp23017.readGPIO(mcp23017.GPA)
    print(" ")
    print("GPIO A input states: " .. a)

    local b = mcp23017.readGPIO(mcp23017.GPB)
    print("GPIO B input states: " .. b)
    print(" ")
end)
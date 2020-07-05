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
local mcp23017 = require "mcp23017"

-- set the address for MCP23017
local address = 0x20

-- SCL pin = 1 = D1 / GPIO 5 (ESP8266)
local cSCL = 1

-- SDA pin = 2 = D2 / GPIO4 (ESP8266)
local cSDA = 2

local i2cId = 0

-- setup i2c bus and create instance for mcp23017 (assigned to mcp)
i2c.setup(i2cId, cSDA, cSCL, i2c.SLOW)
local mcp = mcp23017(address, i2cId)


--[[

 set output and input channels

]]

-- set pin 7 and 8 to output (GPA7 and GPB0) and GPB1 to input
mcp:setMode(mcp.GPA, 7, mcp.OUTPUT)
mcp:setMode(mcp.GPB, 0, mcp.OUTPUT)
mcp:setMode(mcp.GPB, 1, mcp.INPUT)



--[[

 set output channels to high and low

]]

-- set pin 7 to high (GPA7)
mcp:setPin(mcp.GPA, 7, mcp.HIGH)
-- set pin 8 to low (GPB0)
mcp:setPin(mcp.GPB, 0, mcp.LOW)




--[[

 toggle pin 6 channel state every second (blinking)

]]

local currentPin = 6
local currentState = false

mcp:setMode(mcp.GPA, currentPin, mcp.OUTPUT)

tmr.create():alarm(1000, tmr.ALARM_AUTO, function()
    if currentState == true then
        -- print("set to low")
        mcp:setPin(mcp.GPA, currentPin, mcp.LOW)
        currentState = false
    else
        -- print("set to high")
        mcp:setPin(mcp.GPA, currentPin, mcp.HIGH)
        currentState = true
    end
end)





--[[

 read input channels and display every 7 seconds

]]

-- read input register
tmr.create():alarm(7000, tmr.ALARM_AUTO, function()
    local a = mcp:readGPIO(mcp.GPA)
    print(" ")
    print("GPIO A input states: " .. a)

    local b = mcp:readGPIO(mcp.GPB)
    print("GPIO B input states: " .. b)
    print(" ")
end)
